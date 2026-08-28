#include "repiu/engine/timer_tick_delivery.h"

#include <algorithm>
#include <cstdlib>
#include <thread>

namespace repiu::engine
{
namespace
{

bool ReadTimerTickBacklogSetting()
{
    return ResolveTimerTickBacklogEnabled(
        std::getenv("REPIU_TIMER_TICK_BACKLOG"));
}

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

Win32TimerTickDeliveryGuard::Win32TimerTickDeliveryGuard(
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

Win32TimerTickDeliveryGuard::~Win32TimerTickDeliveryGuard()
{
    Release();
}

void Win32TimerTickDeliveryGuard::Release()
{
    if (lock_ != nullptr)
    {
        lock_->clear(std::memory_order_release);
        lock_ = nullptr;
    }
}

bool ResolveTimerTickBacklogEnabled(const char* setting)
{
    // Task 432: on by default. Only an explicit off disables it, so an unset or
    // unrecognised value keeps the accurate behaviour rather than silently
    // reverting to the boolean that loses ticks.
    if (setting == nullptr)
    {
        return true;
    }
    const std::string_view value(setting);
    return !(value == "0" || value == "off" || value == "false");
}

bool TimerTickBacklogEnabled()
{
    static const bool enabled = ReadTimerTickBacklogSetting();
    return enabled;
}

std::uint32_t RecordTimerTicksDue(
    Win32TimerTickDeliveryCounters* counters,
    std::uint32_t due,
    bool already_pending,
    bool backlog_enabled,
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

    if (!backlog_enabled)
    {
        // Stage one accounting for the shipping behaviour: arming delivery
        // publishes a single boolean, so one owed tick becomes the pending
        // injection and the rest are gone. An already-pending flag means even
        // that one is a duplicate of a tick not yet taken.
        const std::uint32_t retained = already_pending ? 0U : 1U;
        counters->coalesced_total.fetch_add(due - retained,
                                            std::memory_order_relaxed);
        if (in_gate)
        {
            counters->coalesced_in_gate_total.fetch_add(
                due - retained, std::memory_order_relaxed);
        }
        counters->backlog.store(1U, std::memory_order_relaxed);
        RaiseMaximum(&counters->max_backlog, 1U);
        return retained;
    }

    // Stage two: keep the owed ticks, bounded. Beyond the cap the guest would be
    // parked ever further in the past, so the excess is counted and dropped
    // rather than delivered late enough to be meaningless.
    std::uint32_t backlog = counters->backlog.load(std::memory_order_relaxed);
    const std::uint32_t room = kWin32TimerTickBacklogCapacity > backlog
        ? kWin32TimerTickBacklogCapacity - backlog
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

bool RecordTimerTickInjected(Win32TimerTickDeliveryCounters* counters,
                             bool backlog_enabled)
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
    // Only the backlog mode keeps delivery armed. Without it the caller's
    // existing "clear the flag after one injection" behaviour is unchanged.
    return backlog_enabled && backlog != 0U;
}

void RecordTimerTickDeferred(Win32TimerTickDeliveryCounters* counters)
{
    if (counters == nullptr)
    {
        return;
    }
    counters->deferred_total.fetch_add(1U, std::memory_order_relaxed);
}

void RecordTimerTickBacklogCleared(
    Win32TimerTickDeliveryCounters* counters)
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

Win32TimerTickDeliverySnapshot SnapshotTimerTickDelivery(
    const Win32TimerTickDeliveryCounters& counters)
{
    Win32TimerTickDeliverySnapshot snapshot;
    snapshot.backlog_enabled = TimerTickBacklogEnabled();
    snapshot.due_total = counters.due_total.load(std::memory_order_relaxed);
    snapshot.injected_total =
        counters.injected_total.load(std::memory_order_relaxed);
    snapshot.coalesced_total =
        counters.coalesced_total.load(std::memory_order_relaxed);
    snapshot.dropped_total =
        counters.dropped_total.load(std::memory_order_relaxed);
    snapshot.deferred_total =
        counters.deferred_total.load(std::memory_order_relaxed);
    snapshot.due_in_gate_total =
        counters.due_in_gate_total.load(std::memory_order_relaxed);
    snapshot.coalesced_in_gate_total =
        counters.coalesced_in_gate_total.load(std::memory_order_relaxed);
    snapshot.max_backlog =
        counters.max_backlog.load(std::memory_order_relaxed);
    snapshot.backlog = counters.backlog.load(std::memory_order_relaxed);
    return snapshot;
}

}  // namespace repiu::engine
