#include "repiu/engine/aot_worker_timing.h"

#include <algorithm>
#include <chrono>
#include "repiu/platform/host_time.h"


namespace repiu::engine
{

std::uint64_t ReadAotWorkerTimingCycles()
{
    return repiu::platform::ReadCycleCounter();
}

std::uint64_t AotWorkerTimingDelta(Win32AotWorkerTimingProfile* profile,
                                   std::uint64_t start,
                                   std::uint64_t end)
{
    if (end >= start)
    {
        return end - start;
    }
    if (profile != nullptr)
    {
        ++profile->clamped_sample_count;
    }
    return 0U;
}

void RecordAotWorkerRequestSignal(Win32AotWorkerTimingProfile* profile,
                                  std::uint64_t signal_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    if (profile->last_request_cycles != 0U)
    {
        profile->request_gap_cycles += AotWorkerTimingDelta(
            profile, profile->last_request_cycles, signal_cycles);
    }
    profile->last_request_cycles = signal_cycles;
    profile->request_signal_cycles = signal_cycles;
}

void RecordAotWorkerWake(Win32AotWorkerTimingProfile* profile,
                         std::uint64_t wake_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    const std::uint64_t latency = AotWorkerTimingDelta(
        profile, profile->request_signal_cycles, wake_cycles);
    profile->wake_latency_cycles += latency;
    profile->max_wake_latency_cycles =
        std::max(profile->max_wake_latency_cycles, latency);
}

void RecordAotWorkerSegmentTable(Win32AotWorkerTimingProfile* profile,
                                 std::uint64_t cycles)
{
    if (profile != nullptr)
    {
        profile->segment_table_cycles += cycles;
    }
}

void RecordAotWorkerAppend(Win32AotWorkerTimingProfile* profile,
                           std::uint64_t cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->append_cycles += cycles;
    profile->max_append_cycles =
        std::max(profile->max_append_cycles, cycles);
}

void RecordAotWorkerCompleteSignal(Win32AotWorkerTimingProfile* profile,
                                   std::uint64_t signal_cycles)
{
    if (profile != nullptr)
    {
        profile->complete_signal_cycles = signal_cycles;
    }
}

void RecordAotWorkerGuestResume(Win32AotWorkerTimingProfile* profile,
                                std::uint64_t request_cycles,
                                std::uint64_t resume_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    ++profile->translate_count;
    profile->complete_latency_cycles += AotWorkerTimingDelta(
        profile, profile->complete_signal_cycles, resume_cycles);
    const std::uint64_t total =
        AotWorkerTimingDelta(profile, request_cycles, resume_cycles);
    profile->guest_total_cycles += total;
    profile->max_guest_total_cycles =
        std::max(profile->max_guest_total_cycles, total);
}

void RecordAotWorkerOtherOperation(Win32AotWorkerTimingProfile* profile)
{
    if (profile != nullptr)
    {
        profile->enabled = true;
        ++profile->other_operation_count;
    }
}

void RecordAotAppendPhases(Win32AotWorkerTimingProfile* profile,
                           const Win32AotAppendPhaseSample& phases)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->append_phase_count;
    profile->arena_snapshot_cycles += phases.arena_snapshot_cycles;
    profile->plan_build_cycles += phases.plan_build_cycles;
    profile->image_emit_cycles += phases.image_emit_cycles;
    profile->validate_cycles += phases.validate_cycles;
    profile->placement_cycles += phases.placement_cycles;
    profile->max_arena_snapshot_cycles = std::max(
        profile->max_arena_snapshot_cycles, phases.arena_snapshot_cycles);
}

void RecordAotAppendScale(Win32AotWorkerTimingProfile* profile,
                          const Win32AotAppendScaleSample& scale)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->plan_block_total += scale.plan_block_count;
    profile->plan_instruction_total += scale.plan_instruction_count;
    profile->emitted_byte_total += scale.emitted_bytes;
    profile->snapshot_byte_total += scale.snapshot_bytes;
    profile->max_plan_instruction_count = std::max(
        profile->max_plan_instruction_count, scale.plan_instruction_count);
}

