#pragma once

#include <atomic>
#include <cstdint>

namespace repiu::engine
{

// Task 366: accounts for the gap between the timer ticks the guest programmed and
// the ones it actually received.
//
// `PitIrqSchedule::Poll` is a catch-up scheduler that returns the exact number of
// ticks owed, but delivery has always been a single `std::atomic<bool>`, so a
// poll owing three ticks produces one `INT 8` and drops two. That is not wrong
// for an 8259, which has one pending bit per IRQ line -- but on original hardware
// the game was fast enough that coalescing was rare, and here it is constant.
//
// The counters are always on and change no behaviour; they cost one increment on
// paths that run a few hundred times per second.
//
// MEASURED (Task 366, three 60-second Release runs each): default delivery was
// 88.1% of owed ticks, and enabling the backlog raised delivery to 91.8% while
// *lowering* frames 16.4%. The regression was not the extra interrupts: keeping a
// tick owed keeps `timer_interrupt_pending` set, which kept the AOT timer safe
// point armed continuously, and safe-point traps rose 20%.
//
// SUPERSEDED (Task 432). That cost mechanism no longer occurs, because the
// backlog now empties between Glide gate calls instead of pinning at the cap:
// `max_backlog` reads 12 against 64, and safe-point traps run at *exactly one per
// owed tick* rather than continuously. Tasks 414, 415, 417 and 419 raised
// execution speed in between, so Task 366's T3 reading -- that the host cannot
// keep up with 240Hz -- does not hold here. Measured on the current build, the
// backlog takes delivery from 50.6% to 99.98% over a 64-second gameplay window
// and holds the guest clock to real time (`tick_lag_ms` growth +11,365ms to
// -11ms), and the user confirmed it removes the note and BGA jumping.
// The backlog therefore became the default; issue #24 removed the single-boolean
// policy and its `REPIU_TIMER_TICK_BACKLOG=0` switch, so owed ticks are always
// kept, bounded by the capacity below.
// See docs/design/20260806-432-timer-tick-backlog-default.md.

// Chosen so a backlog cannot park the guest arbitrarily far in the past: at 240Hz
// this is about a quarter second of owed time. Hitting it is itself a finding --
// it means the host cannot catch up, and the cause is execution speed rather than
// delivery.
constexpr std::uint32_t kTimerTickBacklogCapacity = 64U;

class TimerTickDeliveryGuard
{
public:
    explicit TimerTickDeliveryGuard(std::atomic_flag* lock);
    ~TimerTickDeliveryGuard();

    TimerTickDeliveryGuard(const TimerTickDeliveryGuard&) = delete;
    TimerTickDeliveryGuard& operator=(
        const TimerTickDeliveryGuard&) = delete;

    void Release();

private:
    std::atomic_flag* lock_ = nullptr;
};

struct TimerTickDeliveryCounters
{
    // Ticks the schedule said were owed. This is the programmed time base.
    std::atomic<std::uint32_t> due_total{0};
    // `INT 8` frames actually pushed onto the guest.
    std::atomic<std::uint32_t> injected_total{0};
    // Owed ticks discarded because the backlog was already at capacity.
    std::atomic<std::uint32_t> dropped_total{0};
    // Injection attempts deferred by the existing safe-point conditions (IF=0 or
    // a non-guest instruction pointer). These are delays, not losses.
    std::atomic<std::uint32_t> deferred_total{0};
    // Task 431: of the owed ticks above, those that arrived while the guest
    // thread was blocked in the Glide gate. That window runs no guest code, so
    // no safe point is reachable until the gate returns.
    std::atomic<std::uint32_t> due_in_gate_total{0};
    std::atomic<std::uint32_t> max_backlog{0};
    // Owed but still undelivered when the run ended; part of the partition
    // identity rather than a loss.
    std::atomic<std::uint32_t> backlog{0};
};

struct TimerTickDeliverySnapshot
{
    std::uint32_t due_total = 0;
    std::uint32_t injected_total = 0;
    std::uint32_t dropped_total = 0;
    std::uint32_t deferred_total = 0;
    std::uint32_t due_in_gate_total = 0;
    std::uint32_t max_backlog = 0;
    std::uint32_t backlog = 0;
};

// Called from the host poll loop when the schedule reports `due` owed ticks and
// delivery is being armed. Returns the number the backlog accepted, which lets
// the timestamp queue mirror the accounting decision exactly.
// `in_gate` (Task 431) says the guest thread was blocked in the Glide gate at
// this moment, where no safe point is reachable until the gate returns.
std::uint32_t RecordTimerTicksDue(TimerTickDeliveryCounters* counters,
                                  std::uint32_t due,
                                  bool in_gate);

// Called when an `INT 8` frame was actually pushed. Returns true when a further
// tick is still owed and delivery should stay armed, which is how the backlog
// drains one interrupt per safe point instead of bursting.
bool RecordTimerTickInjected(TimerTickDeliveryCounters* counters);

// Called when an injection attempt hit an existing safe-point condition.
void RecordTimerTickDeferred(TimerTickDeliveryCounters* counters);

// Called when delivery is abandoned without injecting -- an unhooked vector, for
// instance -- so the owed ticks are accounted rather than silently vanishing.
void RecordTimerTickBacklogCleared(
    TimerTickDeliveryCounters* counters);

TimerTickDeliverySnapshot SnapshotTimerTickDelivery(
    const TimerTickDeliveryCounters& counters);

}  // namespace repiu::engine
