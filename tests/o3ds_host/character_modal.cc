#include <cassert>
#include <cstdio>
#include <cmath>
#include "platform/ctr/ctr_rectmap.h"
#include "platform/ctr/ctr_input.h"
namespace fallout {
ctr_rectMap_t ctr_rectMap{};
ctr_input_t ctr_input{};
void ctr_input_center_mouse() {}
}
using namespace fallout;
static int verify(rectMap_e map, int x, int y, int w, int h) {
    const auto* r = rectMaps[map][0];
    assert(r->src_x == x && r->src_y == y && r->src_w == w && r->src_h == h);
    assert(r->dst_x >= 0 && r->dst_y >= 0 && r->dst_x+r->dst_w <= 320.01 && r->dst_y+r->dst_h <= 240.01);
    assert(std::abs(r->dst_w/r->dst_h-float(w)/h)<0.001);
    int count = 0;
    float top = 240-r->dst_y-r->dst_h;
    for(int ty=0;ty<240;++ty) for(int tx=0;tx<320;++tx) {
        int sx=-1,sy=-1; characterModalTouchToTexture(map,tx,ty,&sx,&sy);
        bool inside=tx>=r->dst_x&&tx<r->dst_x+r->dst_w&&ty>=top&&ty<top+r->dst_h;
        if(inside) {
            assert(sx>=x&&sx<x+w&&sy>=y&&sy<y+h);
            assert(std::abs((sx-x)-(tx-r->dst_x)*w/r->dst_w)<=1.001);
            assert(std::abs((sy-y)-(ty-top)*h/r->dst_h)<=1.001);
        } else assert(sx==0&&sy==0);
        ++count;
    }
    return count;
}
int main() {
    ctr_rectmap_init(); setActiveRectMap(DISPLAY_CHAR); offsetY_char=175;
    int count=0;
    {
        ScopedCharacterModalMap options(DISPLAY_CHAR_OPTIONS,238,90,164,300);
        assert(ctr_rectMap.active==DISPLAY_CHAR_OPTIONS);
        count+=verify(DISPLAY_CHAR_OPTIONS,238,90,164,300);
        {
            ScopedCharacterModalMap file(DISPLAY_CHAR_FILE,168,80,300,240);
            assert(ctr_rectMap.active==DISPLAY_CHAR_FILE);
            count+=verify(DISPLAY_CHAR_FILE,168,80,300,240);
            setRectMapPos(DISPLAY_DYNAMIC,169,126,300,100,false);
            setActiveRectMap(DISPLAY_DYNAMIC);
            setActiveRectMap(DISPLAY_CHAR_FILE);
            count+=verify(DISPLAY_CHAR_FILE,168,80,300,240);
        }
        assert(ctr_rectMap.active==DISPLAY_CHAR_OPTIONS);
        count+=verify(DISPLAY_CHAR_OPTIONS,238,90,164,300);
    }
    assert(ctr_rectMap.active==DISPLAY_CHAR && offsetY_char==175);
    { ScopedCharacterModalMap disabled(DISPLAY_CHAR_FILE,0,0,0,0,false); assert(ctr_rectMap.active==DISPLAY_CHAR); }
    scrollCharacterView(1000); assert(offsetY_char==525);
    scrollCharacterView(-1000); assert(offsetY_char==0);
    scrollCharacterView(6); assert(offsetY_char==6);
    isAgeWindow=true; scrollCharacterView(6); assert(offsetY_char==6); isAgeWindow=false;
    isSexWindow=true; scrollCharacterView(-6); assert(offsetY_char==6); isSexWindow=false;
    ctr_rectmap_exit(); printf("Character modal mapping/state checks passed (%d touch points)\n",count);
}
