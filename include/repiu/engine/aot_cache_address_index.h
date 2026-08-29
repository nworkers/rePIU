#pragma once

#include <cstdint>
#include <vector>

namespace repiu::engine
{

struct AotCodeCachePlacement;

// O(1) guest-address lookup over AotCodeCachePlacement::address_map
// (Task 324).
//
// Task 323 measured the previous linear scan at 87.75% of kAotResume, averaging
// 1,047,784 ticks per call over 26,710+ records.
//
// address_map holds the same guest address more than once, one entry per
// regenerated translation, so this cannot be a plain key-to-value map. The two
// rules it must reproduce exactly are:
//
//   * guest address present in retired_guest_addresses -> newest ACTIVE entry
//   * otherwise                                        -> oldest entry, active
//                                                         flag irrelevant
//
// Chains link newest-first, so the newest active entry is the first chain match
// with the active flag set, and the oldest entry is the last chain match.
// Buckets mix guest addresses under hash collision, so traversal always
// re-compares guest_address.
//
// The index is a cache, never a precondition: when indexed_entry_count does not
// equal address_map.size() the caller falls back to the original scan, so a
// placement built without the update hooks degrades to slow rather than wrong.
//
// See docs/design/20260727-324-aot-cache-address-hash-index.md.
struct AotCacheAddressIndex
{
    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFU;

    // Valid only while equal to address_map.size().
    std::uint32_t indexed_entry_count = 0;
    // Power-of-two sized; each element is a chain head map index or
    // kInvalidIndex.
    std::vector<std::uint32_t> buckets;
    // Parallel to address_map; next (older) map index in the same bucket.
    std::vector<std::uint32_t> next_in_bucket;

    // Task 334: the reverse direction, cache offset to guest address, which
    // Task 324 left as a linear scan and which Release measurement then found
    // holding 96.00% of the reentry handler at 551,864 ticks per call.
    //
    // Entries are emitted in increasing cache_offset within an image and
    // appended at increasing offsets, so address_map is expected to be sorted
    // by cache_offset. That is verified rather than assumed: the flag is
    // maintained as entries are indexed and a false value falls back to the
    // scan. `max_emitted_length` bounds how far back a longer earlier entry
    // could still cover an offset, which keeps the "first matching entry" rule
    // exact even if two ranges ever overlap.
    bool cache_offset_sorted = true;
    std::uint32_t max_emitted_length = 0;
};

// Discards and rebuilds the whole index from placement->address_map.
void RebuildAotCacheAddressIndex(AotCodeCachePlacement* placement);

// Rebuilds only when the index does not cover address_map. Used as a safety net
// after a batch of appends.
void EnsureAotCacheAddressIndex(AotCodeCachePlacement* placement);

// Drops the index so lookups fall back to the linear scan until a rebuild.
void InvalidateAotCacheAddressIndex(AotCodeCachePlacement* placement);

// Links one newly appended address_map entry in O(1) when it extends the
// indexed range. Any other shape -- an out-of-order registration, or a map
// replaced wholesale -- invalidates instead of rebuilding, so callers that
// register n entries in a loop stay O(n) rather than O(n^2); the trailing
// EnsureAotCacheAddressIndex restores the index once.
void AppendAotCacheAddressIndexEntry(AotCodeCachePlacement* placement,
                                     std::uint32_t map_index);

// Returns false when the index is stale, when no entry matches, or when
// newest_active is requested and no active entry matches. `newest_active`
// selects the retired-generation rule; false selects the oldest-entry rule.
bool LookupAotCacheAddressIndex(const AotCodeCachePlacement& placement,
                                std::uint32_t guest_address,
                                bool newest_active,
                                std::uint32_t* map_index);

// Task 334. Returns the same entry the linear scan would return -- the lowest
// map index whose `[cache_offset, cache_offset + emitted_length)` contains the
// offset -- in O(log n). Reports whether the index was usable at all, separately
// from whether an entry matched, so the caller can fall back rather than treat a
// stale index as "not found".
struct AotGuestAddressLookup
{
    bool usable = false;
    bool found = false;
    std::uint32_t map_index = 0;
};

AotGuestAddressLookup LookupAotGuestAddressIndex(
    const AotCodeCachePlacement& placement,
    std::uint32_t cache_offset);

}  // namespace repiu::engine
