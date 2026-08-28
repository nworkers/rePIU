#include "retired_trap_profile_probe.h"

#include "repiu/engine/aot_code_cache_win32.h"
#include "repiu/engine/aot_retired_trap_profile.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{

bool RunAotRetiredTrapProfileProbe()
{
    using namespace repiu::engine;
    const bool policy =
        !ResolveAotRetiredTrapProfileEnabled("") &&
        ResolveAotRetiredTrapProfileEnabled("1") &&
        ResolveAotRetiredTrapProfileEnabled("on") &&
        ResolveAotRetiredTrapProfileEnabled("true") &&
        !ResolveAotRetiredTrapProfileEnabled("0") &&
        !ResolveAotRetiredTrapProfileEnabled("off") &&
        !ResolveAotRetiredTrapProfileEnabled("invalid");

    Win32AotCodeCachePlacement placement;
    placement.placed = true;
    placement.base_address = 0x10000000U;
    const std::uint32_t guest_a = 0x03010000U;
    const std::uint32_t guest_b = 0x03020000U;
    placement.address_map.push_back({guest_a, 0x100U, 1U, 1U});
    placement.address_map.push_back({guest_b, 0x200U, 4U, 8U});
    placement.address_map.push_back({guest_a, 0x300U, 2U, 7U});
    placement.address_map_states.push_back({3U, false, true});
    placement.address_map_states.push_back({5U, false, true});
    placement.address_map_states.push_back({6U, false, true});
    placement.inactive_map_index_by_cache_offset.emplace(0x100U, 0U);
    placement.inactive_map_index_by_cache_offset.emplace(0x200U, 1U);
    placement.inactive_map_index_by_cache_offset.emplace(0x300U, 2U);

    Win32AotRetiredTrapProfile profile;
    for (std::uint32_t index = 0; index < 3U; ++index)
    {
        RecordAotRetiredTrap(
            &profile, placement, placement.base_address + 0x100U, guest_a);
    }
    RecordAotRetiredTrap(
        &profile, placement, placement.base_address + 0x200U, guest_b);
    RecordAotRetiredTrap(
        &profile, placement, placement.base_address + 0x300U, guest_a);
    RecordAotRetiredTrapResolution(
        &profile, AotRetiredTrapResolution::kActiveHit);
    RecordAotRetiredTrapResolution(
        &profile, AotRetiredTrapResolution::kActiveHit);
    RecordAotRetiredTrapResolution(
        &profile, AotRetiredTrapResolution::kGenerationPublished);
    RecordAotRetiredTrapResolution(
        &profile, AotRetiredTrapResolution::kQuarantined);
    RecordAotRetiredTrapResolution(
        &profile, AotRetiredTrapResolution::kFallback);
    const Win32AotRetiredTrapProfileSnapshot snapshot =
        SnapshotAotRetiredTrapProfile(profile);
    const bool behavior = snapshot.enabled &&
        snapshot.total_trap_count == 5U &&
        snapshot.distinct_guest_count == 2U &&
        snapshot.distinct_cache_count == 3U &&
        snapshot.top_guest_coverage_count == 5U &&
        snapshot.short_trap_count == 3U &&
        snapshot.relinkable_trap_count == 2U &&
        snapshot.metadata_miss_count == 0U &&
        snapshot.guest_hotspot_count == 2U &&
        snapshot.guest_hotspots[0].guest_address == guest_a &&
        snapshot.guest_hotspots[0].trap_count == 4U &&
        snapshot.cache_hotspot_count == 3U &&
        snapshot.cache_hotspots[0].cache_address ==
            placement.base_address + 0x100U &&
        snapshot.cache_hotspots[0].trap_count == 3U &&
        snapshot.cache_hotspots[0].generation == 3U &&
        snapshot.cache_hotspots[0].emitted_length == 1U &&
        snapshot.resolution_counts[0] == 2U &&
        snapshot.resolution_counts[1] == 1U &&
        snapshot.resolution_counts[2] == 1U &&
        snapshot.resolution_counts[4] == 1U;

    Win32AotCodeCachePlacement empty_placement;
    Win32AotRetiredTrapProfile capacity_profile;
    for (std::uint32_t index = 0;
         index <= kWin32AotRetiredTrapHistogramCapacity; ++index)
    {
        RecordAotRetiredTrap(
            &capacity_profile, empty_placement,
            0x20000000U + index, 0x40000000U + index);
    }
    const Win32AotRetiredTrapProfileSnapshot capacity_snapshot =
        SnapshotAotRetiredTrapProfile(capacity_profile);
    const bool capacity =
        capacity_snapshot.distinct_guest_count ==
            kWin32AotRetiredTrapHistogramCapacity &&
        capacity_snapshot.distinct_cache_count ==
            kWin32AotRetiredTrapHistogramCapacity &&
        capacity_snapshot.guest_histogram_overflow_count == 1U &&
        capacity_snapshot.cache_histogram_overflow_count == 1U;

    const bool all = policy && behavior && capacity;
    std::cout << "retired_trap_profile_policy="
              << (policy ? "true" : "false")
              << "\nretired_trap_profile_behavior="
              << (behavior ? "true" : "false")
              << "\nretired_trap_profile_capacity="
              << (capacity ? "true" : "false")
              << "\nretired_trap_profile_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

} // namespace repiu::tools
