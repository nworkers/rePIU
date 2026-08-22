#ifndef REPIU_RUNTIME_AOT_DIRECT_RETURN_TABLE_H_
#define REPIU_RUNTIME_AOT_DIRECT_RETURN_TABLE_H_

#include <cstdint>
#include <vector>

namespace repiu::runtime
{

// Task 499: a flat guest-target to cache-target memo the translated return path
// reads directly, so a megamorphic return resolves without crossing to the host.
//
// Task 482 measured 99.4% of return-policy observations as megamorphic bypasses:
// nearly every return happens at a site that cannot converge on the four-entry
// PIC, and all it ultimately does is map one guest target to one cache target.
// That lookup costs 221-226 cycles inside ResolveAotTransferTarget, wrapped in
// about 900 cycles of reaching the host and coming back.
//
// A table hit is semantically identical to a PIC hit -- both skip the host, the
// call-depth bookkeeping, the telemetry, and the trap-flag cleanup -- so the
// only obligation this structure adds is that an entry lives exactly as long as
// a PIC entry would. See
// docs/design/20260822-499-megamorphic-direct-return-table.md.
struct AotDirectReturnEntry
{
    // Zero marks an empty slot. A zero guest return target is pathological and
    // must reach the host for its existing evidence dump, so the two meanings
    // never collide.
    std::uint32_t guest_key = 0;
    std::uint32_t cache_target = 0;
};

constexpr std::uint32_t kAotDirectReturnTableMinimumBits = 8U;
constexpr std::uint32_t kAotDirectReturnTableMaximumBits = 18U;
// Task 499 measured pumpit8 at 8,192 entries and found it thrashing: 211,909
// of 214,750 inserts were overwrites, and every overwrite costs a later host
// round trip. At 32,768 the working set fits -- 33 overwrites in the same
// scene, and host return dispatches fall from 214,790 to 2,932. The resident
// footprint is unchanged either way, since only the entries actually touched
// occupy cache lines; the larger table only stops distinct targets from
// colliding.
constexpr std::uint32_t kDefaultAotDirectReturnTableBits = 15U;

// Direct-mapped and shared by every return site, so two sites returning to the
// same target reuse one entry. Collisions overwrite: a miss is just the existing
// path, and the existing path is correct.
struct AotDirectReturnTable
{
    std::vector<AotDirectReturnEntry> entries;
    std::uint32_t mask = 0;
    std::uint64_t insert_count = 0;
    std::uint64_t overwrite_count = 0;
    std::uint64_t clear_count = 0;
    std::uint64_t cleared_entry_count = 0;
    // Written by generated code through an absolute address, so it is a plain
    // 32-bit word rather than an atomic: only the guest thread ever increments
    // it, and the host reads it after that thread has stopped.
    std::uint32_t hit_count = 0;
};

// Parses REPIU_AOT_DIRECT_RETURN_TABLE_BITS. Unset, empty, and unparsable
// values keep the default; a parsed value is clamped to the supported range,
// so a typo costs table size rather than correctness.
[[nodiscard]] std::uint32_t ResolveAotDirectReturnTableBits(
    const char* setting);

// Must match the four instructions the emitter produces exactly.
[[nodiscard]] std::uint32_t AotDirectReturnTableIndex(std::uint32_t guest_target,
                                                      std::uint32_t mask);

// Clamps to [kAotDirectReturnTableMinimumBits, kAotDirectReturnTableMaximumBits]
// and allocates 2^bits empty entries. Existing contents are discarded.
void ResetAotDirectReturnTable(AotDirectReturnTable* table, std::uint32_t bits);

// Records one validated mapping. Rejects a zero key or target, and reports
// whether it displaced a different key so the shutdown summary can show how hard
// the table is being contended.
bool InsertAotDirectReturnEntry(AotDirectReturnTable* table,
                                std::uint32_t guest_target,
                                std::uint32_t cache_target);

// Host-side equivalent of the emitted probe, used by the probe suite as the
// oracle for what generated code must do.
[[nodiscard]] bool LookupAotDirectReturnEntry(const AotDirectReturnTable& table,
                                              std::uint32_t guest_target,
                                              std::uint32_t* cache_target);

// Empties every slot without reallocating. Called wherever inline-cache guards
// are reset, which is the one place a guest-to-cache mapping can change.
void ClearAotDirectReturnTable(AotDirectReturnTable* table);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_DIRECT_RETURN_TABLE_H_
