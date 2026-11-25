#ifndef FALLOUT_O3DS_GAMEPLAY_PROFILE_H
#define FALLOUT_O3DS_GAMEPLAY_PROFILE_H
#ifdef __3DS__
#include <stdio.h>
#include "input.h"
#endif
namespace fallout {
class O3dsGameplayProfile {
public:
    O3dsGameplayProfile() {
#ifdef __3DS__
        static const bool enabled = [](){ FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-gameplay-profile.flag", "rb"); if (!f) return false; fclose(f); return true; }();
        enabled_ = enabled;
#endif
    }
    ~O3dsGameplayProfile() { flush(); }
    void begin() {
#ifdef __3DS__
        if (enabled_) { last_ = getTicks(); if (!frames_) start_ = last_; }
#endif
    }
    void mark(int stage) {
#ifdef __3DS__
        if (enabled_) { unsigned now = getTicks(); times_[stage] += now-last_; last_ = now; }
#else
        (void)stage;
#endif
    }
    void end() {
#ifdef __3DS__
        if (enabled_) { ++frames_; if (getTicks()-start_ >= 10000) flush(); }
#endif
    }
private:
    void flush() {
#ifdef __3DS__
        if (!enabled_ || !frames_) return;
        FILE* f = fopen("sdmc:/3ds/fallout2/o3ds-gameplay-profile.csv", "a");
        if (f) { fprintf(f, "%u,%u,%u,%u,%u,%u\n", getTicks()-start_, frames_, times_[0], times_[1], times_[2], times_[3]); fclose(f); }
        frames_=0; for (auto& time : times_) time=0;
#endif
    }
#ifdef __3DS__
    bool enabled_=false;
    unsigned start_=0, last_=0, frames_=0, times_[4]={};
#endif
};
}
#endif
