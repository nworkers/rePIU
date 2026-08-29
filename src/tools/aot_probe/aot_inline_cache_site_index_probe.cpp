#include "aot_inline_cache_site_index_probe.h"

#include "repiu/engine/aot_code_cache.h"
#include "repiu/engine/aot_inline_cache_site_index.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::engine::AotInlineCacheSiteLookup;
using repiu::engine::EnsureAotInlineCacheSiteIndex;
using repiu::engine::InvalidateAotInlineCacheSiteIndex;
using repiu::engine::LookupAotInlineCacheSiteIndex;
using repiu::engine::AotCodeCachePlacement;

// The pre-Task-479 lookup, kept verbatim as the reference oracle. Any divergence
// from this is a defect regardless of which answer looks nicer.
bool ReferenceFindSite(const AotCodeCachePlacement& placement,
                       std::uint32_t miss_offset,
                       std::uint32_t* site_index)
{
    for (std::size_t index = 0;
         index < placement.indirect_inline_cache_sites.size(); ++index)
    {
        const repiu::runtime::AotIndirectInlineCacheSite& site =
            placement.indirect_inline_cache_sites[index];
        if (miss_offset == site.miss_cache_offset ||
            miss_offset == site.miss_cache_offset + 1U)
        {
            *site_index = static_cast<std::uint32_t>(index);
            return true;
        }
    }
    return false;
}

void AddSite(AotCodeCachePlacement* placement,
             std::uint32_t guest_source,
             std::uint32_t miss_cache_offset)
{
    repiu::runtime::AotIndirectInlineCacheSite site;
    site.guest_source = guest_source;
    site.cache_offset = miss_cache_offset > 0x20U
        ? miss_cache_offset - 0x20U : 0U;
    site.miss_cache_offset = miss_cache_offset;
    placement->indirect_inline_cache_sites.push_back(site);
}

// Every key of every site plus both neighbours, so an off-by-one in either
// direction is caught rather than sampled past.
bool AgreesEverywhere(const AotCodeCachePlacement& placement)
{
    std::vector<std::uint32_t> queries;
    for (const repiu::runtime::AotIndirectInlineCacheSite& site :
         placement.indirect_inline_cache_sites)
    {
        queries.push_back(site.miss_cache_offset);
        queries.push_back(site.miss_cache_offset + 1U);
        queries.push_back(site.miss_cache_offset + 2U);
        queries.push_back(site.miss_cache_offset - 1U);
    }
    queries.push_back(0U);
    queries.push_back(1U);
    queries.push_back(0xFFFFFFFFU);
    queries.push_back(0x00A5A5A5U);

    for (const std::uint32_t query : queries)
    {
        std::uint32_t expected = 0xDEADBEEFU;
        const bool expected_found =
            ReferenceFindSite(placement, query, &expected);
        const AotInlineCacheSiteLookup actual =
            LookupAotInlineCacheSiteIndex(placement, query);
        if (!actual.usable)
        {
            // A stale index is allowed to decline, and the patch path then runs
            // the scan; it is never allowed to answer wrongly.
            continue;
        }
        if (actual.found != expected_found)
        {
            return false;
        }
        if (expected_found && actual.site_index != expected)
        {
            return false;
        }
    }
    return true;
}

AotCodeCachePlacement MakeBasePlacement()
{
    AotCodeCachePlacement placement;
    placement.valid = true;
    placement.placed = true;
    placement.base_address = 0x0D770000U;
    return placement;
}

// Two miss offsets that land in the same bucket for every table width this probe
// reaches, so chain traversal must still compare the key.
bool BuildCollisionPair(std::uint32_t* first, std::uint32_t* second)
{
    constexpr std::uint32_t kKnuth = 2654435761U;
    constexpr std::uint32_t kWidestMask = 4096U - 1U;
    *first = 0x00010000U;
    for (std::uint32_t candidate = 0x00010010U;
         candidate < 0x00400000U; candidate += 0x10U)
    {
        if (((candidate * kKnuth) & kWidestMask) ==
            ((*first * kKnuth) & kWidestMask))
        {
            *second = candidate;
            return true;
        }
    }
    return false;
}

}  // namespace

