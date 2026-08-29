#include "aot_cache_address_index_probe.h"

#include "repiu/engine/aot_cache_address_index.h"
#include "repiu/engine/aot_code_cache.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::engine::EnsureAotCacheAddressIndex;
using repiu::engine::FindAotCacheAddress;
using repiu::engine::InvalidateAotCacheAddressIndex;
using repiu::engine::AotCodeCachePlacement;

// The pre-Task-324 implementation, kept verbatim as the reference oracle. Any
// divergence from this is a defect regardless of which answer looks nicer.
bool ReferenceFindAotCacheAddress(const AotCodeCachePlacement& placement,
                                  std::uint32_t guest_address,
                                  std::uint32_t* cache_address)
{
    if (!placement.placed || cache_address == nullptr)
    {
        return false;
    }
    if (placement.retired_guest_addresses.empty())
    {
        for (const repiu::runtime::AotAddressMapEntry& entry :
             placement.address_map)
        {
            if (entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
        return false;
    }
    const bool has_retired_generation = std::binary_search(
        placement.retired_guest_addresses.begin(),
        placement.retired_guest_addresses.end(), guest_address);
    if (has_retired_generation)
    {
        for (std::size_t index = placement.address_map.size();
             index-- > 0U;)
        {
            const repiu::runtime::AotAddressMapEntry& entry =
                placement.address_map[index];
            if (index < placement.address_map_states.size() &&
                placement.address_map_states[index].active &&
                entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
    }
    else
    {
        for (std::size_t index = 0;
             index < placement.address_map.size(); ++index)
        {
            const repiu::runtime::AotAddressMapEntry& entry =
                placement.address_map[index];
            if (entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
    }
    return false;
}

void AddEntry(AotCodeCachePlacement* placement,
              std::uint32_t guest_address,
              std::uint32_t cache_offset,
              std::uint32_t generation,
              bool active)
{
    placement->address_map.push_back(
        {guest_address, cache_offset, 4U, 4U});
    placement->address_map_states.push_back(
        {generation, active, true});
}

// Compares index and oracle over every guest address in the map plus misses and
// neighbours, so both a wrong hit and a wrong miss are caught.
bool AgreesEverywhere(const AotCodeCachePlacement& placement)
{
    std::vector<std::uint32_t> queries;
    for (const repiu::runtime::AotAddressMapEntry& entry :
         placement.address_map)
    {
        queries.push_back(entry.guest_address);
        queries.push_back(entry.guest_address + 1U);
        queries.push_back(entry.guest_address - 1U);
    }
    queries.push_back(0U);
    queries.push_back(0xFFFFFFFFU);
    queries.push_back(0x12345678U);

    for (const std::uint32_t query : queries)
    {
        std::uint32_t actual = 0xDEADBEEFU;
        std::uint32_t expected = 0xDEADBEEFU;
        const bool actual_found =
            FindAotCacheAddress(placement, query, &actual);
        const bool expected_found =
            ReferenceFindAotCacheAddress(placement, query, &expected);
        if (actual_found != expected_found)
        {
            return false;
        }
        if (expected_found && actual != expected)
        {
            return false;
        }
    }
    return true;
}

// Task 334: the pre-index reverse scan, kept verbatim as the oracle for the
// cache-to-guest direction. First match by map index wins, which is what the
// binary search has to reproduce.
bool ReferenceFindAotGuestAddress(const AotCodeCachePlacement& placement,
                                  std::uint32_t cache_address,
                                  std::uint32_t* guest_address)
{
    if (!placement.placed || guest_address == nullptr ||
        cache_address < placement.base_address)
    {
        return false;
    }
    const std::uint32_t offset = cache_address - placement.base_address;
    for (const repiu::runtime::AotAddressMapEntry& entry :
         placement.address_map)
    {
        if (offset >= entry.cache_offset &&
            offset < entry.cache_offset + entry.emitted_length)
        {
            *guest_address = entry.guest_address;
            return true;
        }
    }
    return false;
}

// Every byte of every entry plus the gaps around them, so an off-by-one at
// either end of a range is caught rather than sampled past.
bool ReverseAgreesEverywhere(const AotCodeCachePlacement& placement)
{
    std::vector<std::uint32_t> queries;
    for (const repiu::runtime::AotAddressMapEntry& entry :
         placement.address_map)
    {
        for (std::uint32_t byte = 0; byte < entry.emitted_length; ++byte)
        {
            queries.push_back(placement.base_address + entry.cache_offset +
                              byte);
        }
        queries.push_back(placement.base_address + entry.cache_offset - 1U);
        queries.push_back(placement.base_address + entry.cache_offset +
                          entry.emitted_length);
    }
    queries.push_back(placement.base_address);
    queries.push_back(placement.base_address - 1U);
    queries.push_back(placement.base_address + 0x7FFFFFU);

    for (const std::uint32_t query : queries)
    {
        std::uint32_t actual = 0xDEADBEEFU;
        std::uint32_t expected = 0xDEADBEEFU;
        const bool actual_found = repiu::engine::FindAotGuestAddress(
            placement, query, &actual);
        const bool expected_found =
            ReferenceFindAotGuestAddress(placement, query, &expected);
        if (actual_found != expected_found)
        {
            return false;
        }
        if (expected_found && actual != expected)
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

// Two guest addresses that land in the same bucket for every table width the
// index can pick, so chain traversal must still compare guest_address.
bool BuildCollisionPair(std::uint32_t* first, std::uint32_t* second)
{
    constexpr std::uint32_t kKnuth = 2654435761U;
    // Widths from the minimum table size up past the sizes this probe reaches.
    constexpr std::uint32_t kWidestMask = 4096U - 1U;
    *first = 0x03010000U;
    for (std::uint32_t candidate = 0x03010004U;
         candidate < 0x03080000U; candidate += 4U)
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

bool RunAotCacheAddressIndexProbe()
{
    // 1. No retired generations: oldest entry wins even when a newer duplicate
    //    exists and even when the oldest is inactive.
    AotCodeCachePlacement no_retired = MakeBasePlacement();
    AddEntry(&no_retired, 0x03010000U, 0x100U, 1U, false);
    AddEntry(&no_retired, 0x03020000U, 0x200U, 1U, true);
    AddEntry(&no_retired, 0x03010000U, 0x300U, 2U, true);
    EnsureAotCacheAddressIndex(&no_retired);
    const bool empty_retired = AgreesEverywhere(no_retired);

    // 2. Retired generation present for one address only, so the two rules run
    //    side by side in the same placement.
    AotCodeCachePlacement mixed = MakeBasePlacement();
    AddEntry(&mixed, 0x03010000U, 0x100U, 1U, false);
    AddEntry(&mixed, 0x03020000U, 0x200U, 1U, true);
    AddEntry(&mixed, 0x03010000U, 0x300U, 2U, true);
    AddEntry(&mixed, 0x03030000U, 0x400U, 1U, true);
    mixed.retired_guest_addresses = {0x03010000U};
    EnsureAotCacheAddressIndex(&mixed);
    const bool mixed_retired = AgreesEverywhere(mixed);

    // 3. Newest entry inactive with an older active one behind it: the newest
    //    ACTIVE entry must win, not simply the newest.
    AotCodeCachePlacement newest_inactive = MakeBasePlacement();
    AddEntry(&newest_inactive, 0x03010000U, 0x100U, 1U, true);
    AddEntry(&newest_inactive, 0x03010000U, 0x200U, 2U, true);
    AddEntry(&newest_inactive, 0x03010000U, 0x300U, 3U, false);
    newest_inactive.retired_guest_addresses = {0x03010000U};
    EnsureAotCacheAddressIndex(&newest_inactive);
    const bool inactive_newest = AgreesEverywhere(newest_inactive);

    // 4. All generations inactive: both implementations must miss.
    AotCodeCachePlacement all_inactive = MakeBasePlacement();
    AddEntry(&all_inactive, 0x03010000U, 0x100U, 1U, false);
    AddEntry(&all_inactive, 0x03010000U, 0x200U, 2U, false);
    all_inactive.retired_guest_addresses = {0x03010000U};
    EnsureAotCacheAddressIndex(&all_inactive);
    const bool inactive_all = AgreesEverywhere(all_inactive) &&
        !FindAotCacheAddress(all_inactive, 0x03010000U, nullptr);

    // 5. Forced hash collision between two distinct guest addresses.
    std::uint32_t collision_a = 0;
    std::uint32_t collision_b = 0;
    const bool collision_built =
        BuildCollisionPair(&collision_a, &collision_b);
    AotCodeCachePlacement collision = MakeBasePlacement();
    if (collision_built)
    {
        AddEntry(&collision, collision_a, 0x100U, 1U, true);
        AddEntry(&collision, collision_b, 0x200U, 1U, true);
        AddEntry(&collision, collision_a, 0x300U, 2U, true);
        collision.retired_guest_addresses = {collision_a};
        EnsureAotCacheAddressIndex(&collision);
    }
    const bool collisions =
        collision_built && AgreesEverywhere(collision);

    // 6. Dynamic append after the index is live, including growth past the
    //    initial table width.
    AotCodeCachePlacement appended = MakeBasePlacement();
    AddEntry(&appended, 0x03040000U, 0x100U, 1U, true);
    EnsureAotCacheAddressIndex(&appended);
    bool append_agrees = AgreesEverywhere(appended);
    for (std::uint32_t step = 0; step < 300U && append_agrees; ++step)
    {
        // Every third append duplicates an existing address so chains grow.
        const std::uint32_t guest = 0x03040000U + ((step % 100U) * 0x10U);
        AddEntry(&appended, guest, 0x200U + step * 0x10U, 2U + step,
                 (step % 5U) != 0U);
        EnsureAotCacheAddressIndex(&appended);
        if (step % 7U == 0U)
        {
            appended.retired_guest_addresses.push_back(guest);
            std::sort(appended.retired_guest_addresses.begin(),
                      appended.retired_guest_addresses.end());
            appended.retired_guest_addresses.erase(
                std::unique(appended.retired_guest_addresses.begin(),
                            appended.retired_guest_addresses.end()),
                appended.retired_guest_addresses.end());
        }
        append_agrees = AgreesEverywhere(appended);
    }

    // 7. Invalidated index must fall back to the scan and still agree.
    InvalidateAotCacheAddressIndex(&appended);
    const bool fallback = AgreesEverywhere(appended);

    // 8. An unindexed placement (built the way several probes build one) must
    //    behave exactly as before this task.
    AotCodeCachePlacement unindexed = MakeBasePlacement();
    AddEntry(&unindexed, 0x03050000U, 0x100U, 1U, true);
    AddEntry(&unindexed, 0x03050000U, 0x200U, 2U, true);
    unindexed.retired_guest_addresses = {0x03050000U};
    const bool unindexed_ok = AgreesEverywhere(unindexed);

    // 9. Task 334, cache-to-guest. The same placements exercise the reverse
    //    direction, plus three shapes the forward index never cares about: a
    //    gap between ranges, an entry longer than its successor's distance, and
    //    an out-of-order append that must turn the index off.
    bool reverse_agrees = ReverseAgreesEverywhere(no_retired) &&
        ReverseAgreesEverywhere(mixed) &&
        ReverseAgreesEverywhere(appended) &&
        ReverseAgreesEverywhere(unindexed);

    AotCodeCachePlacement ranged = MakeBasePlacement();
    ranged.address_map.push_back({0x03060000U, 0x000U, 4U, 16U});
    ranged.address_map_states.push_back({1U, true, true});
    ranged.address_map.push_back({0x03060010U, 0x010U, 4U, 1U});
    ranged.address_map_states.push_back({1U, true, true});
    // Deliberate gap: 0x011..0x01F belong to nobody.
    ranged.address_map.push_back({0x03060020U, 0x020U, 4U, 8U});
    ranged.address_map_states.push_back({1U, true, true});
    EnsureAotCacheAddressIndex(&ranged);
    const bool reverse_ranges = ReverseAgreesEverywhere(ranged) &&
        ranged.cache_address_index.cache_offset_sorted &&
        ranged.cache_address_index.max_emitted_length == 16U;

    // An out-of-order cache offset is the one shape the binary search cannot
    // answer, so the index must report itself unusable and the scan must run.
    AotCodeCachePlacement unsorted = MakeBasePlacement();
    unsorted.address_map.push_back({0x03070000U, 0x100U, 4U, 8U});
    unsorted.address_map_states.push_back({1U, true, true});
    unsorted.address_map.push_back({0x03070010U, 0x080U, 4U, 8U});
    unsorted.address_map_states.push_back({1U, true, true});
    EnsureAotCacheAddressIndex(&unsorted);
    const bool reverse_unsorted =
        !unsorted.cache_address_index.cache_offset_sorted &&
        !repiu::engine::LookupAotGuestAddressIndex(
             unsorted, 0x100U).usable &&
        ReverseAgreesEverywhere(unsorted);

    reverse_agrees = reverse_agrees && reverse_ranges && reverse_unsorted;

    const bool all = empty_retired && mixed_retired && inactive_newest &&
        inactive_all && collisions && append_agrees && fallback &&
        unindexed_ok && reverse_agrees;

    std::cout
        << "aot_cache_address_index_empty_retired="
        << (empty_retired ? "true" : "false")
        << "\naot_cache_address_index_mixed_retired="
        << (mixed_retired ? "true" : "false")
        << "\naot_cache_address_index_newest_inactive="
        << (inactive_newest ? "true" : "false")
        << "\naot_cache_address_index_all_inactive="
        << (inactive_all ? "true" : "false")
        << "\naot_cache_address_index_collisions="
        << (collisions ? "true" : "false")
        << "\naot_cache_address_index_dynamic_append="
        << (append_agrees ? "true" : "false")
        << "\naot_cache_address_index_invalidated_fallback="
        << (fallback ? "true" : "false")
        << "\naot_cache_address_index_unindexed="
        << (unindexed_ok ? "true" : "false")
        << "\naot_cache_address_index_reverse="
        << (reverse_agrees ? "true" : "false")
        << "\naot_cache_address_index_reverse_ranges="
        << (reverse_ranges ? "true" : "false")
        << "\naot_cache_address_index_reverse_unsorted="
        << (reverse_unsorted ? "true" : "false")
        << "\naot_cache_address_index_all="
        << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
