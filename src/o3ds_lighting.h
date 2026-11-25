#ifndef FALLOUT_O3DS_LIGHTING_H
#define FALLOUT_O3DS_LIGHTING_H
#include <stdint.h>
#include <string.h>
namespace fallout {
// Small lazy cache: one contiguous palette for each of 16 recent intensities.
// No approximation: matches the old fixed-light transparent blitter exactly.
class O3dsLightRows {
public:
    void invalidate() { memset(valid_, 0, sizeof(valid_)); }
    const uint8_t* lookup(const uint8_t (&table)[256][256], int intensity) {
        if (intensity < 0 || intensity > 255) return nullptr;
        const int slot = intensity & 15;
        if (!valid_[slot] || keys_[slot] != intensity) {
            for (int c = 0; c < 229; ++c) rows_[slot][c] = table[c][intensity];
            for (int c = 229; c < 256; ++c) rows_[slot][c] = static_cast<uint8_t>(c);
            keys_[slot] = intensity;
            valid_[slot] = true;
        }
        return rows_[slot];
    }
private:
    uint8_t rows_[16][256] = {};
    int keys_[16] = {};
    bool valid_[16] = {};
};
inline void o3dsLightBlit(const uint8_t* src, int w, int h, int sp,
    uint8_t* dst, int dp, const uint8_t* shade) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            unsigned c = src[x];
            if (c) dst[x] = shade[c];
        }
        src += sp; dst += dp;
    }
}
// Clip a candidate tile rectangle to the square grid, not the hex grid.
inline bool o3dsClipSquareBounds(int& x0, int& y0, int& x1, int& y1, int w, int h) {
    if (w <= 0 || h <= 0 || x0 >= w || y0 >= h || x1 < 0 || y1 < 0) return false;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (y1 >= h) y1 = h - 1;
    return x0 <= x1 && y0 <= y1;
}
}
#endif
