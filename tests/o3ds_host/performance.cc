#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "art.h"
#include "cache.h"
#include "db.h"
#include "memory.h"
using namespace fallout;
namespace fallout {
int debugPrint(const char*, ...) { return 0; }
void soundContinueAll() {}
}
static void save(const char* path,const std::vector<unsigned char>& bytes) {
    FILE* f=fopen(path,"wb"); assert(f); assert(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()); assert(fclose(f)==0);
}
static void be16(std::vector<unsigned char>& b, uint16_t v) { b.push_back(v>>8); b.push_back(v); }
static void be32(std::vector<unsigned char>& b, uint32_t v) { be16(b,v>>16); be16(b,v); }
static void le32(FILE* f,uint32_t v) { for(int i=0;i<4;i++) fputc((v>>(8*i))&255,f); }
static void makeDat(const char* path,const std::vector<unsigned char>& bytes,bool compressed) {
    std::vector<unsigned char> packed=bytes;
    if(compressed) { uLongf len=compressBound(bytes.size()); packed.resize(len); assert(compress(packed.data(),&len,bytes.data(),bytes.size())==Z_OK); packed.resize(len); }
    FILE* f=fopen(path,"wb"); assert(f); assert(fwrite(packed.data(),1,packed.size(),f)==packed.size());
    const char* name="archive.frm"; uint32_t tableSize=4+4+strlen(name)+1+12;
    le32(f,1); le32(f,strlen(name)); fwrite(name,1,strlen(name),f); fputc(compressed?1:0,f); le32(f,bytes.size()); le32(f,packed.size()); le32(f,0); le32(f,tableSize); le32(f,packed.size()+tableSize+8); fclose(f);
}
static int sizeProc(int, int* size) { *size=64; return 0; }
static int readProc(int key, int* size, unsigned char* data) { assert(*size==64); if(key==999999) return -1; memset(data,key&255,*size); return 0; }
static void freeProc(void*) {}
static void checkCache() {
    Cache cache{}; assert(cacheInit(&cache,sizeProc,readProc,freeProc,64*1024));
    CacheEntry* locked=nullptr; void* data=nullptr;
    assert(cacheLock(&cache,INT_MIN,&data,&locked)); assert(data); assert(locked->referenceCount==1);
    for(int key: {INT_MAX,0,-1,100,2,-100,1}) { CacheEntry* entry=nullptr; assert(cacheLock(&cache,key,&data,&entry)); assert(((unsigned char*)data)[0]==(key&255)); assert(cacheUnlock(&cache,entry)); }
    for(int i=0;i<350;i++) { int key=1000+i*13; CacheEntry* entry=nullptr; assert(cacheLock(&cache,key,&data,&entry)); assert(cacheUnlock(&cache,entry)); }
    for(int i=1;i<cache.entriesLength;i++) assert(cache.entries[i-1]->key<cache.entries[i]->key);
    // A hit should not allocate a second entry.
    int before=cache.entriesLength; CacheEntry* hit=nullptr; assert(cacheLock(&cache,INT_MAX,&data,&hit)); assert(cache.entriesLength==before); assert(cacheUnlock(&cache,hit));
    // Failed read must not create a cache entry.
    assert(!cacheLock(&cache,999999,&data,&hit)); assert(cache.entriesLength==before);
    assert(cacheFlush(&cache)); assert(cache.entriesLength==1 && cache.entries[0]==locked && cache.size==64); assert(cacheUnlock(&cache,locked)); assert(cacheFlush(&cache)); assert(cache.entriesLength==0 && cache.size==0); assert(cacheFree(&cache));
    // Exercise normal automatic eviction under cache pressure, not just flush.
    Cache pressure{}; assert(cacheInit(&pressure,sizeProc,readProc,freeProc,16*1024));
    assert(cacheLock(&pressure,-7,&data,&locked));
    for(int i=0;i<700;i++) { CacheEntry* e=nullptr; assert(cacheLock(&pressure,i,&data,&e)); assert(((unsigned char*)data)[0]==(i&255)); assert(cacheUnlock(&pressure,e)); }
    assert(pressure.entriesLength<700); assert(locked->referenceCount==1); assert(cacheUnlock(&pressure,locked)); assert(cacheFree(&pressure));
    puts("PASS sorted cache lookup, extreme keys, failed load, locked-entry preservation, bulk and pressure eviction");
}
static int callbacks=0;
static void progress(){ callbacks++; }
static void checkWords(const char* path) {
    File* f=fileOpen(path,"rb"); assert(f); int value=0; assert(fileReadInt32(f,&value)==0 && value==INT_MIN);
    int words[18]={}; callbacks=0; fileSetReadProgressHandler(progress,16); assert(fileReadInt32List(f,words,18)==0); assert(callbacks==4); fileSetReadProgressHandler(nullptr,0);
    for(int i=0;i<18;i++) assert(words[i]==i-9);
    value=42; assert(fileReadInt32(f,&value)==-1 && value==42); assert(fileClose(f)==0);
}
static void checkArt(const char* path) {
    Art* art=artLoad(path); assert(art); assert(art->field_0==4 && art->framesPerSecond==10 && art->frameCount==2);
    for(int i=0;i<6;i++){ assert(art->xOffsets[i]==i-3); assert(art->yOffsets[i]==3-i); assert(art->dataOffsets[i]==0); }
    auto* frame=reinterpret_cast<ArtFrame*>(reinterpret_cast<unsigned char*>(art)+sizeof(Art));
    assert(frame->width==3 && frame->height==1 && frame->size==3 && frame->x==-2 && frame->y==5); assert(!memcmp(frame+1,"abc",3));
    auto* second=reinterpret_cast<ArtFrame*>(reinterpret_cast<unsigned char*>(frame)+sizeof(ArtFrame)+4); assert(second->size==4 && second->width==2 && second->height==2 && second->x==7 && second->y==-8); assert(!memcmp(second+1,"defg",4)); internal_free(art);
}
int main(){
    checkCache();
    std::vector<unsigned char> words; be32(words,0x80000000); for(int i=-9;i<9;i++) be32(words,i); words.push_back(0xAB);
    save("words.bin",words); checkWords("words.bin"); gzFile gz=gzopen("words.gz","wb"); assert(gz); assert(gzwrite(gz,words.data(),words.size())==(int)words.size()); assert(gzclose(gz)==Z_OK); checkWords("words.gz");
    puts("PASS scalar and 18-word reads, signed endian conversion, progress and truncated EOF, plain/gzip");
    std::vector<unsigned char> frm; be32(frm,4); be16(frm,10); be16(frm,0); be16(frm,2); for(int i=0;i<6;i++) be16(frm,i-3); for(int i=0;i<6;i++) be16(frm,3-i); for(int i=0;i<6;i++) be32(frm,0); be32(frm,31); assert(frm.size()==62);
    be16(frm,3); be16(frm,1); be32(frm,3); be16(frm,-2); be16(frm,5); frm.insert(frm.end(),{'a','b','c'}); be16(frm,2); be16(frm,2); be32(frm,4); be16(frm,7); be16(frm,-8); frm.insert(frm.end(),{'d','e','f','g'}); assert(frm.size()==93);
    save("test.frm",frm); checkArt("test.frm");
    for(bool compressed: {false,true}) { makeDat("art.dat",frm,compressed); assert(xbaseOpen("art.dat")); checkArt("archive.frm"); dbExit(); }
    frm.resize(61); save("short.frm",frm); assert(artLoad("short.frm")==nullptr);
    puts("PASS FRM header and frame parsing, odd padding, negative offsets, plain/stored/compressed DAT, short header");
}
