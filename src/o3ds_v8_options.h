#ifndef FALLOUT_O3DS_V8_OPTIONS_H
#define FALLOUT_O3DS_V8_OPTIONS_H
#include "o3ds_v7_options.h"
namespace fallout {
inline bool o3dsV8FastIo() {
#if defined(__3DS__)
    static const bool enabled = []() {
        FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-v8-compat.flag", "rb");
        if (!f) return true;
        fclose(f);
        return false;
    }();
    return enabled && o3dsV7FastIo();
#elif defined(O3DS_TEST_V8)
    return true;
#else
    return false;
#endif
}
}
#endif
