#include "glide_lfb_timing_probe.h"

#include "repiu/engine/glide_lfb_timing.h"

#include <iostream>

namespace repiu::tools
{

bool RunGlideLfbTimingProbe()
{
    using engine::GlideLfbTimingProfile;
    using engine::RecordGlideLfbTiming;
    using engine::SnapshotGlideLfbTiming;

    const bool policy =
        !engine::ResolveGlideLfbTimingProfileEnabled("") &&
        !engine::ResolveGlideLfbTimingProfileEnabled("0") &&
        engine::ResolveGlideLfbTimingProfileEnabled("1") &&
        engine::ResolveGlideLfbTimingProfileEnabled("on") &&
        engine::ResolveGlideLfbTimingProfileEnabled("true");

    GlideLfbTimingProfile profile;
    RecordGlideLfbTiming(&profile, true, true, true, 100U, 110U, 130U);
    RecordGlideLfbTiming(&profile, false, false, false, 200U, 230U, 230U);
    RecordGlideLfbTiming(&profile, true, true, false, 300U, 320U, 350U);
    const auto snapshot = SnapshotGlideLfbTiming(profile);
    const bool aggregation =
        snapshot.enabled &&
        snapshot.lock_count == 3U &&
        snapshot.readback_success_count == 2U &&
        snapshot.readback_failure_count == 1U &&
        snapshot.encode_success_count == 1U &&
        snapshot.encode_failure_count == 1U &&
        snapshot.readback_cycles == 60U &&
        snapshot.encode_cycles == 50U &&
        snapshot.total_cycles == 110U &&
        snapshot.max_readback_cycles == 30U &&
        snapshot.max_encode_cycles == 30U &&
        snapshot.max_total_cycles == 50U;

    GlideLfbTimingProfile clamped_profile;
    RecordGlideLfbTiming(&clamped_profile, false, false, false,
                         50U, 40U, 30U);
    const auto clamped = SnapshotGlideLfbTiming(clamped_profile);
    const bool clamps = clamped.clamped_sample_count == 3U &&
        clamped.total_cycles == 0U;

    RecordGlideLfbTiming(nullptr, true, true, true, 0U, 1U, 2U);
    const GlideLfbTimingProfile untouched;
    const bool inert = !SnapshotGlideLfbTiming(untouched).enabled;

    const bool all = policy && aggregation && clamps && inert;
    std::cout << "glide_lfb_timing_policy=" << (policy ? "true" : "false")
              << "\nglide_lfb_timing_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_lfb_timing_clamps=" << (clamps ? "true" : "false")
              << "\nglide_lfb_timing_inert=" << (inert ? "true" : "false")
              << "\nglide_lfb_timing_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
