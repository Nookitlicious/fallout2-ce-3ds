#include "o3ds_lighting.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
#include <cstring>
using namespace fallout;
static uint8_t table[256][256];
int main() {
    std::mt19937 rng(17);
    for(auto& row:table) for(auto& c:row) c=rng()%256;
    O3dsLightRows cache;
    for (int n=0;n<1200;n++) {
        int idx=rng()%256;
        int w=1+rng()%101,h=1+rng()%43,sp=w+5,dp=w+9;
        std::vector<uint8_t> src(sp*h), a(dp*h), b;
        for(auto& c:src)c=rng()%256;
        for(auto& c:a)c=rng()%256;
        b=a;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            unsigned c=src[y*sp+x]; if(c) a[y*dp+x]=c<229?table[c][idx]:c;
        }
        o3dsLightBlit(src.data(),w,h,sp,b.data(),dp,cache.lookup(table,idx));
        assert(a==b);
        if(n%13==0){ for(auto& row:table) for(auto& c:row)c=rng()%256; cache.invalidate(); }
    }
    assert(!cache.lookup(table,-1)); assert(!cache.lookup(table,256));
    for(int n=0;n<5000;n++){
        int x0=int(rng()%160)-30,y0=int(rng()%160)-30,x1=x0+int(rng()%80),y1=y0+int(rng()%80);
        bool valid=o3dsClipSquareBounds(x0,y0,x1,y1,100,73);
        if(valid){assert(x0>=0&&y0>=0&&x1<100&&y1<73&&x0<=x1&&y0<=y1);}
    }
    // Compare bulk overlap copies with the original vertical-scroll row loops.
    for(int dy:{-24,24}){
        int pitch=640,h=380,lines=h-24;
        std::vector<uint8_t> a(pitch*h); for(auto& c:a)c=rng()%256; auto b=a;
        int src=dy<0?pitch*(lines-1):pitch*24, dst=dy<0?pitch*(h-1):0,step=dy<0?-pitch:pitch;
        for(int y=0;y<lines;y++){memmove(a.data()+dst,a.data()+src,pitch);src+=step;dst+=step;}
        memmove(b.data()+(dy<0?pitch*24:0), b.data()+(dy<0?0:pitch*24),size_t(lines)*pitch);
        assert(a==b);
    }
    puts("PASS exact lighting with transparency and animated colors, cache invalidation, grid clipping, vertical scroll overlap");
}
