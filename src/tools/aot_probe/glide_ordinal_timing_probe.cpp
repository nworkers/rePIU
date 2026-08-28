#include "glide_ordinal_timing_probe.h"

#include "repiu/engine/glide_ordinal_timing.h"

#include <iostream>

namespace repiu::tools
{
bool RunGlideOrdinalTimingProbe()
{
    using engine::RecordGlideOrdinalDirectWork;
    using engine::RecordGlideOrdinalGateTime;
    using engine::RecordGlideOrdinalRendezvous;
    using engine::SnapshotGlideOrdinalTiming;
    using engine::Win32GlideOrdinalTimingProfile;

    const bool policy =
        !engine::ResolveGlideOrdinalTimingProfileEnabled("") &&
        !engine::ResolveGlideOrdinalTimingProfileEnabled("0") &&
        engine::ResolveGlideOrdinalTimingProfileEnabled("1") &&
        engine::ResolveGlideOrdinalTimingProfileEnabled("on") &&
        engine::ResolveGlideOrdinalTimingProfileEnabled("true");

    Win32GlideOrdinalTimingProfile profile;
    RecordGlideOrdinalGateTime(&profile, 73U, 150U);
    RecordGlideOrdinalRendezvous(
        &profile, 73U, 1000U, 1010U, 1030U, 1060U, 1100U);
    RecordGlideOrdinalGateTime(&profile, 73U, 50U);
    RecordGlideOrdinalRendezvous(
        &profile, 73U, 2000U, 2001U, 2003U, 2006U, 2010U);
    RecordGlideOrdinalDirectWork(&profile, 73U, 7U);
    RecordGlideOrdinalGateTime(&profile, 85U, 80U);

    const auto snapshot = SnapshotGlideOrdinalTiming(profile);
    const auto& draw = snapshot.entries[73U];
    const auto& swap = snapshot.entries[85U];
    const bool aggregation = snapshot.enabled &&
        snapshot.active_entry_count == 2U &&
        snapshot.completed_gate_count == 3U &&
        snapshot.gate_cycles == 280U &&
        draw.count == 2U && draw.gate_cycles == 200U &&
        draw.max_gate_cycles == 150U &&
        swap.count == 1U && swap.gate_cycles == 80U;
    const bool backend_delta =
        snapshot.rendezvous_count == 2U &&
        snapshot.queue_cycles == 11U &&
        snapshot.wake_cycles == 22U &&
        snapshot.work_cycles == 33U &&
        snapshot.complete_cycles == 44U &&
        snapshot.backend_total_cycles == 110U &&
        snapshot.direct_count == 1U &&
        snapshot.direct_work_cycles == 7U &&
        draw.rendezvous_count == 2U &&
        draw.backend_total_cycles == 110U &&
        swap.backend_total_cycles == 0U;

    Win32GlideOrdinalTimingProfile clamped_profile;
    RecordGlideOrdinalGateTime(&clamped_profile, 1U, 10U);
    RecordGlideOrdinalRendezvous(
        &clamped_profile, 1U, 50U, 40U, 30U, 20U, 10U);
    const auto clamped = SnapshotGlideOrdinalTiming(clamped_profile);
    const bool clamps = clamped.clamped_sample_count == 5U &&
        clamped.rendezvous_count == 1U &&
        clamped.backend_total_cycles == 0U;

    Win32GlideOrdinalTimingProfile overflow_profile;
    RecordGlideOrdinalGateTime(&overflow_profile, 300U, 10U);
    const auto overflow = SnapshotGlideOrdinalTiming(overflow_profile);
    const bool capacity = overflow.enabled &&
        overflow.overflow_count == 1U &&
        overflow.completed_gate_count == 0U;

    RecordGlideOrdinalGateTime(nullptr, 1U, 10U);
    RecordGlideOrdinalRendezvous(
        nullptr, 1U, 0U, 1U, 2U, 3U, 4U);
    RecordGlideOrdinalDirectWork(nullptr, 1U, 1U);
    const Win32GlideOrdinalTimingProfile untouched;
    const auto inert_snapshot = SnapshotGlideOrdinalTiming(untouched);
    const bool inert = !inert_snapshot.enabled &&
        inert_snapshot.completed_gate_count == 0U;

    const bool all =
        policy && aggregation && backend_delta && clamps && capacity && inert;
    std::cout << "glide_ordinal_timing_policy="
              << (policy ? "true" : "false")
              << "\nglide_ordinal_timing_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_ordinal_timing_backend_delta="
              << (backend_delta ? "true" : "false")
              << "\nglide_ordinal_timing_clamps="
              << (clamps ? "true" : "false")
              << "\nglide_ordinal_timing_capacity="
              << (capacity ? "true" : "false")
              << "\nglide_ordinal_timing_inert="
              << (inert ? "true" : "false")
              << "\nglide_ordinal_timing_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
