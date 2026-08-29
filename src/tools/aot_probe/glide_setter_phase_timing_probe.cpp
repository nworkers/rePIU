#include "glide_setter_phase_timing_probe.h"

#include "repiu/engine/glide_setter_phase_timing.h"

#include <iostream>

namespace repiu::tools
{

bool RunGlideSetterPhaseTimingProbe()
{
    using engine::RecordGlideSetterPhaseSample;
    using engine::SnapshotGlideSetterPhaseTiming;
    using engine::GlideSetterPhaseKind;
    using engine::GlideSetterPhaseProfile;

    const bool policy =
        !engine::ResolveGlideSetterPhaseProfileEnabled("") &&
        !engine::ResolveGlideSetterPhaseProfileEnabled("0") &&
        engine::ResolveGlideSetterPhaseProfileEnabled("1") &&
        engine::ResolveGlideSetterPhaseProfileEnabled("on") &&
        engine::ResolveGlideSetterPhaseProfileEnabled("true");

    GlideSetterPhaseProfile profile;
    // Depth mask: entry and apply-start are the same instant, so drain is zero
    // by construction rather than by omission.
    RecordGlideSetterPhaseSample(
        &profile, GlideSetterPhaseKind::kDepthMask,
        100U, 100U, 130U, 200U, 0U, false);
    RecordGlideSetterPhaseSample(
        &profile, GlideSetterPhaseKind::kDepthMask,
        300U, 300U, 310U, 330U, 0U, true);
    RecordGlideSetterPhaseSample(
        &profile, GlideSetterPhaseKind::kAlphaBlend,
        1000U, 1040U, 1060U, 1100U, 3U, false);

    const auto snapshot = SnapshotGlideSetterPhaseTiming(profile);
    const bool depth_mask =
        snapshot.enabled &&
        snapshot.depth_mask.call_count == 2U &&
        snapshot.depth_mask.drain_cycles == 0U &&
        snapshot.depth_mask.apply_cycles == 40U &&
        snapshot.depth_mask.error_cycles == 90U &&
        snapshot.depth_mask.total_cycles == 130U &&
        snapshot.depth_mask.max_total_cycles == 100U &&
        snapshot.depth_mask.max_apply_cycles == 30U &&
        snapshot.depth_mask.max_error_cycles == 70U &&
        snapshot.depth_mask.drain_iteration_count == 0U &&
        snapshot.depth_mask.error_count == 1U;
    const bool alpha_blend =
        snapshot.alpha_blend.call_count == 1U &&
        snapshot.alpha_blend.drain_cycles == 40U &&
        snapshot.alpha_blend.apply_cycles == 20U &&
        snapshot.alpha_blend.error_cycles == 40U &&
        snapshot.alpha_blend.total_cycles == 100U &&
        snapshot.alpha_blend.drain_iteration_count == 3U &&
        snapshot.alpha_blend.error_count == 0U;
    // The partition identity the measurement script also checks.
    const bool phase_sum =
        snapshot.depth_mask.drain_cycles + snapshot.depth_mask.apply_cycles +
                snapshot.depth_mask.error_cycles ==
            snapshot.depth_mask.total_cycles &&
        snapshot.alpha_blend.drain_cycles + snapshot.alpha_blend.apply_cycles +
                snapshot.alpha_blend.error_cycles ==
            snapshot.alpha_blend.total_cycles;

    GlideSetterPhaseProfile clamped_profile;
    RecordGlideSetterPhaseSample(
        &clamped_profile, GlideSetterPhaseKind::kAlphaBlend,
        400U, 300U, 200U, 100U, 0U, false);
    const auto clamped = SnapshotGlideSetterPhaseTiming(clamped_profile);
    const bool clamps =
        clamped.clamped_sample_count == 4U &&
        clamped.alpha_blend.total_cycles == 0U;

    RecordGlideSetterPhaseSample(
        nullptr, GlideSetterPhaseKind::kDepthMask,
        0U, 1U, 2U, 3U, 0U, false);
    const GlideSetterPhaseProfile untouched;
    const auto inert_snapshot = SnapshotGlideSetterPhaseTiming(untouched);
    const bool inert =
        !inert_snapshot.enabled &&
        inert_snapshot.depth_mask.call_count == 0U &&
        inert_snapshot.alpha_blend.call_count == 0U;

    const bool all = policy && depth_mask && alpha_blend && phase_sum &&
        clamps && inert;
    std::cout << "glide_setter_phase_timing_policy="
              << (policy ? "true" : "false")
              << "\nglide_setter_phase_timing_depth_mask="
              << (depth_mask ? "true" : "false")
              << "\nglide_setter_phase_timing_alpha_blend="
              << (alpha_blend ? "true" : "false")
              << "\nglide_setter_phase_timing_phase_sum="
              << (phase_sum ? "true" : "false")
              << "\nglide_setter_phase_timing_clamps="
              << (clamps ? "true" : "false")
              << "\nglide_setter_phase_timing_inert="
              << (inert ? "true" : "false")
              << "\nglide_setter_phase_timing_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
