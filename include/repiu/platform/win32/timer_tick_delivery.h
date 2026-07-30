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
// MEASURED (Task 366, three 60-second Release runs each): default delivery is
// 88.1% of owed ticks, and enabling the backlog raised delivery to 91.8% while
// *lowering* frames 16.4% (1,400 to 1,171, non-overlapping ranges). Frame rate is
// therefore not gated by tick delivery. The regression is not the extra
// interrupts themselves: keeping a tick owed keeps `timer_interrupt_pending` set,
// which keeps the AOT timer safe point armed continuously, and safe-point traps
// rose 20% with exceptions per frame up 6.8%. The backlog is kept opt-in as the
// reproducible control arm for any future drain that does not hold the safe point
// armed -- do not enable it expecting a speedup.

// Chosen so a backlog cannot park the guest arbitrarily far in the past: at 240Hz
// this is about a quarter second of owed time. Hitting it is itself a finding --
// it means the host cannot catch up, and the cause is execution speed rather than
// delivery.
constexpr std::uint32_t kWin32TimerTickBacklogCapacity = 64U;

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
    std::uint32_t max_backlog = 0;
    std::uint32_t backlog = 0;
};

// Off by default: stage one only measures. `REPIU_TIMER_TICK_BACKLOG=1` turns on
// the bounded backlog that preserves owed ticks across safe points.
bool ResolveTimerTickBacklogEnabled(std::string_view setting);
bool TimerTickBacklogEnabled();

// Called from the host poll loop when the schedule reports `due` owed ticks and
// delivery is being armed. `already_pending` says whether an undelivered tick was
// still outstanding, which is what makes the difference between coalescing and a
// clean handoff. Returns nothing: the caller still arms delivery exactly as
// before.
void RecordTimerTicksDue(Win32TimerTickDeliveryCounters* counters,
                         std::uint32_t due,
                         bool already_pending,
                         bool backlog_enabled);

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
