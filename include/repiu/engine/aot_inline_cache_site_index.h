#pragma once

#include <cstdint>
#include <vector>

namespace repiu::engine
{

struct AotCodeCachePlacement;

// O(1) miss-offset lookup over
// AotCodeCachePlacement::indirect_inline_cache_sites (Task 479).
//
// Task 478 measured the patch path at about 75,100 cycles per call over
// 1,203,695 calls -- roughly 28.7% of guest-run on pumpit8 -- with the site
// lookup a linear scan over 8,019 entries of about 44 to 48 bytes each. The scan
// stops at the first match, so an average patch streamed half of a ~370 KB array.
// This is the same defect shape Task 334 removed from FindAotGuestAddress.
//
// Keys are exact rather than intervals: a miss address selects the site whose
// miss_cache_offset equals the offset or is one below it, so chains are hashed on
// miss_cache_offset and a lookup probes two keys. Sites may in principle repeat a
// key, and the scan took the first in array order, so traversal keeps the lowest
// matching site index rather than the first one reached.
//
// The index is a cache, never a precondition: when indexed_site_count does not
// equal the site count, the lookup reports itself unusable and the caller runs
// the original scan, so a placement built outside the maintained paths degrades
// to slow rather than wrong.
//
// See docs/design/20260814-479-inline-cache-site-index.md.
struct AotInlineCacheSiteIndex
{
    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFU;

    // Valid only while equal to indirect_inline_cache_sites.size().
    std::uint32_t indexed_site_count = 0;
    // Power-of-two sized; each element is a chain head site index or
    // kInvalidIndex.
    std::vector<std::uint32_t> buckets;
    // Parallel to the site array; next site index in the same bucket.
    std::vector<std::uint32_t> next_in_bucket;

    // Reported so a run cannot silently fall back to scanning: lookups answered
    // by the index, lookups that had to run the scan, and rebuilds.
    std::uint32_t lookup_count = 0;
    std::uint32_t fallback_scan_count = 0;
    std::uint32_t rebuild_count = 0;
};

// Discards and rebuilds the whole index from
// placement->indirect_inline_cache_sites.
void RebuildAotInlineCacheSiteIndex(AotCodeCachePlacement* placement);

// Rebuilds only when the index does not cover the site array. Sites are assigned
// wholesale at placement and appended by dynamic translation, and are never
// removed, so a count comparison is a sufficient staleness test.
void EnsureAotInlineCacheSiteIndex(AotCodeCachePlacement* placement);

// Drops the index so lookups fall back to the linear scan until a rebuild.
void InvalidateAotInlineCacheSiteIndex(AotCodeCachePlacement* placement);

// Returns the same site the linear scan would return -- the lowest site index
// whose miss_cache_offset equals `miss_offset` or `miss_offset - 1` -- in O(1).
// Reports whether the index was usable at all, separately from whether a site
// matched, so the caller can fall back rather than treat a stale index as
// "not found".
struct AotInlineCacheSiteLookup
{
    bool usable = false;
    bool found = false;
    std::uint32_t site_index = 0;
};

AotInlineCacheSiteLookup LookupAotInlineCacheSiteIndex(
    const AotCodeCachePlacement& placement,
    std::uint32_t miss_offset);

}  // namespace repiu::engine
