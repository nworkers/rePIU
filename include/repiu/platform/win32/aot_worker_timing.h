#pragma once

#include <cstdint>

namespace repiu::platform::win32
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
struct Win32AotWorkerTimingProfile
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
};

struct Win32AotWorkerTimingSnapshot
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
};

std::uint64_t ReadAotWorkerTimingCycles();

// Difference that clamps a backwards TSC read to zero and counts it.
std::uint64_t AotWorkerTimingDelta(Win32AotWorkerTimingProfile* profile,
                                   std::uint64_t start,
                                   std::uint64_t end);

// Guest side, immediately before SetEvent(request).
void RecordAotWorkerRequestSignal(Win32AotWorkerTimingProfile* profile,
                                  std::uint64_t signal_cycles);

// Worker side, immediately after waking for a translate operation.
void RecordAotWorkerWake(Win32AotWorkerTimingProfile* profile,
                         std::uint64_t wake_cycles);

void RecordAotWorkerSegmentTable(Win32AotWorkerTimingProfile* profile,
                                 std::uint64_t cycles);
void RecordAotWorkerAppend(Win32AotWorkerTimingProfile* profile,
                           std::uint64_t cycles);

// Worker side, immediately before SetEvent(complete).
void RecordAotWorkerCompleteSignal(Win32AotWorkerTimingProfile* profile,
                                   std::uint64_t signal_cycles);

// Guest side, after WaitForSingleObject returns.
void RecordAotWorkerGuestResume(Win32AotWorkerTimingProfile* profile,
                                std::uint64_t request_cycles,
                                std::uint64_t resume_cycles);

void RecordAotWorkerOtherOperation(Win32AotWorkerTimingProfile* profile);

Win32AotWorkerTimingSnapshot SnapshotAotWorkerTiming(
    const Win32AotWorkerTimingProfile& profile);

}  // namespace repiu::platform::win32
