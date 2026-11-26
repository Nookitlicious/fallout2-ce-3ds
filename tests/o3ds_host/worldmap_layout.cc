#include <initializer_list>
#include <cassert>
#include <cstdio>
#include <cmath>
#include "platform/ctr/ctr_rectmap.h"
#include "platform/ctr/ctr_input.h"

namespace fallout {
ctr_rectMap_t ctr_rectMap{};
ctr_input_t ctr_input{};
void ctr_input_center_mouse() {}
}
using namespace fallout;

static void checkTouch(int x, int y, int ox, int oy) {
    int sx = -1, sy = -1;
    worldMapTouchToTexture(x, y, &sx, &sy);
    assert(sx == ox && sy == oy);
}

int main() {
    ctr_rectmap_init();
    for (int origin : {0, 40}) {
        setWorldMapOrigin(origin, origin);
        const auto* top = rectMaps[DISPLAY_WORLDMAP_TOP][0];
        assert(top->src_x == origin + 22 && top->src_y == origin + 21);
        assert(top->src_w == 450 && top->src_h == 443);
        assert(top->dst_x >= 0 && top->dst_x + top->dst_w <= 400);
        assert(top->dst_y >= 0 && top->dst_y + top->dst_h <= 240);
        assert(std::abs(top->dst_w / top->dst_h - top->src_w / top->src_h) < 0.005);
        // Exhaust every touchscreen pixel: exact inverse of visible rectangles,
        // no out-of-bounds source samples or stale positions in blank areas.
        for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
            if (x >= 8 && x < 134) {
                checkTouch(x, y, origin + 472 + (x - 8) * 168 / 126,
                    origin + y * 320 / 240);
            } else if (x >= 144 && x < 312 && y >= 40 && y < 200) {
                checkTouch(x, y, origin + 472 + x - 144, origin + 320 + y - 40);
            } else {
                checkTouch(x, y, origin + 475, origin + 470);
            }
        }
        // Red destination buttons, both scroll arrows, and town/world switch.
        for (int i = 0; i < 7; ++i) {
            int y = 138 + 27 * i + 4;
            checkTouch(38, y * 3 / 4, origin + 512, origin + (y * 3 / 4) * 4 / 3);
        }
        checkTouch(17, 104, origin + 484, origin + 138);
        checkTouch(17, 115, origin + 484, origin + 153);
        checkTouch(195, 163, origin + 523, origin + 443);
        // SELECT moves only the map view onto the touchscreen. The existing
        // destination controls are drawn on top, with a 40-pixel translation.
        assert(!worldMapScreensSwapped());
        toggleWorldMapScreens();
        assert(worldMapScreensSwapped());
        const auto swapped = worldMapViewportRect();
        assert(swapped.dst_x == 38 && swapped.dst_w == 244);
        for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
            if (x >= 38 && x < 282) {
                checkTouch(x, y, origin + 22 + (x - 38) * 450 / 244,
                    origin + 21 + y * 443 / 240);
            } else {
                checkTouch(x, y, origin + 475, origin + 470);
            }
        }
        for (int i = 0; i < numRectsInMap[DISPLAY_WORLDMAP]; ++i) {
            const auto* controls = rectMaps[DISPLAY_WORLDMAP][i];
            assert(controls->dst_x + 40 >= 0);
            assert(controls->dst_x + 40 + controls->dst_w <= 400);
        }
        toggleWorldMapScreens();
        assert(!worldMapScreensSwapped());
        assert(worldMapViewportRect().dst_x == 78);
        checkTouch(195, 163, origin + 523, origin + 443);

    }
    toggleWorldMapScreens();
    setWorldMapOrigin(0, 0);
    assert(!worldMapScreensSwapped());
    assert(rectMaps[DISPLAY_WORLDMAP][0]->src_x == 472);
    for (auto mode : {DISPLAY_MODE_ADAPT, DISPLAY_MODE_FULL_BOT}) {
        ctr_input.mode = mode;
        setActiveRectMap(DISPLAY_GUI);
        auto before = ctr_rectMap.active;
        setActiveRectMap(DISPLAY_WORLDMAP);
        assert(ctr_rectMap.active == DISPLAY_WORLDMAP);
        toggleWorldMapScreens();
        assert(ctr_input.mode == mode);
        setActiveRectMap(DISPLAY_DYNAMIC);
        setActiveRectMap(DISPLAY_WORLDMAP);
        assert(ctr_rectMap.active == DISPLAY_WORLDMAP);
        assert(worldMapScreensSwapped());
        toggleWorldMapScreens();
        setActiveRectMap(DISPLAY_GUI);
        assert(ctr_rectMap.active == before);
    }
    ctr_rectmap_exit();
    puts("World map layout: 307200 touch pixels and display transitions passed");
}
