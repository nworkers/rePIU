#include "timer_tick_delivery_probe.h"

#include "repiu/platform/win32/timer_tick_delivery.h"

#include <iostream>

namespace repiu::tools
{
namespace
{

using platform::win32::kWin32TimerTickBacklogCapacity;
using platform::win32::RecordTimerTickBacklogCleared;
using platform::win32::RecordTimerTickDeferred;
using platform::win32::RecordTimerTickInjected;
using platform::win32::RecordTimerTicksDue;
using platform::win32::SnapshotTimerTickDelivery;
using platform::win32::Win32TimerTickDeliveryCounters;

// The gate the whole decomposition rests on: every owed tick must end up in
// exactly one of delivered, coalesced away, dropped, or still owed.
bool PartitionHolds(const Win32TimerTickDeliveryCounters& counters)
{
    const auto snapshot = SnapshotTimerTickDelivery(counters);
    return snapshot.due_total ==
        snapshot.injected_total + snapshot.coalesced_total +
            snapshot.dropped_total + snapshot.backlog;
}

}  // namespace

bool RunTimerTickDeliveryProbe()
{
    // Task 432: on by default, so only an explicit off disables it. Unset
    // (nullptr) and unrecognised values must resolve to on -- a typo in the
    // variable must not silently restore the tick-losing path.
    const bool policy =
        platform::win32::ResolveTimerTickBacklogEnabled(nullptr) &&
        platform::win32::ResolveTimerTickBacklogEnabled("") &&
        platform::win32::ResolveTimerTickBacklogEnabled("yes") &&
        !platform::win32::ResolveTimerTickBacklogEnabled("0") &&
        !platform::win32::ResolveTimerTickBacklogEnabled("off") &&
        !platform::win32::ResolveTimerTickBacklogEnabled("false") &&
        platform::win32::ResolveTimerTickBacklogEnabled("1") &&
        platform::win32::ResolveTimerTickBacklogEnabled("on") &&
        platform::win32::ResolveTimerTickBacklogEnabled("true");

    // The opt-out path, kept as the regression control: three owed ticks become
    // one injection and two losses, because delivery is a single boolean.
    Win32TimerTickDeliveryCounters legacy;
    const std::uint32_t legacy_retained_first =
        RecordTimerTicksDue(&legacy, 3U, false, false, false);
    const bool legacy_armed_once =
        !RecordTimerTickInjected(&legacy, false);
    const std::uint32_t legacy_retained_second =
        RecordTimerTicksDue(&legacy, 2U, false, false, false);
    const bool legacy_armed_twice =
        !RecordTimerTickInjected(&legacy, false);
    const auto legacy_snapshot = SnapshotTimerTickDelivery(legacy);
    const bool coalescing =
        legacy_retained_first == 1U && legacy_retained_second == 1U &&
        legacy_armed_once && legacy_armed_twice &&
        legacy_snapshot.due_total == 5U &&
        legacy_snapshot.injected_total == 2U &&
        legacy_snapshot.coalesced_total == 3U &&
        legacy_snapshot.backlog == 0U &&
        PartitionHolds(legacy);

    // A poll arriving while a tick is still outstanding loses all of its own
    // ticks, since even the one it would have kept is a duplicate of the tick
    // not yet taken.
    Win32TimerTickDeliveryCounters outstanding;
    const std::uint32_t outstanding_retained_first =
        RecordTimerTicksDue(&outstanding, 1U, false, false, false);
    const std::uint32_t outstanding_retained_second =
        RecordTimerTicksDue(&outstanding, 4U, true, false, false);
    const auto outstanding_snapshot =
        SnapshotTimerTickDelivery(outstanding);
    const bool already_pending =
        outstanding_retained_first == 1U &&
        outstanding_retained_second == 0U &&
        outstanding_snapshot.due_total == 5U &&
        outstanding_snapshot.coalesced_total == 4U &&
        outstanding_snapshot.backlog == 1U &&
        PartitionHolds(outstanding);

    // Backlog mode keeps owed ticks and drains one per safe point.
    Win32TimerTickDeliveryCounters backlog;
    const std::uint32_t backlog_retained =
        RecordTimerTicksDue(&backlog, 3U, false, true, false);
    const bool drain_first = RecordTimerTickInjected(&backlog, true);
    const bool drain_second = RecordTimerTickInjected(&backlog, true);
    const bool drain_last = RecordTimerTickInjected(&backlog, true);
    const auto backlog_snapshot = SnapshotTimerTickDelivery(backlog);
    const bool draining =
        backlog_retained == 3U && drain_first && drain_second && !drain_last &&
        backlog_snapshot.due_total == 3U &&
        backlog_snapshot.injected_total == 3U &&
        backlog_snapshot.coalesced_total == 0U &&
        backlog_snapshot.backlog == 0U &&
        backlog_snapshot.max_backlog == 3U &&
        PartitionHolds(backlog);

    // The cap bounds how far into the past the guest can be parked, and the
    // excess is counted rather than delivered late.
    Win32TimerTickDeliveryCounters capped;
    const std::uint32_t capped_retained_first = RecordTimerTicksDue(
        &capped, kWin32TimerTickBacklogCapacity + 10U, false, true, false);
    const std::uint32_t capped_retained_second =
        RecordTimerTicksDue(&capped, 5U, false, true, false);
    const auto capped_snapshot = SnapshotTimerTickDelivery(capped);
    const bool capping =
        capped_retained_first == kWin32TimerTickBacklogCapacity &&
        capped_retained_second == 0U &&
        capped_snapshot.backlog == kWin32TimerTickBacklogCapacity &&
        capped_snapshot.dropped_total == 15U &&
        capped_snapshot.max_backlog == kWin32TimerTickBacklogCapacity &&
        PartitionHolds(capped);

    // Abandoning delivery must account the owed ticks, not drop them out of the
    // identity.
    Win32TimerTickDeliveryCounters cleared;
    RecordTimerTicksDue(&cleared, 6U, false, true, false);
    RecordTimerTickBacklogCleared(&cleared);
    const auto cleared_snapshot = SnapshotTimerTickDelivery(cleared);
    const bool clearing =
        cleared_snapshot.backlog == 0U &&
        cleared_snapshot.dropped_total == 6U &&
        PartitionHolds(cleared);

    // Deferrals are delays, not losses, so they stay out of the partition.
    Win32TimerTickDeliveryCounters deferred;
    RecordTimerTicksDue(&deferred, 1U, false, true, false);
    RecordTimerTickDeferred(&deferred);
    RecordTimerTickDeferred(&deferred);
    RecordTimerTickInjected(&deferred, true);
    const auto deferred_snapshot = SnapshotTimerTickDelivery(deferred);
    const bool deferral =
        deferred_snapshot.deferred_total == 2U &&
        deferred_snapshot.injected_total == 1U &&
        deferred_snapshot.backlog == 0U &&
        PartitionHolds(deferred);

    // Task 431: in-gate ticks are a subset of the same partition, never a
    // separate bucket -- a loss counted twice would overstate the gate's share,
    // which is the whole quantity the attribution turns on.
    Win32TimerTickDeliveryCounters gated;
    RecordTimerTicksDue(&gated, 3U, false, false, true);
    RecordTimerTickInjected(&gated, false);
    RecordTimerTicksDue(&gated, 4U, false, false, false);
    const auto gated_snapshot = SnapshotTimerTickDelivery(gated);
    const bool gate_attribution =
        gated_snapshot.due_total == 7U &&
        gated_snapshot.due_in_gate_total == 3U &&
        gated_snapshot.coalesced_total == 5U &&
        gated_snapshot.coalesced_in_gate_total == 2U &&
        gated_snapshot.coalesced_in_gate_total <=
            gated_snapshot.coalesced_total &&
        gated_snapshot.due_in_gate_total <= gated_snapshot.due_total &&
        PartitionHolds(gated);

    RecordTimerTicksDue(nullptr, 1U, false, true, true);
    RecordTimerTickDeferred(nullptr);
    RecordTimerTickBacklogCleared(nullptr);
    const bool inert =
        !RecordTimerTickInjected(nullptr, true) &&
        SnapshotTimerTickDelivery(
            Win32TimerTickDeliveryCounters{}).due_total == 0U;

    const bool all = policy && coalescing && already_pending && draining &&
        capping && clearing && deferral && gate_attribution && inert;
    std::cout << "timer_tick_delivery_policy="
              << (policy ? "true" : "false")
              << "\ntimer_tick_delivery_coalescing="
              << (coalescing ? "true" : "false")
              << "\ntimer_tick_delivery_already_pending="
              << (already_pending ? "true" : "false")
              << "\ntimer_tick_delivery_draining="
              << (draining ? "true" : "false")
              << "\ntimer_tick_delivery_capping="
              << (capping ? "true" : "false")
              << "\ntimer_tick_delivery_clearing="
              << (clearing ? "true" : "false")
              << "\ntimer_tick_delivery_deferral="
              << (deferral ? "true" : "false")
              << "\ntimer_tick_delivery_gate_attribution="
              << (gate_attribution ? "true" : "false")
              << "\ntimer_tick_delivery_inert="
              << (inert ? "true" : "false")
              << "\ntimer_tick_delivery_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
