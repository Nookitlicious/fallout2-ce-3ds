#ifndef FALLOUT_PLATFORM_CTR_RENDER_ROWS_H_
#define FALLOUT_PLATFORM_CTR_RENDER_ROWS_H_

#include <stdint.h>
#include <string.h>

namespace fallout {

// CPU staging texture bookkeeping. No allocation, GPU calls, or SDL dependency.
// All indexed framebuffer writes go through svga.cc on the game thread.
class CtrRenderRows {
public:
    static constexpr int kMaxRows = 512;

    void invalidate()
    {
        memset(dirty_, 1, sizeof(dirty_));
        memset(usageValid_, 0, sizeof(usageValid_));
        paletteValid_ = false;
        width_ = height_ = 0;
    }

    void copyRect(const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch,
        int width, int height, int destY, bool forceCopy = false)
    {
        if (width <= 0 || height <= 0) return;
        for (int y = 0; y < height; ++y) {
            const int row = destY + y;
            const uint8_t* rowSrc = src + y * srcPitch;
            uint8_t* rowDst = dst + y * dstPitch;
            // An already dirty row needs no additional comparison.
            // Preserve the original memcpy semantics for valid blits.
            if (row < 0 || row >= kMaxRows) {
                memcpy(rowDst, rowSrc, width);
            } else if (forceCopy || dirty_[row] || memcmp(rowDst, rowSrc, width) != 0) {
                memcpy(rowDst, rowSrc, width);
                dirty_[row] = 1;
                usageValid_[row] = 0;
            }
        }
    }

    // Palette entries are stored as BGR (GPU_RGB8 byte order), not RGB565.
    // Returns number of rows converted, zero means the old texture is valid.
    // A sparse palette change is tested against the indexed pixels of clean
    // rows, preserving animated lights, water, fades, and movie palettes.
    template<typename Color>
    int update(const uint8_t* src, int srcPitch, int width, int height,
        const Color* palette, uint8_t* dst, int dstPitch, bool forceFull = false, bool fastPalette = true)
    {
        if (!src || !dst || !palette || width <= 0 || height <= 0
            || height > kMaxRows || srcPitch < width || width > dstPitch / 3) return 0;
        uint8_t changed[256] = {};
        uint32_t changedMask[8] = {};
        int changedCount = 0;
        bool all = forceFull || !paletteValid_ || width != width_ || height != height_;
        if (forceFull || !paletteValid_ || width != width_ || height != height_) memset(usageValid_, 0, sizeof(usageValid_));
        for (int i = 0; i < 256; ++i) {
            const Color& c = palette[i];
            uint32_t bgr = uint32_t(c.b) | (uint32_t(c.g) << 8) | (uint32_t(c.r) << 16);
            if (bgr != palette_[i]) {
                changed[i] = 1;
                changedMask[i >> 5] |= uint32_t(1) << (i & 31);
                ++changedCount;
            }
            palette_[i] = bgr;
        }
        paletteValid_ = true;
        width_ = width;
        height_ = height;
        // Scanning for widely changed palette indices is usually slower than
        // simply converting the image (e.g. during palette fades).
        if (changedCount > 32) all = true;

        int converted = 0;
        for (int y = 0; y < height; ++y) {
            const uint8_t* rowSrc = src + y * srcPitch;
            bool needsConversion = all || dirty_[y];
            if (!needsConversion && changedCount != 0) {
                if (fastPalette) {
                    if (!usageValid_[y]) {
                        memset(usage_[y], 0, sizeof(usage_[y]));
                        for (int x = 0; x < width; ++x) {
                            unsigned c = rowSrc[x];
                            usage_[y][c >> 5] |= uint32_t(1) << (c & 31);
                        }
                        usageValid_[y] = 1;
                    }
                    for (int i = 0; i < 8; ++i) {
                        if (usage_[y][i] & changedMask[i]) { needsConversion = true; break; }
                    }
                } else {
                    for (int x = 0; x < width; ++x) {
                        if (changed[rowSrc[x]]) { needsConversion = true; break; }
                    }
                }
            }
            if (!needsConversion) continue;
            uint8_t* rowDst = dst + y * dstPitch;
            for (int x = 0; x < width; ++x) {
                const Color& c = palette[rowSrc[x]];
                rowDst[x * 3 + 0] = c.b;
                rowDst[x * 3 + 1] = c.g;
                rowDst[x * 3 + 2] = c.r;
            }
            dirty_[y] = 0;
            ++converted;
        }
        return converted;
    }

private:
    uint8_t dirty_[kMaxRows] = {};
    uint32_t palette_[256] = {};
    uint32_t usage_[kMaxRows][8] = {};
    uint8_t usageValid_[kMaxRows] = {};
    bool paletteValid_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace fallout
#endif
