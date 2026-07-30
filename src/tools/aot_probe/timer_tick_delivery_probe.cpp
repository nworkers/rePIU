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
    const bool policy =
        !platform::win32::ResolveTimerTickBacklogEnabled("") &&
        !platform::win32::ResolveTimerTickBacklogEnabled("0") &&
        platform::win32::ResolveTimerTickBacklogEnabled("1") &&
        platform::win32::ResolveTimerTickBacklogEnabled("on") &&
        platform::win32::ResolveTimerTickBacklogEnabled("true");

    // Shipping behaviour: three owed ticks become one injection and two losses,
    // because delivery is a single boolean.
    Win32TimerTickDeliveryCounters legacy;
    RecordTimerTicksDue(&legacy, 3U, false, false);
    const bool legacy_armed_once =
        !RecordTimerTickInjected(&legacy, false);
    RecordTimerTicksDue(&legacy, 2U, false, false);
    const bool legacy_armed_twice =
        !RecordTimerTickInjected(&legacy, false);
    const auto legacy_snapshot = SnapshotTimerTickDelivery(legacy);
    const bool coalescing =
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
    RecordTimerTicksDue(&outstanding, 1U, false, false);
    RecordTimerTicksDue(&outstanding, 4U, true, false);
    const auto outstanding_snapshot =
        SnapshotTimerTickDelivery(outstanding);
    const bool already_pending =
        outstanding_snapshot.due_total == 5U &&
        outstanding_snapshot.coalesced_total == 4U &&
        outstanding_snapshot.backlog == 1U &&
        PartitionHolds(outstanding);

    // Backlog mode keeps owed ticks and drains one per safe point.
    Win32TimerTickDeliveryCounters backlog;
    RecordTimerTicksDue(&backlog, 3U, false, true);
    const bool drain_first = RecordTimerTickInjected(&backlog, true);
    const bool drain_second = RecordTimerTickInjected(&backlog, true);
    const bool drain_last = RecordTimerTickInjected(&backlog, true);
    const auto backlog_snapshot = SnapshotTimerTickDelivery(backlog);
    const bool draining =
        drain_first && drain_second && !drain_last &&
        backlog_snapshot.due_total == 3U &&
        backlog_snapshot.injected_total == 3U &&
        backlog_snapshot.coalesced_total == 0U &&
        backlog_snapshot.backlog == 0U &&
        backlog_snapshot.max_backlog == 3U &&
        PartitionHolds(backlog);

    // The cap bounds how far into the past the guest can be parked, and the
    // excess is counted rather than delivered late.
    Win32TimerTickDeliveryCounters capped;
    RecordTimerTicksDue(&capped, kWin32TimerTickBacklogCapacity + 10U, false,
                        true);
    RecordTimerTicksDue(&capped, 5U, false, true);
    const auto capped_snapshot = SnapshotTimerTickDelivery(capped);
    const bool capping =
        capped_snapshot.backlog == kWin32TimerTickBacklogCapacity &&
        capped_snapshot.dropped_total == 15U &&
        capped_snapshot.max_backlog == kWin32TimerTickBacklogCapacity &&
        PartitionHolds(capped);

    // Abandoning delivery must account the owed ticks, not drop them out of the
    // identity.
    Win32TimerTickDeliveryCounters cleared;
    RecordTimerTicksDue(&cleared, 6U, false, true);
    RecordTimerTickBacklogCleared(&cleared);
    const auto cleared_snapshot = SnapshotTimerTickDelivery(cleared);
    const bool clearing =
        cleared_snapshot.backlog == 0U &&
        cleared_snapshot.dropped_total == 6U &&
        PartitionHolds(cleared);

    // Deferrals are delays, not losses, so they stay out of the partition.
    Win32TimerTickDeliveryCounters deferred;
    RecordTimerTicksDue(&deferred, 1U, false, true);
    RecordTimerTickDeferred(&deferred);
    RecordTimerTickDeferred(&deferred);
    RecordTimerTickInjected(&deferred, true);
    const auto deferred_snapshot = SnapshotTimerTickDelivery(deferred);
    const bool deferral =
        deferred_snapshot.deferred_total == 2U &&
        deferred_snapshot.injected_total == 1U &&
        deferred_snapshot.backlog == 0U &&
        PartitionHolds(deferred);

    RecordTimerTicksDue(nullptr, 1U, false, true);
    RecordTimerTickDeferred(nullptr);
    RecordTimerTickBacklogCleared(nullptr);
    const bool inert =
        !RecordTimerTickInjected(nullptr, true) &&
        SnapshotTimerTickDelivery(
            Win32TimerTickDeliveryCounters{}).due_total == 0U;

    const bool all = policy && coalescing && already_pending && draining &&
        capping && clearing && deferral && inert;
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
              << "\ntimer_tick_delivery_inert="
              << (inert ? "true" : "false")
              << "\ntimer_tick_delivery_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
