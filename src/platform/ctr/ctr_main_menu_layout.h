#ifndef FALLOUT_PLATFORM_CTR_MAIN_MENU_LAYOUT_H
#define FALLOUT_PLATFORM_CTR_MAIN_MENU_LAYOUT_H

namespace fallout {
namespace ctr_menu {
// Original MAINMENU.FRM has six 192x36 metal plates on its left.
constexpr int RowCount = 6;
constexpr int RowX = 16;
constexpr int RowY = 15;
constexpr int RowWidth = 192;
constexpr int RowHeight = 36;
constexpr int RowStep = 41;
constexpr int LampX = 14;
constexpr int LampY = 4;
constexpr int LampSize = 26;
constexpr int LabelLeft = 44;
constexpr int LabelRight = 184; // exclusive

// Source artwork and bottom-screen placement. Preserve aspect ratio to
// within one pixel, with a little padding around the original plates.
constexpr int BottomSrcX = 12;
constexpr int BottomSrcY = 11;
constexpr int BottomSrcW = 200;
constexpr int BottomSrcH = 248;
constexpr int BottomDstX = 63;
constexpr int BottomDstY = 0; // renderer uses a bottom-left origin
constexpr int BottomDstW = 194;
constexpr int BottomDstH = 240;

// Fill the entire 400x240 top screen with a centered 5:3 crop of
// the helmet-only region. This preserves proportions and excludes the plates.
constexpr int TopSrcX = 209;
constexpr int TopSrcY = 111;
constexpr int TopSrcW = 430;
constexpr int TopSrcH = 258;
constexpr int TopDstX = 0;
constexpr int TopDstY = 0;
constexpr int TopDstW = 400;
constexpr int TopDstH = 240;

constexpr int rowY(int row) { return RowY + row * RowStep; }

// Match the bottom texture's inverse mapping. Outside the panel, use a
// harmless point in the original framebuffer, not the previous touch.
inline void mapTouch(int x, int y, int* outX, int* outY)
{
    constexpr int top = 240 - BottomDstY - BottomDstH;
    if (x < BottomDstX || x >= BottomDstX + BottomDstW || y < top || y >= top + BottomDstH) {
        *outX = 0;
        *outY = 0;
        return;
    }
    *outX = BottomSrcX + (x - BottomDstX) * BottomSrcW / BottomDstW;
    *outY = BottomSrcY + (y - top) * BottomSrcH / BottomDstH;
}

static_assert(RowX >= BottomSrcX && RowX + RowWidth <= BottomSrcX + BottomSrcW);
static_assert(RowY >= BottomSrcY && rowY(RowCount - 1) + RowHeight <= BottomSrcY + BottomSrcH);
static_assert(RowX + RowWidth <= TopSrcX);
static_assert(LampX + LampSize <= LabelLeft && LampY + LampSize <= RowHeight);
static_assert(LabelLeft < LabelRight && LabelRight <= RowWidth);
static_assert(TopSrcX + TopSrcW <= 640 && TopSrcY + TopSrcH <= 480);
static_assert(TopDstX == 0 && TopDstY == 0 && TopDstW == 400 && TopDstH == 240);
static_assert(TopSrcW * TopDstH == TopSrcH * TopDstW);
static_assert(BottomDstX + BottomDstW <= 320 && BottomDstY + BottomDstH <= 240);
}
}
#endif
