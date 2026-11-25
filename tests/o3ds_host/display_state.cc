#include <initializer_list>
#include <cassert>
#include <cstdio>
#include "platform/ctr/ctr_rectmap.h"
#include "platform/ctr/ctr_input.h"
namespace fallout {
ctr_rectMap_t ctr_rectMap{};
ctr_input_t ctr_input{};
void ctr_input_center_mouse() {}
}
using namespace fallout;
int main() {
    ctr_rectmap_init();
    restoreGameplayRectMap(DISPLAY_FULL);
    assert(ctr_rectMap.active == DISPLAY_GUI);
    ctr_input.mode = DISPLAY_MODE_FULL_BOT;
    setActiveRectMap(DISPLAY_LAST);
    assert(ctr_rectMap.main == DISPLAY_GUI && ctr_rectMap.active == DISPLAY_FULL);
    setPreviousRectMap(0);
    assert(getPreviousRectMap(0) == DISPLAY_GUI);
    for (int i = DISPLAY_SPLASH; i < DISPLAY_LAST; ++i) {
        const auto map = static_cast<rectMap_e>(i);
        if (isGameplayRectMap(map)) continue;
        setActiveRectMap(map);
        assert(ctr_rectMap.active == map);
    }
    setActiveRectMap(getPreviousRectMap(0));
    assert(ctr_rectMap.active == DISPLAY_FULL);
    restoreGameplayRectMap(ctr_rectMap.main);
    assert(ctr_input.mode == DISPLAY_MODE_ADAPT && ctr_rectMap.active == DISPLAY_GUI);
    setActiveRectMap(DISPLAY_FIELD);
    setPreviousRectMap(1);
    setActiveRectMap(DISPLAY_INVENTORY);
    setActiveRectMap(getPreviousRectMap(1));
    assert(ctr_rectMap.active == DISPLAY_FIELD);
    restoreGameplayRectMap(DISPLAY_FIELD);
    assert(ctr_rectMap.active == DISPLAY_FIELD);
    for (auto map : {DISPLAY_GUI, DISPLAY_PIPBOY, DISPLAY_MAIN}) {
        setActiveRectMap(map);
        auto previous = ctr_rectMap.main;
        setActiveRectMap(DISPLAY_MOVIE);
        setActiveRectMap(previous);
        assert(ctr_rectMap.active == map);
    }
    ctr_rectmap_exit();
    puts("Display state regressions passed");
}
