#ifndef FALLOUT_CTR_ORIGINAL_MENU_H
#define FALLOUT_CTR_ORIGINAL_MENU_H

#include "xfile.h"

namespace fallout {
namespace ctr_menu {
// FRM header: 62 bytes, then the 12-byte first frame header. Read only
// the stock, single 640x480 indexed frame into the existing menu buffer.
inline unsigned int menuBe16(const unsigned char* p) { return (static_cast<unsigned int>(p[0]) << 8) | p[1]; }
inline unsigned int menuBe32(const unsigned char* p) { return (menuBe16(p) << 16) | menuBe16(p + 2); }
inline bool loadOriginalBackground(const char* archivePath, unsigned char* dest, size_t capacity)
{
    constexpr size_t pixels = 640 * 480;
    unsigned char header[74];
    if (!dest || capacity < pixels) return false;
    DFile* stream = xfileOpenArchiveEntry(archivePath, "art\\intrface\\mainmenu.frm");
    if (!stream) return false;
    bool ok = dfileGetSize(stream) >= static_cast<long>(sizeof(header) + pixels)
        && dfileRead(header, 1, sizeof(header), stream) == sizeof(header)
        && menuBe32(header) == 4 && menuBe16(header + 8) == 1
        && menuBe32(header + 34) == 0
        && menuBe32(header + 58) >= pixels + 12
        && menuBe16(header + 62) == 640 && menuBe16(header + 64) == 480
        && menuBe32(header + 66) == pixels
        && dfileRead(dest, 1, pixels, stream) == pixels;
    dfileClose(stream);
    return ok;
}
}
}
#endif
