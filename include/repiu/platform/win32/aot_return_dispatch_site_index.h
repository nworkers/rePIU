#pragma once

#include <cstdint>
#include <vector>

namespace repiu::platform::win32
{

struct Win32AotCodeCachePlacement;

struct Win32AotReturnDispatchSiteIndex
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
    Win32AotCodeCachePlacement* placement);
void EnsureAotReturnDispatchSiteIndex(
    Win32AotCodeCachePlacement* placement);
void InvalidateAotReturnDispatchSiteIndex(
    Win32AotCodeCachePlacement* placement);
AotReturnDispatchSiteLookup LookupAotReturnDispatchSiteIndex(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t miss_offset);

}  // namespace repiu::platform::win32
