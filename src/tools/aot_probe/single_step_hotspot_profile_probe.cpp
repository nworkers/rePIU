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

    // Task 322 stage attribution. Verify that stage totals accumulate
    // identically at global and entry scope, that a disabled profile
    // accumulates nothing, and that the derived residual can never go negative
    // because staged cycles stay within the enclosing sample total.
    const auto prologue = static_cast<std::uint32_t>(
        SingleStepProfileStage::kPrologueTrace);
    const auto hle_dispatch = static_cast<std::uint32_t>(
        SingleStepProfileStage::kHleDispatch);
    const auto aot_resume = static_cast<std::uint32_t>(
        SingleStepProfileStage::kAotResume);

    auto stage_profile = std::make_unique<Win32SingleStepHotspotProfile>();
    Win32SingleStepStageTally tally;
    tally.counts[prologue] = 1U;
    tally.cycles[prologue] = 40U;
    tally.counts[hle_dispatch] = 1U;
    tally.cycles[hle_dispatch] = 120U;
    tally.counts[aot_resume] = 1U;
    tally.cycles[aot_resume] = 700U;
    for (std::uint32_t index = 0; index < 2U; ++index)
    {
        RecordSingleStepHotspot(
            stage_profile.get(), guest_a, 900U,
            SingleStepProfileOutcome::kHandledHle, &tally);
    }
    // A sample recorded without a tally must leave the stage totals untouched.
    RecordSingleStepHotspot(
        stage_profile.get(), guest_b, 10U,
        SingleStepProfileOutcome::kTrapFlagRearm);

    const Win32SingleStepHotspotProfileSnapshot stage_snapshot =
        SnapshotSingleStepHotspotProfile(*stage_profile);
    std::uint64_t staged_cycles = 0;
    for (std::uint32_t index = 0;
         index < kSingleStepProfileStageCount; ++index)
    {
        staged_cycles += stage_snapshot.stage_cycles[index];
    }
    const Win32SingleStepHotspotSample* guest_a_sample = nullptr;
    const Win32SingleStepHotspotSample* guest_b_sample = nullptr;
    for (std::uint32_t index = 0;
         index < stage_snapshot.cycle_hotspot_count; ++index)
    {
        const auto& sample = stage_snapshot.cycle_hotspots[index];
        if (sample.guest_address == guest_a)
        {
            guest_a_sample = &sample;
        }
        else if (sample.guest_address == guest_b)
        {
            guest_b_sample = &sample;
        }
    }
    const bool stages =
        guest_a_sample != nullptr &&
        guest_b_sample != nullptr &&
        stage_snapshot.stage_counts[prologue] == 2U &&
        stage_snapshot.stage_counts[hle_dispatch] == 2U &&
        stage_snapshot.stage_counts[aot_resume] == 2U &&
        stage_snapshot.stage_cycles[prologue] == 80U &&
        stage_snapshot.stage_cycles[hle_dispatch] == 240U &&
        stage_snapshot.stage_cycles[aot_resume] == 1400U &&
        guest_a_sample->stage_counts[prologue] == 2U &&
        guest_a_sample->stage_cycles[aot_resume] == 1400U &&
        guest_b_sample->stage_counts[prologue] == 0U &&
        guest_b_sample->stage_cycles[prologue] == 0U &&
        staged_cycles <= stage_snapshot.total_cycles;

    // A null profile must remain inert even when a tally is supplied.
    Win32SingleStepStageTally discarded = tally;
    RecordSingleStepHotspot(
        nullptr, guest_a, 1U,
        SingleStepProfileOutcome::kHandledHle, &discarded);

    // Task 323: the kAotResume sub-stages must stay within their parent stage,
    // which is what makes the reported sub-stage residual meaningful.
    auto sub_stage_profile =
        std::make_unique<Win32SingleStepHotspotProfile>();
    Win32SingleStepStageTally sub_tally;
    const auto aot_resume_index = static_cast<std::uint32_t>(
        SingleStepProfileStage::kAotResume);
    const auto cache_lookup = static_cast<std::uint32_t>(
        SingleStepProfileStage::kCacheLookup);
    const auto span_safety = static_cast<std::uint32_t>(
        SingleStepProfileStage::kSpanSafety);
    sub_tally.counts[aot_resume_index] = 1U;
    sub_tally.cycles[aot_resume_index] = 1000U;
    sub_tally.counts[cache_lookup] = 1U;
    sub_tally.cycles[cache_lookup] = 700U;
    sub_tally.counts[span_safety] = 1U;
    sub_tally.cycles[span_safety] = 200U;
    RecordSingleStepHotspot(
        sub_stage_profile.get(), guest_a, 1200U,
        SingleStepProfileOutcome::kHandledHle, &sub_tally);
    const Win32SingleStepHotspotProfileSnapshot sub_stage_snapshot =
        SnapshotSingleStepHotspotProfile(*sub_stage_profile);
    std::uint64_t sub_stage_total = 0;
    for (std::uint32_t index = kSingleStepProfileFirstAotResumeSubStage;
         index < kSingleStepProfileStageCount; ++index)
    {
        sub_stage_total += sub_stage_snapshot.stage_cycles[index];
    }
    const bool sub_stages =
        kSingleStepProfileFirstAotResumeSubStage > aot_resume_index &&
        sub_stage_snapshot.stage_cycles[cache_lookup] == 700U &&
        sub_stage_snapshot.stage_cycles[span_safety] == 200U &&
        sub_stage_total <=
            sub_stage_snapshot.stage_cycles[aot_resume_index] &&
        sub_stage_snapshot.stage_cycles[aot_resume_index] <=
            sub_stage_snapshot.total_cycles;

    const bool all =
        policy && behavior && capacity && stages && sub_stages;
    std::cout
        << "single_step_hotspot_profile_policy="
        << (policy ? "true" : "false")
        << "\nsingle_step_hotspot_profile_behavior="
        << (behavior ? "true" : "false")
        << "\nsingle_step_hotspot_profile_capacity="
        << (capacity ? "true" : "false")
        << "\nsingle_step_hotspot_profile_stages="
        << (stages ? "true" : "false")
        << "\nsingle_step_hotspot_profile_sub_stages="
        << (sub_stages ? "true" : "false")
        << "\nsingle_step_hotspot_profile_all="
        << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
