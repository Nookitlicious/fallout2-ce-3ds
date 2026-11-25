#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include "heap.h"
#include "art.h"
#include "memory.h"
using namespace fallout;
namespace fallout { int debugPrint(const char*, ...) { return 0; } }
static void writeFrm(const char* name, int size, int declared, bool duplicate = true) {
    std::vector<unsigned char> bytes(62 + 12 + 4, 0);
    auto s16=[&](int i,int v){bytes[i]=v>>8;bytes[i+1]=v;};
    auto s32=[&](int i,int v){s16(i,v>>16);s16(i+2,v);};
    s32(0,4); s16(8,1); s32(58,declared);
    s16(62,2);s16(64,2);s32(66,size); bytes[74]=7; bytes[75]=8; bytes[76]=9; bytes[77]=10;
    if (!duplicate) s32(38,99999);
    FILE* f=fopen(name,"wb");assert(f);assert(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());fclose(f);
}
int main(){
    Heap h; assert(heapInit(&h,8192));
    std::mt19937 r(1234); struct Item{int id;int size;unsigned char value;}; std::vector<Item> items;
    for(int i=0;i<5000;i++) {
        if(!items.empty() && (r()%2==0 || items.size()>20)) {
            size_t j=r()%items.size(); auto v=items[j]; unsigned char* p; assert(heapLock(&h,v.id,&p));for(int k=0;k<v.size;k++)assert(p[k]==v.value);assert(heapUnlock(&h,v.id));assert(heapBlockDeallocate(&h,&v.id));items.erase(items.begin()+j);
        } else {
            int size=1+r()%1700,id; if(heapBlockAllocate(&h,&id,size,1)) {unsigned char* p; assert(heapLock(&h,id,&p));unsigned char v=r();memset(p,v,size);assert(heapUnlock(&h,id));items.push_back({id,size,v});}
        }
        assert(heapValidate(&h));
    }
    for(auto v:items)assert(heapBlockDeallocate(&h,&v.id));
    assert(heapValidate(&h)); assert(heapFree(&h));
    assert(heapInit(&h,8192)); auto* header=(int*)h.data; int saved=header[1];header[1]=0x7fffffff;int id;assert(!heapBlockAllocate(&h,&id,8,0));header[1]=saved;assert(heapFree(&h));
    writeFrm("valid.frm",4,16); Art* art=artLoad("valid.frm");assert(art);for(int i=0;i<6;i++){auto* frame=artGetFrame(art,0,i);assert(frame && frame->size==4);assert(((unsigned char*)frame)[sizeof(ArtFrame)]==7);}internal_free(art);
    writeFrm("zero.frm",4,0);art=artLoad("zero.frm");assert(art);internal_free(art);
    writeFrm("oversized.frm",0x7fffffff,16);assert(!artLoad("oversized.frm"));
    writeFrm("offset.frm",4,16,false);assert(!artLoad("offset.frm"));
    alignas(Art) unsigned char small[sizeof(Art)+8];memset(small,0xa5,sizeof small);assert(artRead("valid.frm",small,sizeof small)!=0);for(size_t i=sizeof(Art);i<sizeof small;i++)assert(small[i]==0xa5);
    puts("Artwork heap and malformed FRM regression tests passed");
}
