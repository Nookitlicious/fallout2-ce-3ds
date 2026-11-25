#ifndef FALLOUT_CTR_SCENE_GATE_H
#define FALLOUT_CTR_SCENE_GATE_H
#include <stdint.h>
#include <string.h>
#include "ctr_rectmap.h"
namespace fallout {
struct SceneSnapshot {
    int display;
    int mode;
    int indicators;
    bool wide;
    rectMap_t rects[8]; // field, 3 GUI rectangles, 4 indicators
};

inline bool sameScene(const SceneSnapshot& a, const SceneSnapshot& b)
{
    return a.display == b.display && a.mode == b.mode && a.indicators == b.indicators
        && a.wide == b.wide && memcmp(a.rects, b.rects, sizeof(a.rects)) == 0;
}
inline bool shouldRedrawScene(bool track, bool valid, int changedRows,
    const SceneSnapshot& previous, const SceneSnapshot& current, uint32_t elapsed) {
    return !track || !valid || changedRows != 0 || !sameScene(previous, current) || elapsed >= 50;
}
}
#endif
