#pragma once

#include <cstdint>

namespace repiu::platform::win32
{

// Splits one Glide host-thread rendezvous into waiting and work (Task 333).
//
// Task 331 measured the Glide gate at 60.78% of Release guest wall clock, about
// 1.85ms per gate entry against only 275 frames in 60 seconds. Whether that is
// host CPU work or waiting for the host thread decides the remedy completely,
// and Tasks 326 and 327 already showed that guessing between those two gets it
// backwards.
//
//   guest: t0 enter -> t1 publish + notify ................. wait ...... t4
//   host:                    t2 (pump picks it up) -> t3 done -> notify
//
// `wake` (t2 - t1) is the host pump cadence; `work` (t3 - t2) is real CPU work.
//
// Deliberately uses no atomics: waiting is the quantity under measurement, so
// instrumentation must not perturb it, and the backend's existing mutex and
// condition variable already supply the happens-before between the two threads.
// Only one command is ever in flight, so three scalar handoff timestamps are
// enough.
//
// See docs/design/20260728-333-glide-gate-rendezvous-timing.md.
struct Win32GlideGateTimingProfile
{
    bool enabled = false;

    // Handoff timestamps. Each is written by one thread before the
    // synchronization point the other thread waits on.
    std::uint64_t publish_cycles = 0;
    std::uint64_t host_start_cycles = 0;
    std::uint64_t host_finish_cycles = 0;

    std::uint32_t rendezvous_count = 0;
    std::uint64_t queue_cycles = 0;
    std::uint64_t wake_cycles = 0;
    std::uint64_t work_cycles = 0;
    std::uint64_t complete_cycles = 0;
    std::uint64_t total_cycles = 0;

    // Commands issued while already on the host thread take no rendezvous. A
    // large share here would mean part of the gate cost was never waiting.
    std::uint32_t direct_count = 0;
    std::uint64_t direct_work_cycles = 0;

    std::uint64_t max_wake_cycles = 0;
    std::uint64_t max_work_cycles = 0;
    std::uint64_t max_total_cycles = 0;

    // Cross-core TSC reads can go backwards; such samples clamp to zero and are
    // counted rather than silently distorting the totals.
    std::uint32_t clamped_sample_count = 0;
};

struct Win32GlideGateTimingSnapshot
{
    bool enabled = false;
    std::uint32_t rendezvous_count = 0;
    std::uint64_t queue_cycles = 0;
    std::uint64_t wake_cycles = 0;
    std::uint64_t work_cycles = 0;
    std::uint64_t complete_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint32_t direct_count = 0;
    std::uint64_t direct_work_cycles = 0;
    std::uint64_t max_wake_cycles = 0;
    std::uint64_t max_work_cycles = 0;
    std::uint64_t max_total_cycles = 0;
    std::uint32_t clamped_sample_count = 0;
    // Derived: total minus the four named intervals, so an incomplete
    // decomposition is visible instead of being absorbed silently.
    std::uint64_t residual_cycles = 0;
};

// Task 419. The gate timing above measures how long the rendezvous waited;
// this counts how often a spin caught that wait before the condition variable
// had to, split by which side spun. `budget_microseconds` is the resolved
// `REPIU_GLIDE_RENDEZVOUS_SPIN_US`, where zero means the spin is disabled and
// every count is expected to read zero.
// See docs/design/20260805-419-glide-rendezvous-spin-wait.md.
struct Win32GlideRendezvousSpinSnapshot
{
    std::uint64_t guest_hit = 0;
    std::uint64_t guest_miss = 0;
    std::uint64_t host_hit = 0;
    std::uint64_t host_miss = 0;
    std::uint32_t budget_microseconds = 0;
};

std::uint64_t ReadGlideGateTimingCycles();

// Difference that clamps a backwards TSC read to zero and counts it.
std::uint64_t GlideGateTimingDelta(Win32GlideGateTimingProfile* profile,
                                   std::uint64_t start,
                                   std::uint64_t end);

// Guest side, immediately after publishing the command and notifying.
void RecordGlideGatePublish(Win32GlideGateTimingProfile* profile,
                            std::uint64_t enter_cycles,
                            std::uint64_t publish_cycles);

// Host side, around executing one command.
void RecordGlideGateHostCommand(Win32GlideGateTimingProfile* profile,
                                std::uint64_t start_cycles,
                                std::uint64_t finish_cycles);

// Guest side, after the completion wait returns.
void RecordGlideGateResume(Win32GlideGateTimingProfile* profile,
                           std::uint64_t enter_cycles,
                           std::uint64_t resume_cycles);

// A command run inline because the caller already was the host thread.
void RecordGlideGateDirectCommand(Win32GlideGateTimingProfile* profile,
                                  std::uint64_t cycles);

Win32GlideGateTimingSnapshot SnapshotGlideGateTiming(
    const Win32GlideGateTimingProfile& profile);

}  // namespace repiu::platform::win32
