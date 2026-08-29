#include "glide_buffer_swap_timing_probe.h"

#include "repiu/engine/glide_buffer_swap_timing.h"

#include <iostream>

namespace repiu::tools
{

bool RunGlideBufferSwapTimingProbe()
{
    using engine::RecordGlideBufferSwapSdlInterval;
    using engine::RecordGlideBufferSwapTiming;
    using engine::SnapshotGlideBufferSwapTiming;
    using engine::GlideBufferSwapTimingProfile;

    const bool policy =
        !engine::ResolveGlideBufferSwapTimingProfileEnabled("") &&
        !engine::ResolveGlideBufferSwapTimingProfileEnabled("0") &&
        engine::ResolveGlideBufferSwapTimingProfileEnabled("1") &&
        engine::ResolveGlideBufferSwapTimingProfileEnabled("on") &&
        engine::ResolveGlideBufferSwapTimingProfileEnabled("true");

    GlideBufferSwapTimingProfile profile;
    RecordGlideBufferSwapSdlInterval(&profile, true, 0);
    RecordGlideBufferSwapSdlInterval(&profile, false, 7);
    RecordGlideBufferSwapTiming(
        &profile, 1U, true, 100U, 110U, 130U, 150U, 160U);
    RecordGlideBufferSwapTiming(
        &profile, 3U, false, 200U, 205U, 215U, 215U, 220U);
    const auto snapshot = SnapshotGlideBufferSwapTiming(profile);
    const bool aggregation =
        snapshot.enabled &&
        snapshot.call_count == 2U &&
        snapshot.success_count == 1U &&
        snapshot.failure_count == 1U &&
        snapshot.setup_cycles == 15U &&
        snapshot.present_cycles == 30U &&
        snapshot.accounting_cycles == 20U &&
        snapshot.finalize_cycles == 15U &&
        snapshot.total_cycles == 80U &&
        snapshot.max_present_cycles == 20U;
    const bool phase_sum =
        snapshot.setup_cycles + snapshot.present_cycles +
            snapshot.accounting_cycles + snapshot.finalize_cycles ==
        snapshot.total_cycles;
    const bool intervals =
        snapshot.requested_zero_count == 0U &&
        snapshot.requested_one_count == 1U &&
        snapshot.requested_other_count == 1U &&
        snapshot.requested_minimum == 1U &&
        snapshot.requested_maximum == 3U &&
        snapshot.requested_last == 3U &&
        snapshot.sdl_interval_query_count == 2U &&
        snapshot.sdl_interval_query_success_count == 1U &&
        snapshot.sdl_interval_query_failure_count == 1U &&
        snapshot.observed_sdl_interval == 0;

    GlideBufferSwapTimingProfile clamped_profile;
    RecordGlideBufferSwapTiming(
        &clamped_profile, 0U, false, 50U, 40U, 30U, 20U, 10U);
    const auto clamped = SnapshotGlideBufferSwapTiming(clamped_profile);
    const bool clamps =
        clamped.clamped_sample_count == 5U &&
        clamped.total_cycles == 0U;

    RecordGlideBufferSwapTiming(
        nullptr, 0U, true, 0U, 1U, 2U, 3U, 4U);
    RecordGlideBufferSwapSdlInterval(nullptr, true, 1);
    const GlideBufferSwapTimingProfile untouched;
    const auto inert_snapshot = SnapshotGlideBufferSwapTiming(untouched);
    const bool inert =
        !inert_snapshot.enabled &&
        inert_snapshot.call_count == 0U &&
        inert_snapshot.requested_minimum == 0U;

    const bool all =
        policy && aggregation && phase_sum && intervals && clamps && inert;
    std::cout << "glide_buffer_swap_timing_policy="
              << (policy ? "true" : "false")
              << "\nglide_buffer_swap_timing_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_buffer_swap_timing_phase_sum="
              << (phase_sum ? "true" : "false")
              << "\nglide_buffer_swap_timing_intervals="
              << (intervals ? "true" : "false")
              << "\nglide_buffer_swap_timing_clamps="
              << (clamps ? "true" : "false")
              << "\nglide_buffer_swap_timing_inert="
              << (inert ? "true" : "false")
              << "\nglide_buffer_swap_timing_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
