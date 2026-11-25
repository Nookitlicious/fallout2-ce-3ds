#ifndef FALLOUT_O3DS_PARTY_SNAPSHOT_H
#define FALLOUT_O3DS_PARTY_SNAPSHOT_H
#include <stdint.h>
#include <string.h>
#include "proto_types.h"
#include "platform_compat.h"
namespace fallout {
constexpr size_t O3DS_CRITTER_PROTO_BYTES = 416;
// The disk order is NOT the order of the C++ structure.
inline void o3dsEncodeCritter(const CritterProto& p, unsigned char* out) {
    auto word = [&out](int n) {
        uint32_t v = static_cast<uint32_t>(n);
        for (int shift = 24; shift >= 0; shift -= 8) *out++ = static_cast<unsigned char>(v >> shift);
    };
    word(p.pid); word(p.messageId); word(p.fid); word(p.lightDistance);
    word(p.lightIntensity); word(p.flags); word(p.extendedFlags); word(p.sid);
    word(p.headFid); word(p.aiPacket); word(p.team); word(p.data.flags);
    for (int v : p.data.baseStats) word(v);
    for (int v : p.data.bonusStats) word(v);
    for (int v : p.data.skills) word(v);
    word(p.data.bodyType); word(p.data.experience); word(p.data.killType); word(p.data.damageType);
}
// Stores exact canonical-sized records, never live pointers. V10.1 may hold
// validated restored records before their private working file is needed.
// Clear on game reset/working PRO cleanup, never on ordinary cache eviction.
class O3dsPartySnapshots {
public:
    void clear() { memset(entries_, 0, sizeof(entries_)); }
    const unsigned char* find(int pid, const char* path) const {
        for (const auto& e : entries_) if (e.pid == pid && e.path[0] && strcmp(e.path, path) == 0) return e.bytes;
        return nullptr;
    }
    void forget(int pid) { for (auto& e : entries_) if (e.pid == pid) e.path[0] = 0; }
    bool remember(int pid, const char* path, const unsigned char* bytes, size_t size, bool deferred = false) {
        // Do not evict an unwritten record or overwrite a different PID's path.
        if (!deferred) forget(pid);
        if (!path || !*path || !bytes || size != O3DS_CRITTER_PROTO_BYTES || strlen(path) >= sizeof(entries_[0].path)) return false;
        uint32_t diskPid = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | bytes[3];
        if (diskPid != static_cast<uint32_t>(pid)) return false;
        for (const auto& e : entries_) if (e.path[0] && e.pid != pid && compat_stricmp(e.path, path) == 0) return false;
        forget(pid);
        for (auto& e : entries_) if (!e.path[0]) { e.pid = pid; strcpy(e.path, path); memcpy(e.bytes, bytes, size); e.deferred = deferred; return true; }
        return false;
    }
    const char* pendingPath(int pid) const {
        for (const auto& e : entries_) if (e.pid == pid && e.path[0] && e.deferred) return e.path;
        return nullptr;
    }
    void markMaterialized(int pid) {
        for (auto& e : entries_) if (e.pid == pid && e.path[0]) e.deferred = false;
    }
private:
    struct Entry { int pid; char path[128]; unsigned char bytes[O3DS_CRITTER_PROTO_BYTES]; bool deferred; };
    Entry entries_[64]{};
};
}
#endif
