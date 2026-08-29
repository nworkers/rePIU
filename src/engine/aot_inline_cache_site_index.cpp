#include "repiu/engine/aot_inline_cache_site_index.h"

#include "repiu/engine/aot_code_cache.h"

#include <algorithm>

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kMinimumBucketCount = 64U;

// Same multiplicative hash the cache address index uses. Miss offsets are dense
// and share low bits across sites, so the multiply matters.
std::uint32_t HashMissOffset(std::uint32_t miss_offset,
                             std::uint32_t bucket_count)
{
    return (miss_offset * 2654435761U) & (bucket_count - 1U);
}

std::uint32_t RoundUpToPowerOfTwo(std::uint32_t value)
{
    std::uint32_t result = kMinimumBucketCount;
    while (result < value && result < 0x40000000U)
    {
        result <<= 1;
    }
    return result;
}

// Keeps the lowest matching site index in `match`, which is what the scan this
// replaces returned: it stopped at the first site in array order.
void CollectLowestMatch(const AotCodeCachePlacement& placement,
                        std::uint32_t key,
                        bool* found,
                        std::uint32_t* match)
{
    const AotInlineCacheSiteIndex& index =
        placement.inline_cache_site_index;
    const std::uint32_t bucket_count =
        static_cast<std::uint32_t>(index.buckets.size());
    std::uint32_t candidate =
        index.buckets[HashMissOffset(key, bucket_count)];
    // Bounded so a corrupted chain cannot spin forever.
    std::uint32_t visited = 0;
    while (candidate != AotInlineCacheSiteIndex::kInvalidIndex &&
           candidate < index.indexed_site_count &&
           visited <= index.indexed_site_count)
    {
        // Collisions put unrelated miss offsets in the same chain.
        if (placement.indirect_inline_cache_sites[candidate]
                .miss_cache_offset == key)
        {
            if (!*found || candidate < *match)
            {
                *match = candidate;
            }
            *found = true;
        }
        candidate = index.next_in_bucket[candidate];
        ++visited;
    }
}

}  // namespace

void RebuildAotInlineCacheSiteIndex(AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    AotInlineCacheSiteIndex& index = placement->inline_cache_site_index;
    const std::uint32_t site_count = static_cast<std::uint32_t>(
        placement->indirect_inline_cache_sites.size());

    index.buckets.assign(RoundUpToPowerOfTwo(site_count),
                         AotInlineCacheSiteIndex::kInvalidIndex);
    index.next_in_bucket.assign(
        site_count, AotInlineCacheSiteIndex::kInvalidIndex);

    const std::uint32_t bucket_count =
        static_cast<std::uint32_t>(index.buckets.size());
    for (std::uint32_t site_index = 0; site_index < site_count; ++site_index)
    {
        const std::uint32_t bucket = HashMissOffset(
            placement->indirect_inline_cache_sites[site_index]
                .miss_cache_offset,
            bucket_count);
        index.next_in_bucket[site_index] = index.buckets[bucket];
        index.buckets[bucket] = site_index;
    }
    index.indexed_site_count = site_count;
    ++index.rebuild_count;
}

void EnsureAotInlineCacheSiteIndex(AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    // Sites are assigned wholesale at placement and appended by dynamic
    // translation, and never removed, so the count answers staleness. Task 324's
    // incremental append linking is not reproduced here: the same profile ran 263
    // translations against 1,203,695 patches, so one O(n) rebuild per append
    // batch is negligible.
    if (placement->inline_cache_site_index.indexed_site_count !=
        static_cast<std::uint32_t>(
            placement->indirect_inline_cache_sites.size()))
    {
        RebuildAotInlineCacheSiteIndex(placement);
    }
}

void InvalidateAotInlineCacheSiteIndex(AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    AotInlineCacheSiteIndex& index = placement->inline_cache_site_index;
    index.indexed_site_count = 0;
    index.buckets.clear();
    index.next_in_bucket.clear();
}

AotInlineCacheSiteLookup LookupAotInlineCacheSiteIndex(
    const AotCodeCachePlacement& placement,
    std::uint32_t miss_offset)
{
    AotInlineCacheSiteLookup result;
    const AotInlineCacheSiteIndex& index =
        placement.inline_cache_site_index;
    if (index.buckets.empty() ||
        index.indexed_site_count !=
            static_cast<std::uint32_t>(
                placement.indirect_inline_cache_sites.size()) ||
        index.next_in_bucket.size() != index.indexed_site_count)
    {
        return result;
    }
    result.usable = true;

    // A miss address selects a site whose miss_cache_offset is the offset itself
    // or one below it, so both keys are probed and the lower site index wins.
    // The second key is computed with the same unsigned wrap the scan's
    // `miss_cache_offset + 1U` has, so offset zero stays equivalent too.
    bool found = false;
    std::uint32_t match = 0;
    CollectLowestMatch(placement, miss_offset, &found, &match);
    CollectLowestMatch(placement, miss_offset - 1U, &found, &match);
    result.found = found;
    result.site_index = match;
    return result;
}

}  // namespace repiu::engine
