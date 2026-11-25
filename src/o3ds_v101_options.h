#ifndef FALLOUT_O3DS_V101_OPTIONS_H
#define FALLOUT_O3DS_V101_OPTIONS_H
#include "o3ds_v8_options.h"
namespace fallout {
inline bool o3dsV101DeferredProtos() {
#if defined(__3DS__)
    static const bool enabled = []() {
        FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-v101-compat.flag", "rb");
        if (!f) return true;
        fclose(f);
        return false;
    }();
    return enabled && o3dsV8FastIo();
#elif defined(O3DS_TEST_V101)
    return o3dsV8FastIo();
#else
    return false;
#endif
}
}
#endif
