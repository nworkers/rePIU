#pragma once

#include "repiu/runtime/aot_code_cache.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace repiu::engine
{

// Fixups that jump or call into Glide gate code, keyed by their guest target
// (Task 773).
//
// ActivateGlideGateDirectTarget used to walk every fixup -- about 100,000 on
// pumpit1 -- on each Glide call that reached it through a cache boundary
// breakpoint, which on Linux i386 is every Glide call. With the whole-cache
// protection change that followed, one call cost about 160 us and the guest
// thread spent about 88% of its time there.
//
// Only fixups whose target is at or above `minimum_target` (the start of gate
// code) and whose kind the activation rewrites are kept, so the map stays
// small. Each list follows fixup array order, the order the scan collected in.
//
// The fixup array is assigned wholesale at placement and afterwards only
// appended to, so the index grows by indexing the tail it has not seen. A
// shorter array or a different bound means it no longer describes the array,
// and it is rebuilt.
//
// See docs/design/20261005-773-glide-gate-relink-cost.md.
struct AotGlideGateFixupIndex
{
    // Valid only while not greater than the fixup count.
    std::uint32_t indexed_fixup_count = 0;
    std::uint32_t minimum_target = 0;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>
        patch_offsets_by_target;
    std::uint32_t rebuild_count = 0;
};

// The kinds ActivateGlideGateDirectTarget rewrites to the gate.
bool IsGlideGateRelinkFixupKind(runtime::AotFixupKind kind);

// Indexes the fixups appended since the last call, or rebuilds the index when
// it does not describe `fixups` under `minimum_target`.
void EnsureAotGlideGateFixupIndex(
    const std::vector<runtime::AotCodeCacheFixup>& fixups,
    std::uint32_t minimum_target,
    AotGlideGateFixupIndex* index);

// The patch offsets of the fixups going to `target`, or nullptr when there are
// none.
const std::vector<std::uint32_t>* FindAotGlideGateFixupOffsets(
    const AotGlideGateFixupIndex& index,
    std::uint32_t target);

struct GlideGateFixupWrite
{
    std::uint32_t cache_patch_offset = 0;
    std::int32_t displacement = 0;
};

// For each offset, the rel32 that sends that slot to `direct_target`, computed
// against `base_address` as the cache really sits. Slots that already hold it
// are left out. `cache_bytes` is where the cache's bytes are read; in the
// engine it is `base_address` itself.
//
// `wraps_at_32_bits` says the slot runs with a 32-bit instruction pointer (the
// direct model), where the target is (next + rel32) modulo 2^32 and every
// address is in reach. Without it a slot more than 2 GiB from its target is
// left out. Until Task 773 that limit was applied on every host, so on Linux
// i386, whose cache sits near 0xE8000000 with the gates near 0x01000000, no
// slot was ever rewritten and every Glide call took the boundary breakpoint.
void CollectGlideGateFixupWrites(
    const std::uint8_t* cache_bytes,
    std::uint32_t base_address,
    const std::vector<std::uint32_t>& offsets,
    std::uint32_t direct_target,
    bool wraps_at_32_bits,
    std::vector<GlideGateFixupWrite>* writes);

}  // namespace repiu::engine
