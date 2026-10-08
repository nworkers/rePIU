#include "repiu/engine/timer_tick_delivery.h"

#include <algorithm>
#include <thread>

namespace repiu::engine
{
namespace
{

void RaiseMaximum(std::atomic<std::uint32_t>* maximum, std::uint32_t value)
{
    std::uint32_t observed = maximum->load(std::memory_order_relaxed);
    while (value > observed &&
           !maximum->compare_exchange_weak(observed, value,
                                           std::memory_order_relaxed))
    {
    }
}

}  // namespace

TimerTickDeliveryGuard::TimerTickDeliveryGuard(
    std::atomic_flag* lock) : lock_(lock)
{
    if (lock_ == nullptr)
    {
        return;
    }
    while (lock_->test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

TimerTickDeliveryGuard::~TimerTickDeliveryGuard()
{
    Release();
}

void TimerTickDeliveryGuard::Release()
{
    if (lock_ != nullptr)
    {
        lock_->clear(std::memory_order_release);
        lock_ = nullptr;
    }
}

std::uint32_t RecordTimerTicksDue(
    TimerTickDeliveryCounters* counters,
    std::uint32_t due,
    bool in_gate)
{
    if (counters == nullptr || due == 0U)
    {
        return 0U;
    }
    counters->due_total.fetch_add(due, std::memory_order_relaxed);
    if (in_gate)
    {
        counters->due_in_gate_total.fetch_add(due, std::memory_order_relaxed);
    }

    // Keep the owed ticks, bounded. Beyond the cap the guest would be parked
    // ever further in the past, so the excess is counted and dropped rather
    // than delivered late enough to be meaningless.
    std::uint32_t backlog = counters->backlog.load(std::memory_order_relaxed);
    const std::uint32_t room = kTimerTickBacklogCapacity > backlog
        ? kTimerTickBacklogCapacity - backlog
        : 0U;
    const std::uint32_t accepted = std::min(due, room);
    if (due > accepted)
    {
        counters->dropped_total.fetch_add(due - accepted,
                                          std::memory_order_relaxed);
    }
    backlog += accepted;
    counters->backlog.store(backlog, std::memory_order_relaxed);
    RaiseMaximum(&counters->max_backlog, backlog);
    return accepted;
}

bool RecordTimerTickInjected(TimerTickDeliveryCounters* counters)
{
    if (counters == nullptr)
    {
        return false;
    }
    counters->injected_total.fetch_add(1U, std::memory_order_relaxed);

    std::uint32_t backlog = counters->backlog.load(std::memory_order_relaxed);
    if (backlog != 0U)
    {
        --backlog;
        counters->backlog.store(backlog, std::memory_order_relaxed);
    }
    return backlog != 0U;
}

void RecordTimerTickDeferred(TimerTickDeliveryCounters* counters)
{
    if (counters == nullptr)
    {
        return;
    }
    counters->deferred_total.fetch_add(1U, std::memory_order_relaxed);
}

void RecordTimerTickBacklogCleared(
    TimerTickDeliveryCounters* counters)
{
    if (counters == nullptr)
    {
        return;
    }
    const std::uint32_t backlog =
        counters->backlog.exchange(0U, std::memory_order_relaxed);
    if (backlog != 0U)
    {
        counters->dropped_total.fetch_add(backlog, std::memory_order_relaxed);
    }
}

TimerTickDeliverySnapshot SnapshotTimerTickDelivery(
    const TimerTickDeliveryCounters& counters)
{
    TimerTickDeliverySnapshot snapshot;
    snapshot.due_total = counters.due_total.load(std::memory_order_relaxed);
    snapshot.injected_total =
        counters.injected_total.load(std::memory_order_relaxed);
    snapshot.dropped_total =
        counters.dropped_total.load(std::memory_order_relaxed);
    snapshot.deferred_total =
        counters.deferred_total.load(std::memory_order_relaxed);
    snapshot.due_in_gate_total =
        counters.due_in_gate_total.load(std::memory_order_relaxed);
    snapshot.max_backlog =
        counters.max_backlog.load(std::memory_order_relaxed);
    snapshot.backlog = counters.backlog.load(std::memory_order_relaxed);
    return snapshot;
}

}  // namespace repiu::engine
