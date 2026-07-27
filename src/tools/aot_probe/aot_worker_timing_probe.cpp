#include "aot_worker_timing_probe.h"

#include "repiu/platform/win32/aot_worker_timing.h"

#include <cstdint>
#include <iostream>
#include <memory>

namespace repiu::tools
{

bool RunAotWorkerTimingProbe()
{
    using namespace repiu::platform::win32;

    // Replay one rendezvous with synthetic timestamps, in the exact order the
    // guest and worker record them.
    auto profile = std::make_unique<Win32AotWorkerTimingProfile>();
    RecordAotWorkerRequestSignal(profile.get(), 1000U);   // guest T0
    RecordAotWorkerWake(profile.get(), 1300U);            // worker T1
    RecordAotWorkerSegmentTable(profile.get(), 50U);
    RecordAotWorkerAppend(profile.get(), 700U);
    RecordAotWorkerCompleteSignal(profile.get(), 2100U);  // worker T2
    RecordAotWorkerGuestResume(profile.get(), 1000U, 2400U);  // guest T3

    const Win32AotWorkerTimingSnapshot one =
        SnapshotAotWorkerTiming(*profile);
    const bool single_rendezvous =
        one.enabled &&
        one.translate_count == 1U &&
        one.wake_latency_cycles == 300U &&
        one.segment_table_cycles == 50U &&
        one.append_cycles == 700U &&
        one.complete_latency_cycles == 300U &&
        one.guest_total_cycles == 1400U &&
        one.clamped_sample_count == 0U &&
        // The parts must never exceed the whole, or the reported residual
        // would go negative and be silently clamped.
        one.wake_latency_cycles + one.segment_table_cycles +
            one.append_cycles + one.complete_latency_cycles <=
                one.guest_total_cycles;

    // A second rendezvous accumulates and records the request gap and maxima.
    RecordAotWorkerRequestSignal(profile.get(), 5000U);
    RecordAotWorkerWake(profile.get(), 5100U);
    RecordAotWorkerAppend(profile.get(), 4000U);
    RecordAotWorkerCompleteSignal(profile.get(), 9200U);
    RecordAotWorkerGuestResume(profile.get(), 5000U, 9300U);
    const Win32AotWorkerTimingSnapshot two =
        SnapshotAotWorkerTiming(*profile);
    const bool accumulation =
        two.translate_count == 2U &&
        two.append_cycles == 4700U &&
        two.max_append_cycles == 4000U &&
        two.max_wake_latency_cycles == 300U &&
        two.max_guest_total_cycles == 4300U &&
        two.request_gap_cycles == 4000U;

    // A backwards TSC read clamps to zero and is counted rather than wrapping
    // into an enormous unsigned value.
    auto clamped = std::make_unique<Win32AotWorkerTimingProfile>();
    RecordAotWorkerRequestSignal(clamped.get(), 9000U);
    RecordAotWorkerWake(clamped.get(), 8000U);
    RecordAotWorkerCompleteSignal(clamped.get(), 9500U);
    RecordAotWorkerGuestResume(clamped.get(), 9000U, 9400U);
    const Win32AotWorkerTimingSnapshot clamp_snapshot =
        SnapshotAotWorkerTiming(*clamped);
    const bool clamping =
        clamp_snapshot.wake_latency_cycles == 0U &&
        clamp_snapshot.complete_latency_cycles == 0U &&
        clamp_snapshot.guest_total_cycles == 400U &&
        clamp_snapshot.clamped_sample_count == 2U;

    // Non-translate operations share the event pair and are counted only.
    auto other = std::make_unique<Win32AotWorkerTimingProfile>();
    RecordAotWorkerOtherOperation(other.get());
    RecordAotWorkerOtherOperation(other.get());
    const Win32AotWorkerTimingSnapshot other_snapshot =
        SnapshotAotWorkerTiming(*other);
    const bool other_operations =
        other_snapshot.enabled &&
        other_snapshot.other_operation_count == 2U &&
        other_snapshot.translate_count == 0U &&
        other_snapshot.guest_total_cycles == 0U;

    // Every entry point must tolerate a null profile.
    RecordAotWorkerRequestSignal(nullptr, 1U);
    RecordAotWorkerWake(nullptr, 2U);
    RecordAotWorkerSegmentTable(nullptr, 3U);
    RecordAotWorkerAppend(nullptr, 4U);
    RecordAotWorkerCompleteSignal(nullptr, 5U);
    RecordAotWorkerGuestResume(nullptr, 1U, 6U);
    RecordAotWorkerOtherOperation(nullptr);
    const Win32AotWorkerTimingSnapshot empty =
        SnapshotAotWorkerTiming(Win32AotWorkerTimingProfile{});
    const bool disabled =
        !empty.enabled && empty.translate_count == 0U &&
        empty.guest_total_cycles == 0U &&
        AotWorkerTimingDelta(nullptr, 10U, 5U) == 0U &&
        AotWorkerTimingDelta(nullptr, 5U, 10U) == 5U;

    const bool all = single_rendezvous && accumulation && clamping &&
        other_operations && disabled;

    std::cout
        << "aot_worker_timing_single_rendezvous="
        << (single_rendezvous ? "true" : "false")
        << "\naot_worker_timing_accumulation="
        << (accumulation ? "true" : "false")
        << "\naot_worker_timing_clamping="
        << (clamping ? "true" : "false")
        << "\naot_worker_timing_other_operations="
        << (other_operations ? "true" : "false")
        << "\naot_worker_timing_disabled="
        << (disabled ? "true" : "false")
        << "\naot_worker_timing_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
