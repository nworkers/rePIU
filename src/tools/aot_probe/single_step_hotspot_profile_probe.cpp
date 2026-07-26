#include "single_step_hotspot_profile_probe.h"

#include "repiu/platform/win32/single_step_hotspot_profile.h"

#include <cstdint>
#include <iostream>
#include <memory>

namespace repiu::tools
{

bool RunSingleStepHotspotProfileProbe()
{
    using namespace repiu::platform::win32;
    const bool policy =
        !ResolveSingleStepHotspotProfileEnabled("") &&
        ResolveSingleStepHotspotProfileEnabled("1") &&
        ResolveSingleStepHotspotProfileEnabled("on") &&
        ResolveSingleStepHotspotProfileEnabled("true") &&
        !ResolveSingleStepHotspotProfileEnabled("0") &&
        !ResolveSingleStepHotspotProfileEnabled("off") &&
        !ResolveSingleStepHotspotProfileEnabled("invalid");

    auto profile = std::make_unique<Win32SingleStepHotspotProfile>();
    constexpr std::uint32_t guest_a = 0x03010000U;
    constexpr std::uint32_t guest_b = 0x03020000U;
    for (std::uint32_t index = 0; index < 3U; ++index)
    {
        RecordSingleStepHotspot(
            profile.get(), guest_a, 30U,
            SingleStepProfileOutcome::kHandledHle);
    }
    RecordSingleStepHotspot(
        profile.get(), guest_b, 500U,
        SingleStepProfileOutcome::kTrapFlagRearm);

    const Win32SingleStepHotspotProfileSnapshot snapshot =
        SnapshotSingleStepHotspotProfile(*profile);
    const auto hle = static_cast<std::uint32_t>(
        SingleStepProfileOutcome::kHandledHle);
    const auto trap_flag = static_cast<std::uint32_t>(
        SingleStepProfileOutcome::kTrapFlagRearm);
    const bool behavior =
        snapshot.enabled &&
        snapshot.total_sample_count == 4U &&
        snapshot.distinct_guest_count == 2U &&
        snapshot.overflow_count == 0U &&
        snapshot.total_cycles == 590U &&
        snapshot.max_cycles == 500U &&
        snapshot.outcome_counts[hle] == 3U &&
        snapshot.outcome_cycles[hle] == 90U &&
        snapshot.outcome_counts[trap_flag] == 1U &&
        snapshot.outcome_cycles[trap_flag] == 500U &&
        snapshot.count_hotspot_count == 2U &&
        snapshot.count_hotspots[0].guest_address == guest_a &&
        snapshot.count_hotspots[0].sample_count == 3U &&
        snapshot.cycle_hotspot_count == 2U &&
        snapshot.cycle_hotspots[0].guest_address == guest_b &&
        snapshot.cycle_hotspots[0].total_cycles == 500U &&
        snapshot.top_count_coverage_count == 4U &&
        snapshot.top_cycle_coverage_cycles == 590U;

    auto capacity_profile =
        std::make_unique<Win32SingleStepHotspotProfile>();
    for (std::uint32_t index = 0;
         index <= kWin32SingleStepHotspotCapacity; ++index)
    {
        RecordSingleStepHotspot(
            capacity_profile.get(), 0x04000000U + index, 1U,
            SingleStepProfileOutcome::kTrapFlagRearm);
    }
    const Win32SingleStepHotspotProfileSnapshot capacity_snapshot =
        SnapshotSingleStepHotspotProfile(*capacity_profile);
    const bool capacity =
        capacity_snapshot.total_sample_count ==
            kWin32SingleStepHotspotCapacity + 1U &&
        capacity_snapshot.distinct_guest_count ==
            kWin32SingleStepHotspotCapacity &&
        capacity_snapshot.overflow_count == 1U;

    const bool all = policy && behavior && capacity;
    std::cout
        << "single_step_hotspot_profile_policy="
        << (policy ? "true" : "false")
        << "\nsingle_step_hotspot_profile_behavior="
        << (behavior ? "true" : "false")
        << "\nsingle_step_hotspot_profile_capacity="
        << (capacity ? "true" : "false")
        << "\nsingle_step_hotspot_profile_all="
        << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
