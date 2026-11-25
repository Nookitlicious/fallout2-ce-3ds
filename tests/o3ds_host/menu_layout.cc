#include <cassert>
#include <initializer_list>
#include <cstdio>
#include "platform/ctr/ctr_main_menu_layout.h"
using namespace fallout::ctr_menu;

int rowAt(int x, int y)
{
    if (x < RowX || x >= RowX + RowWidth) return -1;
    for (int row = 0; row < RowCount; ++row) {
        if (y >= rowY(row) && y < rowY(row) + RowHeight) return row;
    }
    return -1;
}
int main()
{
    int hit[RowCount] = {};
    int blank = 0;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            int sx = -1, sy = -1;
            mapTouch(x, y, &sx, &sy);
            assert(sx >= 0 && sx < 640 && sy >= 0 && sy < 480);
            int row = rowAt(sx, sy);
            if (x < BottomDstX || x >= BottomDstX + BottomDstW) {
                assert(row == -1 && sx == 0 && sy == 0);
            }
            if (row == -1) ++blank;
            else ++hit[row];
        }
    }
    for (int row = 0; row < RowCount; ++row) {
        assert(hit[row] >= 5900);
        int y = (rowY(row) + RowHeight / 2 - BottomSrcY) * BottomDstH / BottomSrcH;
        for (int srcX : {RowX + 2, RowX + LampX + LampSize / 2, RowX + LabelLeft + 10, RowX + RowWidth - 3}) {
            int x = BottomDstX + (srcX - BottomSrcX) * BottomDstW / BottomSrcW;
            int sx, sy;
            mapTouch(x, y, &sx, &sy);
            assert(rowAt(sx, sy) == row);
        }
        if (row != RowCount - 1) {
            // Each row is separated by a real inactive gap.
            int sy = rowY(row) + RowHeight + 2;
            int dy = (sy - BottomSrcY) * BottomDstH / BottomSrcH;
            int x;
            mapTouch(160, dy, &x, &sy);
            assert(rowAt(x, sy) == -1);
        }
    }
    assert(blank > 0);
    for (int x : {-1, 320}) {
        int sx, sy;
        mapTouch(x, 100, &sx, &sy);
        assert(sx == 0 && sy == 0);
    }
    assert(TopSrcW * TopDstH == TopSrcH * TopDstW);
    assert(TopDstX == 0 && TopDstY == 0 && TopDstW == 400 && TopDstH == 240);
    assert(TopSrcX >= RowX + RowWidth);
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 400; ++x) {
            int sx = TopSrcX + x * TopSrcW / TopDstW;
            int sy = TopSrcY + y * TopSrcH / TopDstH;
            assert(sx >= RowX + RowWidth && sx < 640 && sy >= 0 && sy < 480);
            assert(rowAt(sx, sy) == -1);
        }
    }
    puts("top layout: all 96000 pixels covered, proportional crop, no menu plates");
    printf("menu layout: all 76800 touch pixels checked; six rows hit counts:");
    for (int h : hit) printf(" %d", h);
    printf("; %d inactive pixels\n", blank);
}
