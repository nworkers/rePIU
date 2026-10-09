#include "timer_tick_delivery_probe.h"

#include "repiu/engine/timer_tick_delivery.h"

#include <iostream>

namespace repiu::tools
{
namespace
{

using engine::kTimerTickBacklogCapacity;
using engine::RecordTimerTickBacklogCleared;
using engine::RecordTimerTickDeferred;
using engine::RecordTimerTickInjected;
using engine::RecordTimerTicksDue;
using engine::SnapshotTimerTickDelivery;
using engine::TimerTickDeliveryCounters;

// The gate the whole decomposition rests on: every owed tick must end up in
// exactly one of delivered, dropped, or still owed.
bool PartitionHolds(const TimerTickDeliveryCounters& counters)
{
    const auto snapshot = SnapshotTimerTickDelivery(counters);
    return snapshot.due_total ==
        snapshot.injected_total + snapshot.dropped_total + snapshot.backlog;
}

}  // namespace

bool RunTimerTickDeliveryProbe()
{
    // The backlog keeps owed ticks and drains one per safe point.
    TimerTickDeliveryCounters backlog;
    const std::uint32_t backlog_retained =
        RecordTimerTicksDue(&backlog, 3U, false);
    const bool drain_first = RecordTimerTickInjected(&backlog);
    const bool drain_second = RecordTimerTickInjected(&backlog);
    const bool drain_last = RecordTimerTickInjected(&backlog);
    const auto backlog_snapshot = SnapshotTimerTickDelivery(backlog);
    const bool draining =
        backlog_retained == 3U && drain_first && drain_second && !drain_last &&
        backlog_snapshot.due_total == 3U &&
        backlog_snapshot.injected_total == 3U &&
        backlog_snapshot.backlog == 0U &&
        backlog_snapshot.max_backlog == 3U &&
        PartitionHolds(backlog);

    // The cap bounds how far into the past the guest can be parked, and the
    // excess is counted rather than delivered late.
    TimerTickDeliveryCounters capped;
    const std::uint32_t capped_retained_first = RecordTimerTicksDue(
        &capped, kTimerTickBacklogCapacity + 10U, false);
    const std::uint32_t capped_retained_second =
        RecordTimerTicksDue(&capped, 5U, false);
    const auto capped_snapshot = SnapshotTimerTickDelivery(capped);
    const bool capping =
        capped_retained_first == kTimerTickBacklogCapacity &&
        capped_retained_second == 0U &&
        capped_snapshot.backlog == kTimerTickBacklogCapacity &&
        capped_snapshot.dropped_total == 15U &&
        capped_snapshot.max_backlog == kTimerTickBacklogCapacity &&
        PartitionHolds(capped);

    // Abandoning delivery must account the owed ticks, not drop them out of the
    // identity.
    TimerTickDeliveryCounters cleared;
    RecordTimerTicksDue(&cleared, 6U, false);
    RecordTimerTickBacklogCleared(&cleared);
    const auto cleared_snapshot = SnapshotTimerTickDelivery(cleared);
    const bool clearing =
        cleared_snapshot.backlog == 0U &&
        cleared_snapshot.dropped_total == 6U &&
        PartitionHolds(cleared);

    // Deferrals are delays, not losses, so they stay out of the partition.
    TimerTickDeliveryCounters deferred;
    RecordTimerTicksDue(&deferred, 1U, false);
    RecordTimerTickDeferred(&deferred);
    RecordTimerTickDeferred(&deferred);
    RecordTimerTickInjected(&deferred);
    const auto deferred_snapshot = SnapshotTimerTickDelivery(deferred);
    const bool deferral =
        deferred_snapshot.deferred_total == 2U &&
        deferred_snapshot.injected_total == 1U &&
        deferred_snapshot.backlog == 0U &&
        PartitionHolds(deferred);

    // Task 431: in-gate ticks are a subset of the owed ticks, never a separate
    // bucket of the partition.
    TimerTickDeliveryCounters gated;
    RecordTimerTicksDue(&gated, 3U, true);
    RecordTimerTickInjected(&gated);
    RecordTimerTicksDue(&gated, 4U, false);
    const auto gated_snapshot = SnapshotTimerTickDelivery(gated);
    const bool gate_attribution =
        gated_snapshot.due_total == 7U &&
        gated_snapshot.due_in_gate_total == 3U &&
        gated_snapshot.injected_total == 1U &&
        gated_snapshot.backlog == 6U &&
        PartitionHolds(gated);

    RecordTimerTicksDue(nullptr, 1U, true);
    RecordTimerTickDeferred(nullptr);
    RecordTimerTickBacklogCleared(nullptr);
    const bool inert =
        !RecordTimerTickInjected(nullptr) &&
        SnapshotTimerTickDelivery(
            TimerTickDeliveryCounters{}).due_total == 0U;

    const bool all = draining && capping && clearing && deferral &&
        gate_attribution && inert;
    std::cout << "timer_tick_delivery_draining="
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
