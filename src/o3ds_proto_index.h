#ifndef FALLOUT_O3DS_PROTO_INDEX_H
#define FALLOUT_O3DS_PROTO_INDEX_H
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "o3ds_text_reader.h"
namespace fallout {
class O3dsProtoIndex {
public:
    O3dsProtoIndex() = default;
    O3dsProtoIndex(const O3dsProtoIndex&) = delete;
    O3dsProtoIndex& operator=(const O3dsProtoIndex&) = delete;
    ~O3dsProtoIndex() { clear(); }
    void clear() {
        free(names_); free(offsets_); names_ = nullptr; offsets_ = nullptr;
        used_ = count_ = namesCapacity_ = offsetsCapacity_ = 0; attempted_ = false;
    }
    bool attempted() const { return attempted_; }
    // 128 KiB names + 64 KiB offsets per type maximum. Fall back if exceeded.
    bool load(File* file) {
        clear(); attempted_ = true;
        if (!file) return false;
        O3dsTextReader reader(file);
        char line[256];
        while (reader.readLine(line, sizeof(line))) {
            trim(line);
            if (!append(line)) { clear(); attempted_ = true; return false; }
        }
        return true;
    }
    const char* lookup(unsigned int oneBased) const {
        return oneBased && oneBased <= count_ ? names_ + offsets_[oneBased - 1] : nullptr;
    }
    static void trim(char* line) {
        char* end = strchr(line, ' '); if (end) *end = '\0';
        end = strpbrk(line, "\r\n"); if (end) *end = '\0';
    }
private:
    bool append(const char* line) {
        size_t n = strlen(line) + 1;
        if (used_ + n > 128 * 1024 || count_ >= 16384) return false;
        if (used_ + n > namesCapacity_) {
            size_t cap = namesCapacity_ ? namesCapacity_ * 2 : 1024;
            while (cap < used_ + n) cap *= 2;
            char* q = static_cast<char*>(realloc(names_, cap));
            if (!q) return false;
            names_ = q; namesCapacity_ = cap;
        }
        if (count_ == offsetsCapacity_) {
            size_t cap = offsetsCapacity_ ? offsetsCapacity_ * 2 : 128;
            uint32_t* q = static_cast<uint32_t*>(realloc(offsets_, cap * sizeof(uint32_t)));
            if (!q) return false;
            offsets_ = q; offsetsCapacity_ = cap;
        }
        offsets_[count_++] = used_;
        memcpy(names_ + used_, line, n); used_ += n;
        return true;
    }
    char* names_ = nullptr;
    uint32_t* offsets_ = nullptr;
    size_t used_ = 0, count_ = 0, namesCapacity_ = 0, offsetsCapacity_ = 0;
    bool attempted_ = false;
};
}
#endif
