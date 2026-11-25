// NOTE: For unknown reason functions in this module use __stdcall instead
// of regular __usercall.

#include "file_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#endif
#include "o3ds_v8_options.h"
#include <string.h>
#include <zlib.h>

#include "platform_compat.h"

namespace fallout {

static int fileCopy(const char* existingFilePath, const char* newFilePath);

// Allocate large buffers on the regular heap, not the small O3DS stack.
// Fall back to a small local buffer under memory pressure.
class CopyBuffer {
public:
    explicit CopyBuffer(bool small = false) : large_(nullptr), capacity_(0) {
        if (small) return;
        if (o3dsV8FastIo()) {
            large_ = static_cast<unsigned char*>(malloc(256 * 1024));
            if (large_) capacity_ = 256 * 1024;
        }
        if (!large_ && o3dsV7FastIo()) {
            large_ = static_cast<unsigned char*>(malloc(64 * 1024));
            if (large_) capacity_ = 64 * 1024;
        }
    }
    ~CopyBuffer() { free(large_); }
    unsigned char* data() { return large_ ? large_ : small_; }
    size_t size() const { return large_ ? capacity_ : sizeof(small_); }
private:
    unsigned char* large_;
    size_t capacity_;
    unsigned char small_[4096];
};

static int restoreFileFast(const char* source, const char* destination, unsigned char* snapshot = nullptr, unsigned int capacity = 0, unsigned int* snapshotSize = nullptr)
{
    if (snapshotSize) *snapshotSize = 0;
    unsigned int captured = 0;
    // zlib's transparent read mode also copies ordinary uncompressed files.
    gzFile in = compat_gzopen(source, "rb");
    if (!in) return -1;
    gzbuffer(in, snapshot ? 4096 : 32 * 1024);
    FILE* out = compat_fopen(destination, "wb");
    if (!out) { gzclose(in); return -1; }
    setvbuf(out, nullptr, _IOFBF, snapshot ? 4096 : 64 * 1024);
    CopyBuffer buffer(snapshot != nullptr);
    bool ok = true;
    int n;
    while ((n = gzread(in, buffer.data(), buffer.size())) > 0) {
        if (fwrite(buffer.data(), 1, n, out) != static_cast<size_t>(n)) { ok = false; break; }
        if (snapshot) {
            if (static_cast<unsigned int>(n) <= capacity - captured) {
                memcpy(snapshot + captured, buffer.data(), n);
                captured += n;
            } else {
                snapshot = nullptr;
            }
        }
    }
    int zerr;
    gzerror(in, &zerr);
    if (n < 0 || (zerr != Z_OK && zerr != Z_STREAM_END)) ok = false;
    if (gzclose(in) != Z_OK) ok = false;
    if (fclose(out) != 0) ok = false;
    if (ok && snapshot && snapshotSize) *snapshotSize = captured;
    return ok ? 0 : -1;
}

int fileCopyDecompressedSnapshot(const char* source, const char* destination, unsigned char* snapshot, unsigned int capacity, unsigned int* size)
{
    if (size) *size = 0;
    if (o3dsV8FastIo()) return restoreFileFast(source, destination, snapshot, capacity, size);
    return fileCopyDecompressed(source, destination);
}

int fileReadSmallDecompressed(const char* source, unsigned char* bytes, unsigned int capacity, unsigned int* size)
{
    if (!source || !bytes || !size || capacity == 0 || capacity > 4096) return -1;
    *size = 0;
    gzFile in = compat_gzopen(source, "rb");
    if (!in) return -1;
    gzbuffer(in, 1024);
    unsigned int used = 0;
    int rc = 0;
    while (used < capacity) {
        int n = gzread(in, bytes + used, capacity - used);
        if (n < 0) { rc = -1; break; }
        if (n == 0) break;
        used += static_cast<unsigned int>(n);
    }
    if (rc == 0 && used == capacity) {
        unsigned char extra;
        int n = gzread(in, &extra, 1); // validate the end/CRC even at exact capacity
        if (n < 0) rc = -1;
        else if (n > 0) rc = 1;
    }
    int zerr;
    gzerror(in, &zerr);
    if (zerr != Z_OK && zerr != Z_STREAM_END) rc = -1;
    if (gzclose(in) != Z_OK) rc = -1;
    if (rc == 0) *size = used;
    return rc;
}

bool fileFindStockPatches(bool* present)
{
#ifndef _WIN32
    if (!present) return false;
    memset(present, 0, 1000 * sizeof(*present));
    DIR* dir = opendir(".");
    if (!dir) return false;
    bool ok = true;
    for (;;) {
        errno = 0;
        dirent* entry = readdir(dir);
        if (!entry) { if (errno) ok = false; break; }
        const char* n = entry->d_name;
        if (strlen(n) != 12 || compat_strnicmp(n, "patch", 5) || compat_stricmp(n + 8, ".dat")) continue;
        if (n[5] < '0' || n[5] > '9' || n[6] < '0' || n[6] > '9' || n[7] < '0' || n[7] > '9') continue;
        present[(n[5]-'0')*100 + (n[6]-'0')*10 + n[7]-'0'] = true;
    }
    if (closedir(dir) != 0) ok = false;
    return ok;
#else
    (void)present;
    return false;
#endif
}

// 0x452740
int fileCopyDecompressed(const char* existingFilePath, const char* newFilePath)
{
    if (o3dsV7FastIo()) return restoreFileFast(existingFilePath, newFilePath);
    FILE* probe = compat_fopen(existingFilePath, "rb");
    if (probe == nullptr) return -1;
    int magic1 = fgetc(probe);
    int magic2 = fgetc(probe);
    bool probeOk = !ferror(probe);
    fclose(probe);
    if (!probeOk) return -1;
    if (magic1 != 0x1F || magic2 != 0x8B) {
        return fileCopy(existingFilePath, newFilePath);
    }
    gzFile in = compat_gzopen(existingFilePath, "rb");
    if (in == nullptr) return -1;
    FILE* out = compat_fopen(newFilePath, "wb");
    if (out == nullptr) { gzclose(in); return -1; }
    unsigned char buffer[4096];
    bool ok = true;
    int n;
    while ((n = gzread(in, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, n, out) != static_cast<size_t>(n)) { ok = false; break; }
    }
    int zerr;
    gzerror(in, &zerr);
    if (n < 0 || (zerr != Z_OK && zerr != Z_STREAM_END)) ok = false;
    if (gzclose(in) != Z_OK) ok = false;
    if (fclose(out) != 0) ok = false;
    return ok ? 0 : -1;
}

// 0x452804
int fileCopyCompressed(const char* existingFilePath, const char* newFilePath)
{
    FILE* in = compat_fopen(existingFilePath, "rb");
    if (in == nullptr) return -1;
    int magic1 = fgetc(in);
    int magic2 = fgetc(in);
    if (ferror(in) || fseek(in, 0, SEEK_SET) != 0) { fclose(in); return -1; }
    if (magic1 == 0x1F && magic2 == 0x8B) {
        fclose(in);
        return fileCopy(existingFilePath, newFilePath);
    }
    gzFile out = compat_gzopen(newFilePath, o3dsV7FastIo() ? "wb1" : "wb");
    if (out == nullptr) { fclose(in); return -1; }
    if (o3dsV7FastIo()) gzbuffer(out, 32 * 1024);
    CopyBuffer buffer;
    bool ok = true;
    size_t n;
    while ((n = fread(buffer.data(), 1, buffer.size(), in)) > 0) {
        if (gzwrite(out, buffer.data(), n) != static_cast<int>(n)) { ok = false; break; }
    }
    if (ferror(in)) ok = false;
    if (fclose(in) != 0) ok = false;
    if (gzclose(out) != Z_OK) ok = false;
    return ok ? 0 : -1;
}

// TODO: Check, implementation looks odd.
int _gzdecompress_file(const char* existingFilePath, const char* newFilePath)
{
    FILE* stream = compat_fopen(existingFilePath, "rb");
    if (stream == nullptr) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    fclose(stream);

    // TODO: Is it broken?
    if (magic[0] != 0x1F || magic[1] != 0x8B) {
        gzFile gzstream = compat_gzopen(existingFilePath, "rb");
        if (gzstream == nullptr) {
            return -1;
        }

        stream = compat_fopen(newFilePath, "wb");
        if (stream == nullptr) {
            gzclose(gzstream);
            return -1;
        }

        while (1) {
            int ch = gzgetc(gzstream);
            if (ch == -1) {
                break;
            }

            fputc(ch, stream);
        }

        gzclose(gzstream);
        fclose(stream);
    } else {
        fileCopy(existingFilePath, newFilePath);
    }

    return 0;
}

static int fileCopy(const char* existingFilePath, const char* newFilePath)
{
    FILE* in = compat_fopen(existingFilePath, "rb");
    if (in == nullptr) return -1;
    FILE* out = compat_fopen(newFilePath, "wb");
    if (out == nullptr) { fclose(in); return -1; }
    if (o3dsV7FastIo()) setvbuf(out, nullptr, _IOFBF, 64 * 1024);
    CopyBuffer buffer;
    bool ok = true;
    size_t n;
    while ((n = fread(buffer.data(), 1, buffer.size(), in)) > 0) {
        if (fwrite(buffer.data(), 1, n, out) != n) { ok = false; break; }
    }
    if (ferror(in)) ok = false;
    if (fclose(in) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    return ok ? 0 : -1;
}

// Save cleanup must never enumerate the read-only game DATs. Their entries
// cannot be removed and attempting it on SD incurs filesystem service calls.
int fileEraseLocalExtension(const char* directory, const char* extension)
{
#ifdef _WIN32
    (void)directory; (void)extension;
    return -2;
#else
    char dirPath[COMPAT_MAX_PATH];
    if (strlen(directory) >= sizeof(dirPath)) return -2;
    strcpy(dirPath, directory);
    compat_windows_path_to_native(dirPath);
    compat_resolve_path(dirPath);
    DIR* dir = opendir(dirPath);
    if (!dir) return errno == ENOENT ? 0 : -2;
    char** names = nullptr;
    size_t count = 0, capacity = 0;
    int rc = 0;
    for (;;) {
        errno = 0;
        struct dirent* entry = readdir(dir);
        if (!entry) { if (errno) rc = -2; break; }
        if (entry->d_type == DT_DIR) continue;
        const char* dot = strrchr(entry->d_name, '.');
        if (!dot || compat_stricmp(dot + 1, extension) != 0) continue;
        if (strlen(dirPath) + strlen(entry->d_name) + 2 > COMPAT_MAX_PATH) { rc = -2; break; }
        if (count == capacity) {
            if (capacity >= 16384) { rc = -2; break; }
            size_t next = capacity ? capacity * 2 : 32;
            char** ptr = static_cast<char**>(realloc(names, next * sizeof(*names)));
            if (!ptr) { rc = -2; break; }
            names = ptr; capacity = next;
        }
        size_t n = strlen(entry->d_name) + 1;
        char* copy = static_cast<char*>(malloc(n));
        if (!copy) { rc = -2; break; }
        memcpy(copy, entry->d_name, n); names[count++] = copy;
    }
    if (closedir(dir) != 0) rc = -2;
    if (rc == 0) {
        for (size_t i = 0; i < count; ++i) {
            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "%s%s%s", dirPath,
                dirPath[0] && dirPath[strlen(dirPath)-1] == '/' ? "" : "/", names[i]);
            // unlink never deletes directories; ignore files already removed.
            if (unlink(path) != 0 && errno != ENOENT) rc = -1;
        }
    }
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
    return rc;
#endif
}

} // namespace fallout
