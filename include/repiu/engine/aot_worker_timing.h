#pragma once

#include "repiu/runtime/aot_translation_plan.h"

#include <cstdint>

namespace repiu::engine
{

// Splits one dynamic-translation rendezvous into scheduling latency and worker
// CPU work (Task 327).
//
// Task 326 measured 230 translations holding 61.6% of guest-thread wall clock at
// about 175ms each, and found that RequestAotDynamicTranslation signals a worker
// then blocks in WaitForSingleObject(INFINITE). The measured time is guest-thread
// BLOCKED time, so what it consists of decides the remedy: worker CPU work means
// making translation cheaper or smaller, while scheduling latency means removing
// or asynchronizing the rendezvous.
//
//   guest: T0 -> SetEvent(request) ......................... wait ... T3
//   worker:      T1 (wake) -> segment table -> append -> T2 -> SetEvent(complete)
//
// Deliberately uses no atomics and no locks. Latency is the quantity under
// measurement, so instrumentation must not perturb it; correctness rests on the
// happens-before that SetEvent/WaitForSingleObject already supply.
//
// See docs/design/20260727-327-translation-worker-timing.md.
struct AotWorkerTimingProfile
{
    bool enabled = false;

    // Handoff timestamps. Written by one thread before signalling, read by the
    // other after waking.
    std::uint64_t request_signal_cycles = 0;
    std::uint64_t complete_signal_cycles = 0;

    std::uint32_t translate_count = 0;
    std::uint64_t wake_latency_cycles = 0;
    std::uint64_t segment_table_cycles = 0;
    std::uint64_t append_cycles = 0;
    std::uint64_t complete_latency_cycles = 0;
    std::uint64_t guest_total_cycles = 0;

    std::uint64_t max_wake_latency_cycles = 0;
    std::uint64_t max_append_cycles = 0;
    std::uint64_t max_guest_total_cycles = 0;

    // A large wake latency can mean the worker was still serving an earlier
    // request rather than that scheduling is slow, so spacing is recorded to
    // tell queueing apart from contention.
    std::uint64_t last_request_cycles = 0;
    std::uint64_t request_gap_cycles = 0;

    // Cross-core TSC reads can in principle go backwards; such samples clamp to
    // zero and are counted rather than silently distorting the totals.
    std::uint32_t clamped_sample_count = 0;

    // kPatchInlineCache and kRetireGuestPage share the same event pair. A large
    // value means rendezvous cost exists outside translation too.
    std::uint32_t other_operation_count = 0;

    // Task 328: phases inside AppendDynamicAotTranslation, which Task 327
    // measured at 101.00% of the rendezvous. Worker thread only, so no atomics.
    std::uint32_t append_phase_count = 0;
    std::uint64_t arena_snapshot_cycles = 0;
    std::uint64_t plan_build_cycles = 0;
    std::uint64_t image_emit_cycles = 0;
    std::uint64_t validate_cycles = 0;
    std::uint64_t placement_cycles = 0;
    std::uint64_t max_arena_snapshot_cycles = 0;
    // Scale of one translation, so "shrink the translation unit" can be judged
    // as an available remedy rather than assumed.
    std::uint64_t plan_block_total = 0;
    std::uint64_t plan_instruction_total = 0;
    std::uint64_t emitted_byte_total = 0;
    std::uint64_t snapshot_byte_total = 0;
    std::uint32_t max_plan_instruction_count = 0;

