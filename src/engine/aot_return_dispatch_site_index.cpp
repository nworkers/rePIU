#include "repiu/engine/aot_return_dispatch_site_index.h"

#include "repiu/engine/aot_code_cache_win32.h"

#include <algorithm>

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kMinimumBucketCount = 64U;

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

}  // namespace

void RebuildAotReturnDispatchSiteIndex(
    Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    Win32AotReturnDispatchSiteIndex& index =
        placement->return_dispatch_site_index;
    const std::uint32_t site_count = static_cast<std::uint32_t>(
        placement->dbt_return_dispatch_sites.size());
    index.buckets.assign(RoundUpToPowerOfTwo(site_count),
                         Win32AotReturnDispatchSiteIndex::kInvalidIndex);
    index.next_in_bucket.assign(
        site_count, Win32AotReturnDispatchSiteIndex::kInvalidIndex);

    const std::uint32_t bucket_count =
        static_cast<std::uint32_t>(index.buckets.size());
    for (std::uint32_t site_index = 0; site_index < site_count; ++site_index)
    {
        const std::uint32_t bucket = HashMissOffset(
            placement->dbt_return_dispatch_sites[site_index]
                .miss_cache_offset,
            bucket_count);
        index.next_in_bucket[site_index] = index.buckets[bucket];
        index.buckets[bucket] = site_index;
    }
    index.indexed_site_count = site_count;
    ++index.rebuild_count;
}

void EnsureAotReturnDispatchSiteIndex(
    Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    if (placement->return_dispatch_site_index.indexed_site_count !=
        static_cast<std::uint32_t>(
            placement->dbt_return_dispatch_sites.size()))
    {
        RebuildAotReturnDispatchSiteIndex(placement);
    }
}

void InvalidateAotReturnDispatchSiteIndex(
    Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    Win32AotReturnDispatchSiteIndex& index =
        placement->return_dispatch_site_index;
    index.indexed_site_count = 0;
    index.buckets.clear();
    index.next_in_bucket.clear();
}

AotReturnDispatchSiteLookup LookupAotReturnDispatchSiteIndex(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t miss_offset)
{
    AotReturnDispatchSiteLookup result;
    const Win32AotReturnDispatchSiteIndex& index =
        placement.return_dispatch_site_index;
    if (index.buckets.empty() ||
        index.indexed_site_count != static_cast<std::uint32_t>(
            placement.dbt_return_dispatch_sites.size()) ||
        index.next_in_bucket.size() != index.indexed_site_count)
    {
        return result;
    }
    result.usable = true;

    const std::uint32_t bucket_count =
        static_cast<std::uint32_t>(index.buckets.size());
    std::uint32_t candidate =
        index.buckets[HashMissOffset(miss_offset, bucket_count)];
    std::uint32_t visited = 0;
    std::uint32_t lowest = Win32AotReturnDispatchSiteIndex::kInvalidIndex;
    while (candidate != Win32AotReturnDispatchSiteIndex::kInvalidIndex &&
           candidate < index.indexed_site_count &&
           visited <= index.indexed_site_count)
    {
        if (placement.dbt_return_dispatch_sites[candidate]
                .miss_cache_offset == miss_offset)
        {
            lowest = std::min(lowest, candidate);
        }
        candidate = index.next_in_bucket[candidate];
        ++visited;
    }
    result.found = lowest != Win32AotReturnDispatchSiteIndex::kInvalidIndex;
    result.site_index = result.found ? lowest : 0U;
    return result;
}

}  // namespace repiu::engine
