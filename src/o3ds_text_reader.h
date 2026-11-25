#ifndef FALLOUT_O3DS_TEXT_READER_H
#define FALLOUT_O3DS_TEXT_READER_H

#include "db.h"

namespace fallout {

// Sequential text reader for DAT entries. The legacy DAT character reader
// invokes inflate for each byte. Read blocks instead; preserve its CRLF
// handling, and delegate non-DAT files to their original readers. Do not mix
// with external reads/seeks on the same stream while this reader is active.
class O3dsTextReader {
public:
    explicit O3dsTextReader(File* stream) : stream_(stream) {}

    int readChar()
    {
        if (stream_->type != XFILE_TYPE_DFILE) return fileReadChar(stream_);
        int ch = readRaw();
        if (ch == '\r') {
            int next = readRaw();
            if (next == '\n') return next;
            if (next != -1) --position_;
        }
        return ch;
    }

    char* readLine(char* dest, size_t size)
    {
        if (size < 2) return nullptr;
        if (stream_->type != XFILE_TYPE_DFILE) return fileReadString(dest, size, stream_);
        size_t n = 0;
        while (n < size - 1) {
            int ch = readChar();
            if (ch == -1) break;
            dest[n++] = static_cast<char>(ch);
            if (ch == '\n') break;
        }
        if (n == 0) return nullptr;
        dest[n] = '\0';
        return dest;
    }

private:
    int readRaw()
    {
        if (position_ == length_) {
            if (finished_) return -1;
            length_ = fileRead(buffer_, 1, sizeof(buffer_), stream_);
            position_ = 0;
            if (length_ == 0) {
                finished_ = true;
                return -1;
            }
        }
        return buffer_[position_++];
    }

    File* stream_;
    unsigned char buffer_[2048];
    size_t position_ = 0;
    size_t length_ = 0;
    bool finished_ = false;
};

} // namespace fallout
#endif
