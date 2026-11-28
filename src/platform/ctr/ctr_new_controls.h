#ifndef FALLOUT_CTR_NEW_CONTROLS_H_
#define FALLOUT_CTR_NEW_CONTROLS_H_

#include <algorithm>

namespace fallout {
namespace ctr_new_controls {

// No hardware dependencies: these routing rules are also exercised by host tests.
inline bool shoulderZoom(bool newModel, bool enabled, bool adaptedGameplay)
{
    return newModel && enabled && adaptedGameplay;
}

inline bool stickPans(bool newModel, bool enabled, bool mapContext)
{
    return newModel && enabled && mapContext;
}

struct Directions {
    bool up, down, left, right;
};

inline Directions panDirections(bool useStick, bool shortcutHeld, int x, int y)
{
    constexpr int deadZone = 40;
    if (!useStick || shortcutHeld) return {false, false, false, false};
    return {y >= deadZone, y <= -deadZone, x <= -deadZone, x >= deadZone};
}

// Physical D-pad wins on each axis; two inputs cannot fight or double-scroll.
inline Directions combine(Directions pad, Directions stick)
{
    return {pad.up || (!pad.down && stick.up),
        pad.down || (!pad.up && stick.down),
        pad.left || (!pad.right && stick.left),
        pad.right || (!pad.left && stick.right)};
}

// Resolve each axis once. Camera offsets are viewport coordinates; residual
// movement belongs to the rate-limited isometric map scroller.
struct PanAxis {
    int offset;
    int cursorShift;
    int scroll;
};

inline int axisDirection(bool negative, bool positive)
{
    return static_cast<int>(positive) - static_cast<int>(negative);
}

inline PanAxis panAxis(int offset, int maxOffset, int direction)
{
    const int next = std::clamp(offset + 20 * direction, 0, std::max(0, maxOffset));
    return {next, next - offset, next == offset ? direction : 0};
}

inline int cursorDelta(int position, int stickDelta, int cameraShift, int low, int high)
{
    return std::clamp(position + stickDelta + cameraShift, low, std::max(low, high)) - position;
}

inline bool any(Directions d)
{
    return d.up || d.down || d.left || d.right;
}

inline unsigned panMask(Directions d)
{
    return (d.up ? 1u : 0u) | (d.down ? 2u : 0u) | (d.left ? 4u : 0u) | (d.right ? 8u : 0u);
}

inline float zoomStep(float current, bool zoomOut, bool zoomIn)
{
    if (zoomOut == zoomIn) return current;
    return std::clamp(current + (zoomIn ? 0.2f : -0.2f), 0.0f, 1.0f);
}

} // namespace ctr_new_controls
} // namespace fallout
#endif
