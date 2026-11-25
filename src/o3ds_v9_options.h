#ifndef FALLOUT_O3DS_V9_OPTIONS_H
#define FALLOUT_O3DS_V9_OPTIONS_H
#include <stdio.h>
namespace fallout {
inline bool o3dsV9FastRender() {
#if defined(__3DS__)
    static const bool enabled = []() {
        FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-v9-compat.flag", "rb");
        if (!f) return true;
        fclose(f); return false;
    }();
    return enabled;
#elif defined(O3DS_TEST_V9)
    return true;
#else
    return false;
#endif
}
}
#endif
