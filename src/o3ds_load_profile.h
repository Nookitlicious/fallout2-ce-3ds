#ifndef FALLOUT_O3DS_LOAD_PROFILE_H
#define FALLOUT_O3DS_LOAD_PROFILE_H
#ifdef __3DS__
#include <stdio.h>
#include "input.h"
#endif
namespace fallout {
class O3dsLoadProfile {
public:
    explicit O3dsLoadProfile(const char* name) {
#ifdef __3DS__
        name_ = name;
        static const bool enabled = []() {
            FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-load-profile.flag", "rb");
            if (!f) return false;
            fclose(f);
            return true;
        }();
        enabled_ = enabled;
        if (enabled_) start_ = last_ = getTicks();
#else
        (void)name;
#endif
    }
    ~O3dsLoadProfile() { finish("incomplete"); }
    void mark(const char* stage) {
#ifdef __3DS__
        if (!enabled_ || count_ >= 40) return;
        unsigned int now = getTicks();
        labels_[count_] = stage;
        times_[count_++] = now - last_;
        last_ = now;
#else
        (void)stage;
#endif
    }
    void finish(const char* status = "ok") {
#ifdef __3DS__
        if (!enabled_) return;
        mark("remaining");
        unsigned int total = getTicks() - start_;
        enabled_ = false;
        FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-load-profile.csv", "a");
        if (!f) return;
        for (unsigned int i = 0; i < count_; ++i)
            fprintf(f, "%s,%s,%u\n", name_, labels_[i], times_[i]);
        fprintf(f, "%s,total_%s,%u\n", name_, status, total);
        fclose(f);
#else
        (void)status;
#endif
    }
private:
#ifdef __3DS__
    const char* name_ = nullptr;
    const char* labels_[40];
    unsigned int times_[40];
    unsigned int count_ = 0, start_ = 0, last_ = 0;
    bool enabled_ = false;
#endif
};
} // namespace fallout
#endif