    // Task 330: the platform-neutral plan builder's own stages, accumulated
    // here so `plan_build_cycles` above can be decomposed. The builder measures
    // these; this layer only sums them.
    std::uint32_t plan_profile_count = 0;
    std::uint64_t plan_decoder_init_cycles = 0;
    std::uint64_t plan_decode_cycles = 0;
    std::uint64_t plan_record_build_cycles = 0;
    std::uint64_t plan_classify_cycles = 0;
    std::uint64_t plan_walk_cycles = 0;
    std::uint64_t plan_sweep_cycles = 0;
    std::uint64_t plan_total_cycles = 0;
    std::uint64_t plan_decode_count = 0;
    std::uint64_t plan_record_count = 0;
    std::uint64_t plan_sweep_pass_count = 0;
    std::uint64_t plan_sweep_record_visit_count = 0;
    std::uint32_t max_plan_sweep_pass_count = 0;
};

// One append's phase timings. Phases not reached stay zero.
struct AotAppendPhaseSample
{
    std::uint64_t arena_snapshot_cycles = 0;
    std::uint64_t plan_build_cycles = 0;
    std::uint64_t image_emit_cycles = 0;
    std::uint64_t validate_cycles = 0;
    std::uint64_t placement_cycles = 0;
};

struct AotAppendScaleSample
{
    std::uint32_t plan_block_count = 0;
    std::uint32_t plan_instruction_count = 0;
    std::uint32_t emitted_bytes = 0;
    // Bytes actually copied out of the guest arena. Zero since Task 329
    // replaced the full-arena snapshot with a direct reference, so a nonzero
    // value here means a copy has come back.
    std::uint32_t snapshot_bytes = 0;
};

struct AotWorkerTimingSnapshot
{
    bool enabled = false;
    std::uint32_t translate_count = 0;
    std::uint64_t wake_latency_cycles = 0;
    std::uint64_t segment_table_cycles = 0;
    std::uint64_t append_cycles = 0;
    std::uint64_t complete_latency_cycles = 0;
    std::uint64_t guest_total_cycles = 0;
    std::uint64_t max_wake_latency_cycles = 0;
    std::uint64_t max_append_cycles = 0;
    std::uint64_t max_guest_total_cycles = 0;
    std::uint64_t request_gap_cycles = 0;
    std::uint32_t clamped_sample_count = 0;
    std::uint32_t other_operation_count = 0;
    std::uint32_t append_phase_count = 0;
    std::uint64_t arena_snapshot_cycles = 0;
    std::uint64_t plan_build_cycles = 0;
    std::uint64_t image_emit_cycles = 0;
    std::uint64_t validate_cycles = 0;
    std::uint64_t placement_cycles = 0;
    std::uint64_t max_arena_snapshot_cycles = 0;
    std::uint64_t plan_block_total = 0;
    std::uint64_t plan_instruction_total = 0;
    std::uint64_t emitted_byte_total = 0;
    std::uint64_t snapshot_byte_total = 0;
    std::uint32_t max_plan_instruction_count = 0;
    std::uint32_t plan_profile_count = 0;
    std::uint64_t plan_decoder_init_cycles = 0;
    std::uint64_t plan_decode_cycles = 0;
    std::uint64_t plan_record_build_cycles = 0;
    std::uint64_t plan_classify_cycles = 0;
    std::uint64_t plan_walk_cycles = 0;
    std::uint64_t plan_sweep_cycles = 0;
    std::uint64_t plan_total_cycles = 0;
    std::uint64_t plan_decode_count = 0;
    std::uint64_t plan_record_count = 0;
    std::uint64_t plan_sweep_pass_count = 0;
    std::uint64_t plan_sweep_record_visit_count = 0;
    std::uint32_t max_plan_sweep_pass_count = 0;
};

std::uint64_t ReadAotWorkerTimingCycles();

// Difference that clamps a backwards TSC read to zero and counts it.
std::uint64_t AotWorkerTimingDelta(AotWorkerTimingProfile* profile,
                                   std::uint64_t start,
                                   std::uint64_t end);

// Guest side, immediately before SetEvent(request).
void RecordAotWorkerRequestSignal(AotWorkerTimingProfile* profile,
                                  std::uint64_t signal_cycles);

// Worker side, immediately after waking for a translate operation.
void RecordAotWorkerWake(AotWorkerTimingProfile* profile,
                         std::uint64_t wake_cycles);

void RecordAotWorkerSegmentTable(AotWorkerTimingProfile* profile,
                                 std::uint64_t cycles);
void RecordAotWorkerAppend(AotWorkerTimingProfile* profile,
                           std::uint64_t cycles);

// Worker side, immediately before SetEvent(complete).
void RecordAotWorkerCompleteSignal(AotWorkerTimingProfile* profile,
                                   std::uint64_t signal_cycles);

// Guest side, after WaitForSingleObject returns.
void RecordAotWorkerGuestResume(AotWorkerTimingProfile* profile,
                                std::uint64_t request_cycles,
                                std::uint64_t resume_cycles);

void RecordAotWorkerOtherOperation(AotWorkerTimingProfile* profile);

// Task 328. Called once per append, on the worker thread, with whatever phases
// were reached before an early return.
void RecordAotAppendPhases(AotWorkerTimingProfile* profile,
                           const AotAppendPhaseSample& phases);
void RecordAotAppendScale(AotWorkerTimingProfile* profile,
                          const AotAppendScaleSample& scale);

// Task 330. Accumulates one plan build's neutral stage profile. Ignored when
// the plan builder was called without profiling.
void RecordAotPlanBuildProfile(AotWorkerTimingProfile* profile,
                               const runtime::AotPlanBuildProfile& plan);

AotWorkerTimingSnapshot SnapshotAotWorkerTiming(
    const AotWorkerTimingProfile& profile);

}  // namespace repiu::engine
