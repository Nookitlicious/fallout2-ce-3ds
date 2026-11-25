#include "platform/ctr/ctr_render_rows.h"
#include "platform/ctr/ctr_scene_gate.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

struct Color { uint8_t r, g, b, a; };
using fallout::CtrRenderRows;

static void compare(const std::vector<uint8_t>& src, int sp, int w, int h,
    const Color* p, const std::vector<uint8_t>& dst, int dp)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Color& c = p[src[y*sp+x]];
            assert(dst[y*dp+x*3] == c.b);
            assert(dst[y*dp+x*3+1] == c.g);
            assert(dst[y*dp+x*3+2] == c.r);
        }
        for (int x = w*3; x < dp; ++x) assert(dst[y*dp+x] == 0xcd);
    }
}

int main()
{
    constexpr int w = 640, h = 480, sp = 648, dp = 3072;
    std::vector<uint8_t> src(sp*h, 1), dst(dp*h, 0xcd), patch(20*12, 2);
    Color p[256];
    for (int i = 0; i < 256; ++i) p[i] = Color{uint8_t(i), uint8_t(i*7), uint8_t(i*11), 255};
    CtrRenderRows rows;
    auto update = [&](bool full = false) { return rows.update(src.data(),sp,w,h,p,dst.data(),dp,full); };
    auto check = [&](){ compare(src,sp,w,h,p,dst,dp); };
    assert(update() == h); check();
    assert(update() == 0); check();
    rows.copyRect(patch.data(),20,src.data()+sp*15+5,sp,20,12,15);
    assert(update() == 12); check();
    rows.copyRect(patch.data(),20,src.data()+sp*15+5,sp,20,12,15);
    assert(update() == 0); check();
    p[2].g ^= 255;
    assert(update() == 12); check();
    p[255].r ^= 255;
    assert(update() == 0); check();
    p[1].a = 0; // alpha not used in BGR8
    assert(update() == 0); check();
    for (int i = 0; i < 45; ++i) p[i].b ^= 255;
    assert(update() == h); check();
    std::fill(src.begin(),src.end(),0); // bulk clear, same dimensions
    rows.invalidate();
    assert(update() == h); check();
    assert(update(true) == h); check();
    rows.copyRect(patch.data(),20,src.data()+sp*15+5,sp,20,12,15,true);
    assert(update(true) == h); check();
    // Mixed partial writes, palette changes, and full refreshes.
    std::mt19937 rng(42);
    for (int n = 0; n < 160; ++n) {
        int rw = 1 + rng()%40, rh = 1 + rng()%30;
        int x = rng()%(w-rw+1), y = rng()%(h-rh+1);
        std::vector<uint8_t> region((rw+3)*rh);
        for(auto& b : region) b = rng()%256;
        rows.copyRect(region.data(),rw+3,src.data()+y*sp+x,sp,rw,rh,y);
        for(int i=0;i<n%10;++i) { auto& c=p[rng()%256]; c.r ^= 0x5a; }
        update(n%19==0); check();
        assert(update()==0);
    }
    // Surface resize (even when the palette is identical) and invalidation.
    std::vector<uint8_t> small(17*13, 2), out(32*3*13, 0xcd);
    assert(rows.update(small.data(),17,17,13,p,out.data(),96)==13);
    compare(small,17,17,13,p,out,96);
    assert(rows.update(small.data(),17,17,13,p,out.data(),96)==0);
    assert(update()==h); check();
    // Build usage masks, then replace every indexed pixel through a forced reset.
    std::fill(src.begin(),src.end(),3); rows.invalidate(); update();
    p[3].r ^= 1; assert(update()==h); check();
    std::fill(src.begin(),src.end(),4); assert(update(true)==h); check();
    p[4].g ^= 1; assert(update()==h); check();
    p[3].r ^= 1; assert(update()==0); check();
    p[4].g ^= 1;
    assert(rows.update(src.data(),sp,w,h,p,dst.data(),dp,false,false)==h); check();
    fallout::SceneSnapshot a{}, b{};
    using fallout::shouldRedrawScene;
    assert(!shouldRedrawScene(true,true,0,a,b,49));
    assert(shouldRedrawScene(true,true,0,a,b,50));
    assert(shouldRedrawScene(true,true,1,a,b,0));
    assert(shouldRedrawScene(false,true,0,a,b,0));
    assert(shouldRedrawScene(true,false,0,a,b,0));
    b.rects[0].src_x=1; assert(shouldRedrawScene(true,true,0,a,b,0)); b=a;
    b.display=1; assert(shouldRedrawScene(true,true,0,a,b,0)); b=a;
    b.mode=1; assert(shouldRedrawScene(true,true,0,a,b,0)); b=a;
    b.indicators=1; assert(shouldRedrawScene(true,true,0,a,b,0)); b=a;
    b.wide=true; assert(shouldRedrawScene(true,true,0,a,b,0));
    puts("PASS duplicate-scene gate, geometry, mode, indicator changes, periodic refresh");
    puts("V9 render rows: all pixel-equivalence, palette, cursor-region, resize and fallback tests passed");
}