void RecordAotPlanBuildProfile(Win32AotWorkerTimingProfile* profile,
                               const runtime::AotPlanBuildProfile& plan)
{
    if (profile == nullptr || !plan.enabled)
    {
        return;
    }
    ++profile->plan_profile_count;
    profile->plan_decoder_init_cycles += plan.decoder_init_cycles;
    profile->plan_decode_cycles += plan.decode_cycles;
    profile->plan_record_build_cycles += plan.record_build_cycles;
    profile->plan_classify_cycles += plan.classify_cycles;
    profile->plan_walk_cycles += plan.walk_cycles;
    profile->plan_sweep_cycles += plan.sweep_cycles;
    profile->plan_total_cycles += plan.total_cycles;
    profile->plan_decode_count += plan.decode_count;
    profile->plan_record_count += plan.record_count;
    profile->plan_sweep_pass_count += plan.sweep_pass_count;
    profile->plan_sweep_record_visit_count += plan.sweep_record_visit_count;
    profile->clamped_sample_count += plan.clamped_sample_count;
    profile->max_plan_sweep_pass_count = std::max(
        profile->max_plan_sweep_pass_count, plan.sweep_pass_count);
}

Win32AotWorkerTimingSnapshot SnapshotAotWorkerTiming(
    const Win32AotWorkerTimingProfile& profile)
{
    Win32AotWorkerTimingSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.translate_count = profile.translate_count;
    snapshot.wake_latency_cycles = profile.wake_latency_cycles;
    snapshot.segment_table_cycles = profile.segment_table_cycles;
    snapshot.append_cycles = profile.append_cycles;
    snapshot.complete_latency_cycles = profile.complete_latency_cycles;
    snapshot.guest_total_cycles = profile.guest_total_cycles;
    snapshot.max_wake_latency_cycles = profile.max_wake_latency_cycles;
    snapshot.max_append_cycles = profile.max_append_cycles;
    snapshot.max_guest_total_cycles = profile.max_guest_total_cycles;
    snapshot.request_gap_cycles = profile.request_gap_cycles;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    snapshot.other_operation_count = profile.other_operation_count;
    snapshot.append_phase_count = profile.append_phase_count;
    snapshot.arena_snapshot_cycles = profile.arena_snapshot_cycles;
    snapshot.plan_build_cycles = profile.plan_build_cycles;
    snapshot.image_emit_cycles = profile.image_emit_cycles;
    snapshot.validate_cycles = profile.validate_cycles;
    snapshot.placement_cycles = profile.placement_cycles;
    snapshot.max_arena_snapshot_cycles = profile.max_arena_snapshot_cycles;
    snapshot.plan_block_total = profile.plan_block_total;
    snapshot.plan_instruction_total = profile.plan_instruction_total;
    snapshot.emitted_byte_total = profile.emitted_byte_total;
    snapshot.snapshot_byte_total = profile.snapshot_byte_total;
    snapshot.max_plan_instruction_count = profile.max_plan_instruction_count;
    snapshot.plan_profile_count = profile.plan_profile_count;
    snapshot.plan_decoder_init_cycles = profile.plan_decoder_init_cycles;
    snapshot.plan_decode_cycles = profile.plan_decode_cycles;
    snapshot.plan_record_build_cycles = profile.plan_record_build_cycles;
    snapshot.plan_classify_cycles = profile.plan_classify_cycles;
    snapshot.plan_walk_cycles = profile.plan_walk_cycles;
    snapshot.plan_sweep_cycles = profile.plan_sweep_cycles;
    snapshot.plan_total_cycles = profile.plan_total_cycles;
    snapshot.plan_decode_count = profile.plan_decode_count;
    snapshot.plan_record_count = profile.plan_record_count;
    snapshot.plan_sweep_pass_count = profile.plan_sweep_pass_count;
    snapshot.plan_sweep_record_visit_count =
        profile.plan_sweep_record_visit_count;
    snapshot.max_plan_sweep_pass_count = profile.max_plan_sweep_pass_count;
    return snapshot;
}

}  // namespace repiu::engine