bool RunAotInlineCacheSiteIndexProbe()
{
    // 1. Ordinary spacing: each site owns two keys and nothing else.
    AotCodeCachePlacement spaced = MakeBasePlacement();
    AddSite(&spaced, 0x04010000U, 0x00000100U);
    AddSite(&spaced, 0x04020000U, 0x00000240U);
    AddSite(&spaced, 0x04030000U, 0x00001000U);
    EnsureAotInlineCacheSiteIndex(&spaced);
    const bool spaced_ok = AgreesEverywhere(spaced) &&
        spaced.inline_cache_site_index.indexed_site_count == 3U &&
        LookupAotInlineCacheSiteIndex(spaced, 0x00000101U).site_index == 0U;

    // 2. Adjacent miss offsets, both orders. The scan stopped at the first site
    //    in array order, so the shared key must resolve to the lower index in
    //    both layouts rather than to whichever site the chain reaches first.
    AotCodeCachePlacement adjacent_forward = MakeBasePlacement();
    AddSite(&adjacent_forward, 0x04040000U, 0x00000200U);
    AddSite(&adjacent_forward, 0x04050000U, 0x00000201U);
    EnsureAotInlineCacheSiteIndex(&adjacent_forward);
    AotCodeCachePlacement adjacent_reverse = MakeBasePlacement();
    AddSite(&adjacent_reverse, 0x04060000U, 0x00000201U);
    AddSite(&adjacent_reverse, 0x04070000U, 0x00000200U);
    EnsureAotInlineCacheSiteIndex(&adjacent_reverse);
    const bool adjacent_ok = AgreesEverywhere(adjacent_forward) &&
        AgreesEverywhere(adjacent_reverse) &&
        LookupAotInlineCacheSiteIndex(adjacent_forward, 0x00000201U)
                .site_index == 0U &&
        LookupAotInlineCacheSiteIndex(adjacent_reverse, 0x00000201U)
                .site_index == 0U;

    // 3. Duplicate key: the lowest index wins, as the scan's break did.
    AotCodeCachePlacement duplicate = MakeBasePlacement();
    AddSite(&duplicate, 0x04080000U, 0x00000400U);
    AddSite(&duplicate, 0x04090000U, 0x00000400U);
    EnsureAotInlineCacheSiteIndex(&duplicate);
    const bool duplicate_ok = AgreesEverywhere(duplicate) &&
        LookupAotInlineCacheSiteIndex(duplicate, 0x00000400U).site_index == 0U;

    // 4. Forced hash collision between two distinct miss offsets.
    std::uint32_t collision_a = 0;
    std::uint32_t collision_b = 0;
    const bool collision_built =
        BuildCollisionPair(&collision_a, &collision_b);
    AotCodeCachePlacement collision = MakeBasePlacement();
    if (collision_built)
    {
        AddSite(&collision, 0x040A0000U, collision_a);
        AddSite(&collision, 0x040B0000U, collision_b);
        EnsureAotInlineCacheSiteIndex(&collision);
    }
    const bool collisions = collision_built && AgreesEverywhere(collision) &&
        LookupAotInlineCacheSiteIndex(collision, collision_b).site_index == 1U;

    // 5. Offset zero and the unsigned wrap around it: a site at offset zero owns
    //    key zero, and a site at 0xFFFFFFFF owns key zero as well through the
    //    scan's `+ 1U` wrap, so the index must reproduce that rather than skip
    //    the second probe.
    AotCodeCachePlacement wrapped = MakeBasePlacement();
    AddSite(&wrapped, 0x040C0000U, 0xFFFFFFFFU);
    AddSite(&wrapped, 0x040D0000U, 0x00000000U);
    EnsureAotInlineCacheSiteIndex(&wrapped);
    const bool wrap_ok = AgreesEverywhere(wrapped) &&
        LookupAotInlineCacheSiteIndex(wrapped, 0x00000000U).site_index == 0U &&
        LookupAotInlineCacheSiteIndex(wrapped, 0x00000001U).site_index == 1U;

    // 6. Append after the index is live, past the initial table width, with the
    //    rebuild driven only by the count changing.
    AotCodeCachePlacement appended = MakeBasePlacement();
    AddSite(&appended, 0x04100000U, 0x00002000U);
    EnsureAotInlineCacheSiteIndex(&appended);
    bool append_agrees = AgreesEverywhere(appended);
    for (std::uint32_t step = 0; step < 300U && append_agrees; ++step)
    {
        AddSite(&appended, 0x04100000U + step * 0x40U,
                0x00002000U + (step + 1U) * 0x20U);
        // The lookup must decline while the new site is unindexed, and answer
        // again once the count is reconciled.
        append_agrees =
            !LookupAotInlineCacheSiteIndex(appended, 0x00002000U).usable;
        EnsureAotInlineCacheSiteIndex(&appended);
        // Full agreement every tenth step and on the last one: the oracle is a
        // scan per query, so checking every step would make the probe quadratic
        // without covering another shape.
        if (step % 10U == 9U || step + 1U == 300U)
        {
            append_agrees = append_agrees && AgreesEverywhere(appended);
        }
    }
    const bool rebuilt = append_agrees &&
        appended.inline_cache_site_index.indexed_site_count == 301U &&
        appended.inline_cache_site_index.rebuild_count == 301U;

    // 7. Invalidated index declines, so the caller keeps the scan's answer.
    InvalidateAotInlineCacheSiteIndex(&appended);
    const bool invalidated =
        !LookupAotInlineCacheSiteIndex(appended, 0x00002020U).usable &&
        AgreesEverywhere(appended);

    // 8. No sites at all: nothing to index, and the lookup must not claim an
    //    answer for a placement the patch path will scan.
    AotCodeCachePlacement empty = MakeBasePlacement();
    EnsureAotInlineCacheSiteIndex(&empty);
    const bool empty_ok =
        !LookupAotInlineCacheSiteIndex(empty, 0x00000100U).usable;

    // 9. A placement built directly, the way several probes build one, is never
    //    answered from a stale index.
    AotCodeCachePlacement unindexed = MakeBasePlacement();
    AddSite(&unindexed, 0x04200000U, 0x00003000U);
    const bool unindexed_ok =
        !LookupAotInlineCacheSiteIndex(unindexed, 0x00003000U).usable &&
        AgreesEverywhere(unindexed);

    const bool all = spaced_ok && adjacent_ok && duplicate_ok && collisions &&
        wrap_ok && append_agrees && rebuilt && invalidated && empty_ok &&
        unindexed_ok;

    std::cout
        << "inline_cache_site_index_spaced=" << (spaced_ok ? "true" : "false")
        << "\ninline_cache_site_index_adjacent="
        << (adjacent_ok ? "true" : "false")
        << "\ninline_cache_site_index_duplicate="
        << (duplicate_ok ? "true" : "false")
        << "\ninline_cache_site_index_collisions="
        << (collisions ? "true" : "false")
        << "\ninline_cache_site_index_offset_wrap="
        << (wrap_ok ? "true" : "false")
        << "\ninline_cache_site_index_append="
        << (append_agrees ? "true" : "false")
        << "\ninline_cache_site_index_rebuild="
        << (rebuilt ? "true" : "false")
        << "\ninline_cache_site_index_invalidated_fallback="
        << (invalidated ? "true" : "false")
        << "\ninline_cache_site_index_empty=" << (empty_ok ? "true" : "false")
        << "\ninline_cache_site_index_unindexed="
        << (unindexed_ok ? "true" : "false")
        << "\ninline_cache_site_index_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
