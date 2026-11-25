#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <cstdint>
#include <vector>
#include <sys/stat.h>
#include "db.h"
#include "dfile.h"
#include "file_utils.h"
#include "memory.h"
#include "o3ds_proto_index.h"
#include <unistd.h>
#include <string>
#include "message.cc" // test internal lookup and insertion functions
#include "platform_compat.h"
#include "platform/ctr/ctr_original_menu.h"
using namespace fallout;
namespace fallout { int debugPrint(const char*, ...) { return 0; } }
static void writeFile(const char* path, const std::vector<unsigned char>& data) {
    FILE* f = fopen(path,"wb"); assert(f); if (!data.empty()) assert(fwrite(data.data(),1,data.size(),f)==data.size()); assert(fclose(f)==0);
}
static void checkFile(const char* path, const std::vector<unsigned char>& data) {
    FILE* f=fopen(path,"rb"); assert(f); std::vector<unsigned char> b(data.size());
    if (!b.empty()) assert(fread(b.data(),1,b.size(),f)==b.size()); assert(b==data); assert(fgetc(f)==EOF); fclose(f);
}
static void write32(FILE* f, uint32_t value) { unsigned char b[4] = {(unsigned char)value,(unsigned char)(value>>8),(unsigned char)(value>>16),(unsigned char)(value>>24)}; assert(fwrite(b,1,4,f)==4); }
static void makeDat(const char* path,const std::vector<unsigned char>& data,bool compressed,const char* name="test.bin") {
    std::vector<unsigned char> packed=data;
    if(compressed) { uLongf size=compressBound(data.size()); packed.resize(size); assert(compress(packed.data(),&size,data.data(),data.size())==Z_OK); packed.resize(size); }
    FILE* f=fopen(path,"wb"); assert(f); assert(fwrite(packed.data(),1,packed.size(),f)==packed.size());
    uint32_t tableSize=4+4+strlen(name)+1+12;
    write32(f,1); write32(f,strlen(name)); fwrite(name,1,strlen(name),f); fputc(compressed?1:0,f); write32(f,data.size()); write32(f,packed.size()); write32(f,0); write32(f,tableSize); write32(f,packed.size()+tableSize+8); fclose(f);
}
static void makeTwoEntryDat(const char* path, const std::vector<unsigned char>& data) {
    std::vector<unsigned char> packed(compressBound(data.size())); uLongf packedSize=packed.size();
    assert(compress(packed.data(), &packedSize, data.data(), data.size()) == Z_OK); packed.resize(packedSize);
    FILE* f=fopen(path,"wb"); assert(f);
    assert(fwrite(data.data(),1,data.size(),f)==data.size());
    assert(fwrite(packed.data(),1,packed.size(),f)==packed.size());
    const char* names[]={"a.bin","b.bin"};
    uint32_t tableSize=4; write32(f,2);
    for(int i=0;i<2;++i) {
        uint32_t len=strlen(names[i]); tableSize+=4+len+1+12;
        write32(f,len); fwrite(names[i],1,len,f); fputc(i,f);
        write32(f,data.size()); write32(f,i?packed.size():data.size()); write32(f,i?data.size():0);
    }
    write32(f,tableSize); write32(f,data.size()+packed.size()+tableSize+8); assert(fclose(f)==0);
}
static void testOriginalMenu() {
    constexpr size_t pixels = 640 * 480;
    const char* entry = "art\\intrface\\mainmenu.frm";
    std::vector<unsigned char> original(74 + pixels, 0);
    auto set16 = [&](size_t n, unsigned int v) { original[n] = v >> 8; original[n+1] = v; };
    auto set32 = [&](size_t n, unsigned int v) { set16(n,v >> 16); set16(n+2,v); };
    set32(0,4); set16(8,1); set32(58,pixels+12); set16(62,640); set16(64,480); set32(66,pixels);
    for(size_t i=0;i<pixels;++i) original[74+i]=static_cast<unsigned char>(i*13+i/17);
    auto patch = original; patch[74] ^= 255;
    makeDat("master-menu.dat",original,true,entry);
    makeDat("patch-menu.dat",patch,false,entry);
    assert(xbaseOpen("master-menu.dat")); assert(xbaseOpen("patch-menu.dat"));
    std::vector<unsigned char> out(pixels+2,0xAC);
    assert(ctr_menu::loadOriginalBackground("MASTER-MENU.DAT",out.data()+1,pixels));
    assert(!memcmp(out.data()+1,original.data()+74,pixels)); assert(out.front()==0xAC && out.back()==0xAC);
    assert(!ctr_menu::loadOriginalBackground("master-menu.dat",out.data(),pixels-1));
    assert(!ctr_menu::loadOriginalBackground("missing.dat",out.data(),pixels));
    assert(!ctr_menu::loadOriginalBackground("master-menu.dat",nullptr,pixels));
    File* normal=fileOpen(entry,"rb"); assert(normal); std::vector<unsigned char> buf(patch.size());
    assert(fileRead(buf.data(),1,buf.size(),normal)==buf.size()); fileClose(normal); assert(buf==patch);
    auto invalid=original; invalid[62]=0; makeDat("bad-menu.dat",invalid,false,entry); assert(xbaseOpen("bad-menu.dat"));
    assert(!ctr_menu::loadOriginalBackground("bad-menu.dat",out.data(),pixels));
    invalid.resize(100); makeDat("short-menu.dat",invalid,true,entry); assert(xbaseOpen("short-menu.dat"));
    assert(!ctr_menu::loadOriginalBackground("short-menu.dat",out.data(),pixels));
    dbExit(); puts("PASS V10.5 direct original FRM read, patch priority unchanged, bounds and malformed input");
}
int main() {
    testOriginalMenu();
    char* str=internal_strdup("hello"); assert(str && !strcmp(str,"hello")); internal_free(str);
    assert(internal_malloc(SIZE_MAX)==nullptr); void* block=internal_malloc(32); assert(block); memset(block,42,32); assert(internal_realloc(block,SIZE_MAX)==nullptr); assert(((char*)block)[0]==42); block=internal_realloc(block,64); assert(block && ((char*)block)[0]==42); assert(internal_realloc(block,0)==nullptr); puts("PASS allocation overflow and realloc-to-zero");
    MessageList m{}; char empty[]="", value[]="test", replacement[]="updated";
    for(int i: {INT_MAX, 7, -100, INT_MIN, 0, 25, 7}) { MessageListItem e{i,0,empty,value}; assert(_message_add(&m,&e)); }
    assert(m.entries_num==6); for(int i=1;i<m.entries_num;i++) assert(m.entries[i-1].num<m.entries[i].num);
    MessageListItem e{7,0,empty,replacement}; assert(_message_add(&m,&e)); assert(messageListGetItem(&m,&e)); assert(!strcmp(e.text,replacement)); int index; assert(!_message_find(&m,6,&index)); assert(m.entries[index].num==7); assert(messageListFree(&m)); puts("PASS sorted message insertion, duplicates, extreme IDs");
    std::vector<unsigned char> data(23017); for(size_t i=0;i<data.size();i++) data[i]=(i*31+i/7)%256;
    writeFile("source.bin",data); assert(fileCopyCompressed("source.bin","source.gz")==0); assert(fileCopyDecompressed("source.gz","restored.bin")==0); checkFile("restored.bin",data); assert(fileCopyDecompressed("source.bin","plain.bin")==0); checkFile("plain.bin",data); assert(fileCopyCompressed("source.gz","copy.gz")==0); assert(fileCopyDecompressed("copy.gz","again.bin")==0); checkFile("again.bin",data); assert(fileCopyDecompressed("missing","bad.bin")==-1); puts("PASS block gzip and plain-file copies");
    // V8 captures exact small restored data, rejects oversized snapshots,
    // preserves bytes, and only publishes a snapshot after a successful close.
    std::vector<unsigned char> protoFixture(416,0x42);
    writeFile("proto-fixture.pro",protoFixture);
    assert(fileCopyCompressed("proto-fixture.pro","proto-fixture.gz")==0);
    unsigned char snapshot[416]; unsigned int captured=99;
    assert(fileCopyDecompressedSnapshot("proto-fixture.gz","proto-fixture-restored.pro",snapshot,sizeof(snapshot),&captured)==0);
    assert(captured==sizeof(snapshot) && !memcmp(snapshot,protoFixture.data(),captured)); checkFile("proto-fixture-restored.pro",protoFixture);
    assert(fileCopyDecompressedSnapshot("proto-fixture.pro","proto-fixture-small.pro",snapshot,400,&captured)==0 && captured==0);
    assert(fileCopyDecompressedSnapshot("missing-file","proto-fixture-error.pro",snapshot,sizeof(snapshot),&captured)==-1 && captured==0);
    writeFile("patch000.dat",{}); writeFile("PATCH012.DAT",{}); writeFile("patch999.dat",{}); writeFile("patch01.dat",{}); writeFile("patch123.dat.bak",{});
    bool patches[1000]; assert(fileFindStockPatches(patches)); for(int i=0;i<1000;++i) assert(patches[i] == (i==0 || i==12 || i==999));
    puts("PASS V8 bounded restored snapshots and stock patch discovery");
    // V10.1 reads an exact canonical record without touching a destination.
    assert(fileReadSmallDecompressed("proto-fixture.gz",snapshot,sizeof(snapshot),&captured)==0 && captured==sizeof(snapshot));
    assert(!memcmp(snapshot,protoFixture.data(),captured));
    assert(fileReadSmallDecompressed("proto-fixture.pro",snapshot,sizeof(snapshot),&captured)==0 && captured==sizeof(snapshot));
    assert(fileReadSmallDecompressed("proto-fixture.gz",snapshot,400,&captured)==1 && captured==0);
    assert(fileReadSmallDecompressed("missing-file",snapshot,sizeof(snapshot),&captured)==-1 && captured==0);
    FILE* shortGz=fopen("proto-fixture.gz","rb"); assert(shortGz); assert(fseek(shortGz,0,SEEK_END)==0); long shortGzSize=ftell(shortGz); assert(shortGzSize>8); fclose(shortGz);
    assert(truncate("proto-fixture.gz",shortGzSize-5)==0);
    assert(fileReadSmallDecompressed("proto-fixture.gz",snapshot,sizeof(snapshot),&captured)==-1 && captured==0);
    puts("PASS V10.1 bounded raw/gzip read, overflow fallback, missing and truncated records");
    // V7: transfers crossing several 64KiB boundaries, raw and gzip,
    // truncated gzip, and the legacy byte format after fast compression.
    std::vector<unsigned char> large(262147);
    for(size_t i=0;i<large.size();++i) large[i]=static_cast<unsigned char>((i*31)^(i>>7));
    writeFile("large.bin",large);
    assert(fileCopyCompressed("large.bin","large.gz")==0);
    assert(fileCopyDecompressed("large.gz","large-restored.bin")==0); checkFile("large-restored.bin",large);
    assert(fileCopyDecompressed("large.bin","large-plain.bin")==0); checkFile("large-plain.bin",large);
    assert(fileCopyCompressed("large.gz","large-copy.gz")==0);
    assert(fileCopyDecompressed("large-copy.gz","large-copy.bin")==0); checkFile("large-copy.bin",large);
    FILE* gz=fopen("large-copy.gz","rb"); assert(gz); assert(fseek(gz,0,SEEK_END)==0); long zsize=ftell(gz); assert(zsize>8); fclose(gz);
    assert(truncate("large-copy.gz",zsize-6)==0);
    assert(fileCopyDecompressed("large-copy.gz","truncated.bin")==-1);
    for(const std::vector<unsigned char>& small: {std::vector<unsigned char>{},std::vector<unsigned char>{0x1f},std::vector<unsigned char>{0x1f,0x8a}}) {
        writeFile("short.bin",small); assert(fileCopyDecompressed("short.bin","short-copy.bin")==0); checkFile("short-copy.bin",small);
    }
    File* bufferedWrite=fileOpen("seek-write.bin","wb"); assert(bufferedWrite);
    assert(fileWrite(large.data(),1,large.size(),bufferedWrite)==large.size());
    assert(fileSeek(bufferedWrite,65534,SEEK_SET)==0); unsigned char marker[]={1,2,3,4,5};
    assert(fileWrite(marker,1,sizeof(marker),bufferedWrite)==sizeof(marker));
    assert(fileSeek(bufferedWrite,0,SEEK_END)==0); assert(fileTell(bufferedWrite)==(long)large.size());
    assert(fileClose(bufferedWrite)==0); memcpy(large.data()+65534,marker,sizeof(marker)); checkFile("seek-write.bin",large);
    puts("PASS V7 large gzip/raw transfers, truncated gzip rejection, empty files, buffered seek/write");
    assert(mkdir("cleanup",0755)==0); assert(mkdir("cleanup/KEEP.SAV",0755)==0);
    writeFile("cleanup/keep.msg",data); writeFile("cleanup/noextension",data); writeFile("cleanup/also.sav.bak",data);
    for(int i=0;i<100;++i) { std::string n="cleanup/"+std::to_string(i)+(i%2?".sav":".SAV"); writeFile(n.c_str(),data); }
    assert(fileEraseLocalExtension("cleanup/","SAV")==0);
    for(int i=0;i<100;++i) { std::string n="cleanup/"+std::to_string(i)+(i%2?".sav":".SAV"); assert(access(n.c_str(),F_OK)!=0); }
    checkFile("cleanup/keep.msg",data); checkFile("cleanup/noextension",data); checkFile("cleanup/also.sav.bak",data);
    struct stat st{}; assert(stat("cleanup/KEEP.SAV",&st)==0 && S_ISDIR(st.st_mode));
    assert(fileEraseLocalExtension("missing-directory","SAV")==0);
    puts("PASS V7 direct local cleanup, case-insensitive extensions, directory and unrelated-file preservation");
    // Compare indexed prototype names to the original line reader.
    std::string protoText="00000001.pro comment\r\n00000002.pro\n\n"+std::string(260,'x')+"\n00000003.pro";
    std::vector<unsigned char> protoBytes(protoText.begin(),protoText.end());
    for(bool compressed: {false,true}) {
        makeDat("proto.dat",protoBytes,compressed); DBase* db=dbaseOpen("proto.dat"); assert(db);
        DFile* old=dfileOpen(db,"test.bin","rt"); DFile* fresh=dfileOpen(db,"test.bin","rt"); assert(old&&fresh);
        File wrapper{}; wrapper.type=XFILE_TYPE_DFILE; wrapper.dfile=fresh;
        O3dsProtoIndex index; assert(index.load(&wrapper)); assert(index.attempted());
        char line[256]; unsigned i=1; while(dfileReadString(line,sizeof(line),old)) { O3dsProtoIndex::trim(line); assert(index.lookup(i)&&!strcmp(line,index.lookup(i))); ++i; }
        assert(!index.lookup(i)&&!index.lookup(0)); index.clear(); assert(!index.attempted()&&!index.lookup(1));
        assert(dfileClose(old)==0); assert(dfileClose(fresh)==0); assert(dbaseClose(db));
    }
    puts("PASS V7 prototype name index parity with legacy DAT lines, reset, blank and long lines");
    for(bool compressed: {false,true}) { makeDat("test.dat",data,compressed); DBase* db=dbaseOpen("test.dat"); assert(db); DFile* file=dfileOpen(db,"test.bin","rb"); assert(file); unsigned char buf[64]; assert(dfileSeek(file,12345,SEEK_SET)==0); assert(dfileTell(file)==12345); assert(dfileRead(buf,1,sizeof(buf),file)==sizeof(buf)); assert(!memcmp(buf,data.data()+12345,sizeof(buf))); assert(dfileSeek(file,-30,SEEK_CUR)==0); assert(dfileTell(file)==12379); assert(dfileReadChar(file)==data[12379]); assert(dfileSeek(file,0,SEEK_SET)==0); assert(dfileReadChar(file)==data[0]); assert(dfileSeek(file,0,SEEK_END)==0); assert(dfileTell(file)==(long)data.size()); assert(dfileReadChar(file)==EOF); assert(dfileSeek(file,-1,SEEK_SET)!=0); assert(dfileClose(file)==0); assert(dbaseClose(db)); }
    puts("PASS stored and compressed DAT seek, rewind, EOF");
    // Reuse one FILE across logically independent opens; concurrent open handles must remain independent.
    for (bool compressed : {false, true}) {
        makeDat("pool.dat", data, compressed);
        DBase* db = dbaseOpen("pool.dat"); assert(db);
        DFile* first = dfileOpen(db,"test.bin","rb"); assert(first);
        FILE* native = first->stream;
        DFile* second = dfileOpen(db,"test.bin","rb"); assert(second && second->stream != native);
        assert(dfileSeek(first,1000,SEEK_SET)==0 && dfileSeek(second,500,SEEK_SET)==0);
        assert(dfileReadChar(first)==data[1000] && dfileReadChar(second)==data[500]);
        assert(dfileClose(first)==0 && db->idleStream==native);
        first=dfileOpen(db,"test.bin","rb"); assert(first && first->stream==native && !db->idleStream);
        assert(dfileTell(first)==0 && dfileReadChar(first)==data[0] && dfileReadChar(second)==data[501]);
        assert(dfileClose(first)==0 && dfileClose(second)==0 && db->idleStream==native);
        assert(dbaseClose(db));
    }
    makeTwoEntryDat("mixed.dat",data);
    DBase* mixed=dbaseOpen("mixed.dat"); assert(mixed);
    FILE* native=nullptr;
    for(int i=0;i<24;++i) {
        DFile* f=dfileOpen(mixed,i%2?"b.bin":"a.bin","rb"); assert(f);
        if(i==0) native=f->stream; else assert(f->stream==native);
        assert(dfileTell(f)==0 && dfileReadChar(f)==data[0]);
        assert(dfileSeek(f,20000,SEEK_SET)==0 && dfileReadChar(f)==data[20000]);
        assert(dfileClose(f)==0);
    }
    assert(dbaseClose(mixed));
    puts("PASS archive stream reuse, concurrent handles, independent seek and inflate state");
    // Verify serialized bytes exactly, including negative values and chunk boundaries.
    std::vector<int> words(10001); std::vector<short> shorts(1001);
    std::vector<unsigned char> expected;
    auto be32 = [&](uint32_t v) { for(int j=3;j>=0;--j) expected.push_back((v>>(j*8))&255); };
    auto be16 = [&](uint16_t v) { expected.push_back(v>>8); expected.push_back(v&255); };
    for(size_t i=0;i<words.size();++i) words[i]=int(i)*123457-500000000;
    for(size_t i=0;i<shorts.size();++i) shorts[i]=short(int(i)*47-30000);
    File* out=fileOpen("binary-write.bin","wb"); assert(out);
    assert(fileWriteInt32(out,INT_MIN)==0); be32(INT_MIN);
    assert(fileWriteInt16(out,-12345)==0); be16(uint16_t(-12345));
    assert(fileWriteInt32List(out,words.data(),words.size())==0); for(int w:words) be32(uint32_t(w));
    assert(fileWriteInt16List(out,shorts.data(),shorts.size())==0); for(short w:shorts) be16(uint16_t(w));
    assert(fileWriteUInt8List(out,data.data(),data.size())==0); expected.insert(expected.end(),data.begin(),data.end());
    assert(fileWriteUInt8List(out,nullptr,0)==0 && fileWriteInt32List(out,nullptr,-1)==-1);
    assert(fileClose(out)==0); checkFile("binary-write.bin",expected);
    File* in=fileOpen("binary-write.bin","rb"); assert(in); int scalar; short s16;
    assert(fileReadInt32(in,&scalar)==0 && scalar==INT_MIN); assert(fileReadInt16(in,&s16)==0 && s16==-12345);
    std::vector<int> actualWords(words.size()); std::vector<short> actualShorts(shorts.size()); std::vector<unsigned char> actualData(data.size());
    assert(fileReadInt32List(in,actualWords.data(),actualWords.size())==0 && actualWords==words);
    assert(fileReadInt16List(in,actualShorts.data(),actualShorts.size())==0 && actualShorts==shorts);
    assert(fileReadUInt8List(in,actualData.data(),actualData.size())==0 && actualData==data);
    assert(fileReadUInt8List(in,actualData.data(),1)==-1); assert(fileClose(in)==0);
    puts("PASS byte-exact block serialization and roundtrip across chunk boundaries");
    // Compare the new sequential buffered reader against the legacy DAT text
    // reader with CRLF split across the 2 KiB refill boundary and bare CRs.
    std::vector<unsigned char> txt(2047, 'a');
    const unsigned char suffix[] = {'\r','\n','b','\r','c','\n',0xff,'\r','\r','\n','x','\r'};
    txt.insert(txt.end(), suffix, suffix+sizeof(suffix));
    for(bool compressed: {false,true}) {
        makeDat("text.dat",txt,compressed); DBase* db=dbaseOpen("text.dat"); assert(db);
        DFile* old=dfileOpen(db,"test.bin","rt"); DFile* fresh=dfileOpen(db,"test.bin","rt"); assert(old&&fresh);
        File wrapper{}; wrapper.type=XFILE_TYPE_DFILE; wrapper.dfile=fresh;
        O3dsTextReader buffered(&wrapper);
        for (;;) { int expected=dfileReadChar(old); assert(buffered.readChar()==expected); if(expected==EOF) break; }
        assert(buffered.readChar()==EOF); assert(dfileClose(old)==0); assert(dfileClose(fresh)==0);
        old=dfileOpen(db,"test.bin","rt"); fresh=dfileOpen(db,"test.bin","rt"); assert(old&&fresh); wrapper.dfile=fresh;
        O3dsTextReader lines(&wrapper); char a[256],b[256];
        for (;;) { char* expected=dfileReadString(a,sizeof(a),old); char* actual=lines.readLine(b,sizeof(b)); assert((actual==nullptr)==(expected==nullptr)); if(!actual) break; assert(!strcmp(a,b)); }
        assert(dfileClose(old)==0); assert(dfileClose(fresh)==0); assert(dbaseClose(db));
    }
    const std::string sample="comment\r\n{101}{}{hello\r\nworld}\n{102}{sound}{next}\n";
    std::vector<unsigned char> msg(sample.begin(),sample.end());
    for(bool compressed: {false,true}) {
        makeDat("message.dat",msg,compressed); DBase* db=dbaseOpen("message.dat"); assert(db);
        DFile* f=dfileOpen(db,"test.bin","rt"); assert(f); File wrapper{}; wrapper.type=XFILE_TYPE_DFILE; wrapper.dfile=f;
        O3dsTextReader reader(&wrapper); char field[MESSAGE_LIST_ITEM_FIELD_MAX_SIZE];
        for(const char* expected: {"101","","helloworld","102","sound","next"}) { assert(_message_load_field(reader,field)==0); assert(!strcmp(field,expected)); }
        assert(_message_load_field(reader,field)==1); assert(dfileClose(f)==0); assert(dbaseClose(db));
    }
    puts("PASS buffered DAT text, CRLF boundaries, bare CR, high bytes, lines and message fields");
    mkdir("MAPS",0755); mkdir("patch",0755); mkdir("patch/MAPS",0755); writeFile("MAPS/A.SAV",data); writeFile("patch/MAPS/A.SAV",data); writeFile("patch/MAPS/B.SAV",data); assert(xbaseOpen("patch"));
    char** a=nullptr; char** b=nullptr; char** c=nullptr; assert(fileNameListInit("MAPS/*.SAV",&a,0,0)==2); assert(!compat_stricmp(a[0],"A.SAV")&&!compat_stricmp(a[1],"B.SAV")); assert(fileNameListInit("MAPS/*.SAV",&b,0,0)==2); assert(fileNameListInit("MAPS/*.SAV",&c,0,0)==2); fileNameListFree(&b,0); fileNameListFree(&a,0); fileNameListFree(&c,0); assert(!a&&!b&&!c); assert(fileNameListInit("missing/*.sav",&a,0,0)==0 && !a); dbExit(); puts("PASS duplicate save filenames and out-of-order list cleanup");
    assert(xbaseOpen("patch")); assert(xfilePrimaryLoosePath("test.bin","patch")); assert(!xfilePrimaryLoosePath("test.bin","wrong"));
    makeDat("priority.dat",data,false); assert(xbaseOpen("priority.dat"));
    assert(!xfilePrimaryLoosePath("test.bin","patch")); assert(xfilePrimaryLoosePath("different.bin","patch"));
    assert(!xfilePrimaryLoosePath("../test.bin","patch")); assert(xbaseOpen("patch")); assert(xfilePrimaryLoosePath("test.bin","patch")); dbExit();
    puts("PASS V8 conservative loose-file precedence across archive overrides and reordered bases");
}
