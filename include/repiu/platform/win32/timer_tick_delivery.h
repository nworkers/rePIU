#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace repiu::platform::win32
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
// paths that run a few hundred times per second. The bounded backlog that
// preserves owed ticks is opt-in.
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
// The backlog is therefore ON by default and `REPIU_TIMER_TICK_BACKLOG=0` is
// kept as the regression control -- it was Task 366 leaving this switch in place
// that made the diagnosis possible.
// See docs/design/20260806-432-timer-tick-backlog-default.md.

// Chosen so a backlog cannot park the guest arbitrarily far in the past: at 240Hz
// this is about a quarter second of owed time. Hitting it is itself a finding --
// it means the host cannot catch up, and the cause is execution speed rather than
// delivery.
constexpr std::uint32_t kWin32TimerTickBacklogCapacity = 64U;

class Win32TimerTickDeliveryGuard
{
public:
    explicit Win32TimerTickDeliveryGuard(std::atomic_flag* lock);
    ~Win32TimerTickDeliveryGuard();

    Win32TimerTickDeliveryGuard(const Win32TimerTickDeliveryGuard&) = delete;
    Win32TimerTickDeliveryGuard& operator=(
        const Win32TimerTickDeliveryGuard&) = delete;

    void Release();

private:
    std::atomic_flag* lock_ = nullptr;
};

struct Win32TimerTickDeliveryCounters
{
    // Ticks the schedule said were owed. This is the programmed time base.
    std::atomic<std::uint32_t> due_total{0};
    // `INT 8` frames actually pushed onto the guest.
    std::atomic<std::uint32_t> injected_total{0};
    // Owed ticks discarded because delivery was already pending.
    std::atomic<std::uint32_t> coalesced_total{0};
    // Owed ticks discarded because the backlog was already at capacity.
    std::atomic<std::uint32_t> dropped_total{0};
    // Injection attempts deferred by the existing safe-point conditions (IF=0 or
    // a non-guest instruction pointer). These are delays, not losses.
    std::atomic<std::uint32_t> deferred_total{0};
    // Task 431: of the owed and coalesced ticks above, those that arrived while
    // the guest thread was blocked in the Glide gate. That window runs no guest
    // code, so no safe point is reachable and the tick cannot be delivered at
    // all -- these two say how much of the loss that accounts for.
    std::atomic<std::uint32_t> due_in_gate_total{0};
    std::atomic<std::uint32_t> coalesced_in_gate_total{0};
    std::atomic<std::uint32_t> max_backlog{0};
    // Owed but still undelivered when the run ended; part of the partition
    // identity rather than a loss.
    std::atomic<std::uint32_t> backlog{0};
};

struct Win32TimerTickDeliverySnapshot
{
    bool backlog_enabled = false;
    std::uint32_t due_total = 0;
    std::uint32_t injected_total = 0;
    std::uint32_t coalesced_total = 0;
    std::uint32_t dropped_total = 0;
    std::uint32_t deferred_total = 0;
    std::uint32_t due_in_gate_total = 0;
    std::uint32_t coalesced_in_gate_total = 0;
    std::uint32_t max_backlog = 0;
    std::uint32_t backlog = 0;
};

// On by default (Task 432). `REPIU_TIMER_TICK_BACKLOG=0` restores the single
// boolean that keeps one owed tick and discards the rest. A null pointer means
// the variable is unset, which is the default-on case.
bool ResolveTimerTickBacklogEnabled(const char* setting);
bool TimerTickBacklogEnabled();

// Called from the host poll loop when the schedule reports `due` owed ticks and
// delivery is being armed. `already_pending` says whether an undelivered tick was
// still outstanding, which is what makes the difference between coalescing and a
// clean handoff. Returns the number retained by the selected policy, which lets
// the timestamp queue mirror the accounting decision exactly.
// `in_gate` (Task 431) says the guest thread was blocked in the Glide gate at
// this moment, which is what makes a coalesced tick undeliverable rather than
// merely late.
std::uint32_t RecordTimerTicksDue(Win32TimerTickDeliveryCounters* counters,
                                  std::uint32_t due,
                                  bool already_pending,
                                  bool backlog_enabled,
                                  bool in_gate);

// Called when an `INT 8` frame was actually pushed. Returns true when a further
// tick is still owed and delivery should stay armed, which is how the backlog
// drains one interrupt per safe point instead of bursting.
bool RecordTimerTickInjected(Win32TimerTickDeliveryCounters* counters,
                             bool backlog_enabled);

// Called when an injection attempt hit an existing safe-point condition.
void RecordTimerTickDeferred(Win32TimerTickDeliveryCounters* counters);

// Called when delivery is abandoned without injecting -- an unhooked vector, for
// instance -- so the owed ticks are accounted rather than silently vanishing.
void RecordTimerTickBacklogCleared(
    Win32TimerTickDeliveryCounters* counters);

Win32TimerTickDeliverySnapshot SnapshotTimerTickDelivery(
    const Win32TimerTickDeliveryCounters& counters);

}  // namespace repiu::platform::win32
