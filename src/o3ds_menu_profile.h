#ifndef FALLOUT_O3DS_MENU_PROFILE_H
#define FALLOUT_O3DS_MENU_PROFILE_H

#ifdef __3DS__
#include <stdio.h>
#include "input.h"
#endif

namespace fallout {
class O3dsMenuProfile {
public:
    explicit O3dsMenuProfile(const char* name)
#ifdef __3DS__
        : name_(name)
#endif
    {
#ifdef __3DS__
        static const bool enabled = []() {
            FILE* flag = fopen("sdmc:/3ds/fallout2/o3ds-menu-profile.flag", "rb");
            if (!flag) return false;
            fclose(flag);
            return true;
        }();
        enabled_ = enabled;
        if (enabled_) start_ = previous_ = getTicks();
#else
        (void)name;
#endif
    }
    void mark(const char* stage)
    {
#ifdef __3DS__
        if (!enabled_ || count_ == 12) return;
        unsigned int now = getTicks();
        stages_[count_] = stage;
        times_[count_++] = now - previous_;
        previous_ = now;
#else
        (void)stage;
#endif
    }
    void finish()
    {
#ifdef __3DS__
        if (!enabled_) return;
        mark("remaining");
        unsigned int total = getTicks() - start_;
        FILE* file = fopen("sdmc:/3ds/fallout2/o3ds-menu-profile.csv", "a");
        if (file) {
            for (unsigned int i = 0; i < count_; ++i)
                fprintf(file, "%s,%s,%u\n", name_, stages_[i], times_[i]);
            fprintf(file, "%s,total,%u\n", name_, total);
            fclose(file);
        }
        enabled_ = false;
#endif
    }
private:
#ifdef __3DS__
    const char* name_;
    const char* stages_[12];
    unsigned int times_[12];
    unsigned int count_ = 0;
    unsigned int start_ = 0;
    unsigned int previous_ = 0;
    bool enabled_ = false;
#endif
};
} // namespace fallout
#endif
