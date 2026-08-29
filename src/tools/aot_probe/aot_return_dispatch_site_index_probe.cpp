#include "aot_return_dispatch_site_index_probe.h"

#include "repiu/engine/aot_code_cache.h"
#include "repiu/engine/aot_return_dispatch_site_index.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::engine::AotReturnDispatchSiteLookup;
using repiu::engine::EnsureAotReturnDispatchSiteIndex;
using repiu::engine::InvalidateAotReturnDispatchSiteIndex;
using repiu::engine::LookupAotReturnDispatchSiteIndex;
using repiu::engine::AotCodeCachePlacement;

void AddSite(AotCodeCachePlacement* placement,
             std::uint32_t guest_source,
             std::uint32_t miss_offset)
{
    repiu::runtime::AotDbtReturnDispatchSite site;
    site.guest_source = guest_source;
    site.miss_cache_offset = miss_offset;
    placement->dbt_return_dispatch_sites.push_back(site);
}

bool ReferenceFindSite(const AotCodeCachePlacement& placement,
                       std::uint32_t miss_offset,
                       std::uint32_t* site_index)
{
    for (std::size_t index = 0;
         index < placement.dbt_return_dispatch_sites.size(); ++index)
    {
        if (placement.dbt_return_dispatch_sites[index].miss_cache_offset ==
            miss_offset)
        {
            *site_index = static_cast<std::uint32_t>(index);
            return true;
        }
    }
    return false;
}

bool Agrees(const AotCodeCachePlacement& placement,
            const std::vector<std::uint32_t>& queries)
{
    for (const std::uint32_t query : queries)
    {
        std::uint32_t expected_index = 0;
        const bool expected =
            ReferenceFindSite(placement, query, &expected_index);
        const AotReturnDispatchSiteLookup actual =
            LookupAotReturnDispatchSiteIndex(placement, query);
        if (!actual.usable || actual.found != expected ||
            (expected && actual.site_index != expected_index))
        {
            return false;
        }
    }
    return true;
}

bool BuildCollisionPair(std::uint32_t* first, std::uint32_t* second)
{
    constexpr std::uint32_t kKnuth = 2654435761U;
    constexpr std::uint32_t kMask = 4096U - 1U;
    *first = 0x00010000U;
    for (std::uint32_t candidate = 0x00010010U;
         candidate < 0x00400000U; candidate += 0x10U)
    {
        if (((candidate * kKnuth) & kMask) ==
            ((*first * kKnuth) & kMask))
        {
            *second = candidate;
            return true;
        }
    }
    return false;
}

}  // namespace

bool RunAotReturnDispatchSiteIndexProbe()
{
    AotCodeCachePlacement ordinary;
    AddSite(&ordinary, 0x04010000U, 0x100U);
    AddSite(&ordinary, 0x04020000U, 0x280U);
    AddSite(&ordinary, 0x04030000U, 0x900U);
    EnsureAotReturnDispatchSiteIndex(&ordinary);
    const bool ordinary_ok = Agrees(
        ordinary, {0x100U, 0x280U, 0x900U, 0U, 0xFFFFFFFFU});

    AotCodeCachePlacement duplicate;
    AddSite(&duplicate, 0x04040000U, 0x400U);
    AddSite(&duplicate, 0x04050000U, 0x400U);
    EnsureAotReturnDispatchSiteIndex(&duplicate);
    const AotReturnDispatchSiteLookup duplicate_lookup =
        LookupAotReturnDispatchSiteIndex(duplicate, 0x400U);
    const bool duplicate_ok = duplicate_lookup.usable &&
        duplicate_lookup.found && duplicate_lookup.site_index == 0U;

    std::uint32_t collision_a = 0;
    std::uint32_t collision_b = 0;
    const bool collision_built =
        BuildCollisionPair(&collision_a, &collision_b);
    AotCodeCachePlacement collision;
    if (collision_built)
    {
        AddSite(&collision, 0x04060000U, collision_a);
        AddSite(&collision, 0x04070000U, collision_b);
        EnsureAotReturnDispatchSiteIndex(&collision);
    }
    const bool collision_ok = collision_built &&
        Agrees(collision, {collision_a, collision_b});

    AddSite(&ordinary, 0x04080000U, 0xA00U);
    const bool stale_ok =
        !LookupAotReturnDispatchSiteIndex(ordinary, 0xA00U).usable;
    EnsureAotReturnDispatchSiteIndex(&ordinary);
    const bool rebuild_ok = Agrees(ordinary, {0x100U, 0xA00U}) &&
        ordinary.return_dispatch_site_index.rebuild_count == 2U;

    InvalidateAotReturnDispatchSiteIndex(&ordinary);
    const bool invalidated_ok =
        !LookupAotReturnDispatchSiteIndex(ordinary, 0x100U).usable;

    AotCodeCachePlacement empty;
    EnsureAotReturnDispatchSiteIndex(&empty);
    const bool empty_ok =
        !LookupAotReturnDispatchSiteIndex(empty, 0x100U).usable;

    const bool all = ordinary_ok && duplicate_ok && collision_ok && stale_ok &&
        rebuild_ok && invalidated_ok && empty_ok;
    std::cout
        << "return_dispatch_site_index_ordinary="
        << (ordinary_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_duplicate="
        << (duplicate_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_collision="
        << (collision_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_stale="
        << (stale_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_rebuild="
        << (rebuild_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_invalidated="
        << (invalidated_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_empty="
        << (empty_ok ? "true" : "false")
        << "\nreturn_dispatch_site_index_all=" << (all ? "true" : "false")
        << "\n";
    return all;
}

}  // namespace repiu::tools
