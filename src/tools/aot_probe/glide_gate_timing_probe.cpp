#include "glide_gate_timing_probe.h"

#include "repiu/engine/glide_gate_timing.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

// One rendezvous with synthetic timestamps, so every interval has a different
// width and a mis-assignment cannot be hidden by symmetry.
//
//   enter 1000 -> publish 1100 -> host start 3100 -> host finish 3300
//   -> resume 3350
//
// queue 100, wake 2000, work 200, complete 50, total 2350.
void PlayOneRendezvous(engine::GlideGateTimingProfile* profile,
                       std::uint64_t enter,
                       std::uint64_t publish,
                       std::uint64_t host_start,
                       std::uint64_t host_finish,
                       std::uint64_t resume)
{
    engine::RecordGlideGatePublish(profile, enter, publish);
    engine::RecordGlideGateHostCommand(profile, host_start,
                                                host_finish);
    engine::RecordGlideGateResume(profile, enter, resume);
}

}  // namespace

bool RunGlideGateTimingProbe()
{
    engine::GlideGateTimingProfile profile;
    PlayOneRendezvous(&profile, 1000U, 1100U, 3100U, 3300U, 3350U);

    const engine::GlideGateTimingSnapshot first =
        engine::SnapshotGlideGateTiming(profile);
    const bool intervals = first.enabled && first.rendezvous_count == 1U &&
        first.queue_cycles == 100U && first.wake_cycles == 2000U &&
        first.work_cycles == 200U && first.complete_cycles == 50U &&
        first.total_cycles == 2350U && first.residual_cycles == 0U;

    // A second, cheaper rendezvous must accumulate rather than replace, and the
    // maxima must stay with the first.
    PlayOneRendezvous(&profile, 4000U, 4010U, 4020U, 4120U, 4130U);
    const engine::GlideGateTimingSnapshot second =
        engine::SnapshotGlideGateTiming(profile);
    const bool accumulates = second.rendezvous_count == 2U &&
        second.queue_cycles == 110U && second.wake_cycles == 2010U &&
        second.work_cycles == 300U && second.complete_cycles == 60U &&
        second.total_cycles == 2480U && second.residual_cycles == 0U &&
        second.max_wake_cycles == 2000U && second.max_work_cycles == 200U &&
        second.max_total_cycles == 2350U;

    // Commands run on the host thread take no rendezvous, so they must move
    // only the direct axis.
    engine::RecordGlideGateDirectCommand(&profile, 700U);
    const engine::GlideGateTimingSnapshot third =
        engine::SnapshotGlideGateTiming(profile);
    const bool direct_separate = third.direct_count == 1U &&
        third.direct_work_cycles == 700U &&
        third.rendezvous_count == second.rendezvous_count &&
        third.total_cycles == second.total_cycles;

    // A backwards TSC read clamps to zero and is counted, rather than wrapping
    // into an enormous interval.
    engine::GlideGateTimingProfile backwards;
    PlayOneRendezvous(&backwards, 5000U, 4900U, 4800U, 4700U, 4600U);
    const engine::GlideGateTimingSnapshot clamped =
        engine::SnapshotGlideGateTiming(backwards);
    const bool clamps = clamped.queue_cycles == 0U &&
        clamped.wake_cycles == 0U && clamped.work_cycles == 0U &&
        clamped.complete_cycles == 0U && clamped.total_cycles == 0U &&
        clamped.clamped_sample_count == 5U;

    // A null profile must be inert, since the rendezvous runs with timing off
    // in every normal run.
    engine::RecordGlideGatePublish(nullptr, 1U, 2U);
    engine::RecordGlideGateHostCommand(nullptr, 1U, 2U);
    engine::RecordGlideGateResume(nullptr, 1U, 2U);
    engine::RecordGlideGateDirectCommand(nullptr, 1U);
    const engine::GlideGateTimingProfile untouched;
    const engine::GlideGateTimingSnapshot disabled =
        engine::SnapshotGlideGateTiming(untouched);
    const bool inert = !disabled.enabled && disabled.rendezvous_count == 0U &&
        disabled.total_cycles == 0U;

    const bool all = intervals && accumulates && direct_separate && clamps &&
        inert;
    std::cout << "glide_gate_timing_intervals="
              << (intervals ? "true" : "false")
              << "\nglide_gate_timing_accumulates="
              << (accumulates ? "true" : "false")
              << "\nglide_gate_timing_direct_separate="
              << (direct_separate ? "true" : "false")
              << "\nglide_gate_timing_clamps=" << (clamps ? "true" : "false")
              << "\nglide_gate_timing_inert=" << (inert ? "true" : "false")
              << "\nglide_gate_timing_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
