#pragma once

#include <cstdint>
#include <vector>

namespace repiu::engine
{

struct AotCodeCachePlacement;

struct AotReturnDispatchSiteIndex
{
    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFU;

    std::uint32_t indexed_site_count = 0;
    std::vector<std::uint32_t> buckets;
    std::vector<std::uint32_t> next_in_bucket;
    std::uint32_t lookup_count = 0;
    std::uint32_t fallback_scan_count = 0;
    std::uint32_t rebuild_count = 0;
};

struct AotReturnDispatchSiteLookup
{
    bool usable = false;
    bool found = false;
    std::uint32_t site_index = 0;
};

void RebuildAotReturnDispatchSiteIndex(
    AotCodeCachePlacement* placement);
void EnsureAotReturnDispatchSiteIndex(
    AotCodeCachePlacement* placement);
void InvalidateAotReturnDispatchSiteIndex(
    AotCodeCachePlacement* placement);
AotReturnDispatchSiteLookup LookupAotReturnDispatchSiteIndex(
    const AotCodeCachePlacement& placement,
    std::uint32_t miss_offset);

}  // namespace repiu::engine
