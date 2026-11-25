#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include "o3ds_party_snapshot.h"
#include "proto.cc"
using namespace fallout;
namespace fallout {
int debugPrint(const char*, ...) { return 0; }
Settings settings;
char* artGetObjectTypeName(int type) { static char name[]="critters"; assert(type==OBJ_TYPE_CRITTER); return name; }
}
int main() {
    Proto proto{};
    auto& p = proto.critter;
    p.pid = 0x0100002a; p.messageId = -1; p.fid = 0x01010203; p.lightDistance = 7; p.lightIntensity = 65536;
    p.flags = INT_MIN; p.extendedFlags = 0x12345; p.sid = -42; p.headFid = 12; p.aiPacket = 17; p.team = 2;
    p.data.flags = 0x1000;
    for (int i=0; i<35; ++i) { p.data.baseStats[i] = i*3; p.data.bonusStats[i] = -i*19; }
    for (int i=0; i<18; ++i) p.data.skills[i] = i*73;
    p.data.bodyType = 1; p.data.experience = 250; p.data.killType = 5; p.data.damageType = 3;
    unsigned char bytes[O3DS_CRITTER_PROTO_BYTES]; o3dsEncodeCritter(p, bytes);
    File* f = fileOpen("legacy.pro", "wb"); assert(f); assert(protoWrite(&proto,f)==0); assert(fileClose(f)==0);
    FILE* raw=fopen("legacy.pro","rb"); assert(raw); unsigned char expected[sizeof(bytes)]; assert(fread(expected,1,sizeof(expected),raw)==sizeof(expected)); assert(fgetc(raw)==EOF); assert(fclose(raw)==0);
    assert(memcmp(bytes,expected,sizeof(bytes))==0);
    f=fileOpen("legacy.pro","rb"); assert(f); Proto parsed{}; assert(protoRead(&parsed,f)==0); assert(fileClose(f)==0); unsigned char readBack[sizeof(bytes)]; o3dsEncodeCritter(parsed.critter, readBack); assert(memcmp(bytes,readBack,sizeof(bytes))==0);
    O3dsPartySnapshots snapshots; const char* path="proto\\critters\\00000042.pro";
    snapshots.remember(p.pid,path,bytes,sizeof(bytes)); assert(snapshots.find(p.pid,path)); assert(memcmp(snapshots.find(p.pid,path),bytes,sizeof(bytes))==0);
    ++p.data.baseStats[0]; o3dsEncodeCritter(p,bytes); assert(memcmp(snapshots.find(p.pid,path),bytes,sizeof(bytes))!=0);
    assert(!snapshots.find(p.pid,"other.pro")); snapshots.remember(p.pid,path,bytes,sizeof(bytes)-4); assert(!snapshots.find(p.pid,path));
    snapshots.remember(p.pid+1,path,bytes,sizeof(bytes)); assert(!snapshots.find(p.pid+1,path));
    snapshots.remember(p.pid,path,bytes,sizeof(bytes)); snapshots.clear(); assert(!snapshots.find(p.pid,path));
    puts("PASS V8 critter serializer matches actual legacy writer and reader; mutations, filename and invalidation checks");
    // Exercise the actual optimized save function with a restored disk record,
    // then a loaded and mutated live companion, then a map cache clear.
    assert(mkdir("data",0755)==0 && mkdir("data/proto",0755)==0 && mkdir("data/proto/critters",0755)==0);
    FILE* list=fopen("data/proto/critters/critters.lst","wb"); assert(list);
    for(int i=1;i<=42;++i) fprintf(list,"%08d.pro\n",i); assert(fclose(list)==0);
    const char* physical="data/proto/critters/00000042.pro";
    raw=fopen(physical,"wb"); assert(raw); assert(fwrite(bytes,1,sizeof(bytes),raw)==sizeof(bytes)); assert(fclose(raw)==0);
    assert(xbaseOpen("data"));
    struct utimbuf oldTime={1000000,1000000}; struct stat st{};
    assert(utime(physical,&oldTime)==0);
    protoRememberRestored(p.pid,"00000042.pro",bytes,sizeof(bytes));
    assert(_proto_save_pid(p.pid)==0); assert(stat(physical,&st)==0 && st.st_mtime==oldTime.modtime);
    assert(findResidentProto(p.pid)==nullptr);
    Proto* live=nullptr; assert(protoGetProto(p.pid,&live)==0 && live);
    assert(_proto_save_pid(p.pid)==0 && stat(physical,&st)==0 && st.st_mtime==oldTime.modtime);
    live->critter.data.skills[0]+=5; o3dsEncodeCritter(live->critter,bytes);
    assert(_proto_save_pid(p.pid)==0); assert(stat(physical,&st)==0 && st.st_mtime!=oldTime.modtime);
    raw=fopen(physical,"rb"); assert(raw); assert(fread(expected,1,sizeof(expected),raw)==sizeof(expected)); assert(fgetc(raw)==EOF); assert(fclose(raw)==0); assert(!memcmp(bytes,expected,sizeof(bytes)));
    _proto_remove_all(); assert(utime(physical,&oldTime)==0);
    assert(_proto_save_pid(p.pid)==0 && stat(physical,&st)==0 && st.st_mtime==oldTime.modtime);
    assert(findResidentProto(p.pid)==nullptr);
    protoClearPartySnapshots(); assert(_proto_save_pid(p.pid)==0 && findResidentProto(p.pid));
    _proto_remove_all(); protoClearPartySnapshots();
#ifdef O3DS_TEST_V101
    // Restore without an SD write; normal map saves skip unchanged deferred
    // records. A real lookup materializes exact bytes before legacy parsing.
    assert(unlink(physical)==0);
    assert(protoCanDeferRestored(p.pid,"00000042.pro"));
    assert(!protoCanDeferRestored(p.pid,"../bad.pro"));
    assert(!protoDeferRestored(p.pid,"00000042.pro",bytes,sizeof(bytes)-4));
    assert(protoDeferRestored(p.pid,"00000042.pro",bytes,sizeof(bytes)));
    assert(access(physical,F_OK)!=0);
    assert(_proto_save_pid(p.pid)==0 && access(physical,F_OK)!=0);
    _proto_remove_all(); // map cache eviction is not a game reset
    assert(_proto_save_pid(p.pid)==0 && access(physical,F_OK)!=0);
    live=nullptr; assert(protoGetProto(p.pid,&live)==0 && live);
    o3dsEncodeCritter(live->critter,readBack); assert(!memcmp(bytes,readBack,sizeof(bytes)));
    assert(access(physical,F_OK)==0 && !partySnapshots.pendingPath(p.pid));
    live->critter.data.baseStats[4]+=123; o3dsEncodeCritter(live->critter,bytes);
    assert(_proto_save_pid(p.pid)==0);
    raw=fopen(physical,"rb"); assert(raw && fread(expected,1,sizeof(expected),raw)==sizeof(expected)); fclose(raw);
    assert(!memcmp(bytes,expected,sizeof(bytes)));
    _proto_remove_all(); protoClearPartySnapshots(); assert(unlink(physical)==0);
    // The explicit save path calls this even for companions never encountered.
    assert(protoDeferRestored(p.pid,"00000042.pro",bytes,sizeof(bytes)));
    assert(mkdir(physical,0755)==0); // failure must preserve the deferred record
    assert(protoMaterializePending(p.pid)==-1 && partySnapshots.pendingPath(p.pid));
    assert(rmdir(physical)==0);
    assert(protoMaterializePending(p.pid)==0 && !partySnapshots.pendingPath(p.pid));
    raw=fopen(physical,"rb"); assert(raw && fread(expected,1,sizeof(expected),raw)==sizeof(expected)); assert(fgetc(raw)==EOF); fclose(raw);
    assert(!memcmp(bytes,expected,sizeof(bytes)));
    assert(protoMaterializePending(p.pid)==0);
    protoClearPartySnapshots(); assert(unlink(physical)==0);
    assert(protoDeferRestored(p.pid,"00000042.pro",bytes,sizeof(bytes)));
    protoClearPartySnapshots(); // loading another slot must drop old working bytes
    assert(protoMaterializePending(p.pid)==0 && access(physical,F_OK)!=0);
    settings.system.master_patches_path="other";
    assert(!protoCanDeferRestored(p.pid,"00000042.pro"));
    settings.system.master_patches_path="data";
    puts("PASS V10.1 deferred restore, no redundant write, exact lazy materialization, mutation, explicit save, retry and reset");
#endif
    _proto_remove_all(); clearProtoNameIndices(); dbExit();
    puts("PASS V8 actual party save: restored/nonresident and unchanged live records skip disk writes; changed data persists; eviction and reset behave correctly");
}
