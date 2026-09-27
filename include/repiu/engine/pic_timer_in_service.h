#pragma once

#include <cstdint>

namespace repiu::engine
{

// Task 735. When an owed timer tick may be injected: the 8259 PIC's in-service
// bit for IRQ0, and a bound on catching up at `sti`.
//
// On the real machine a delivered IRQ0 sets its in-service bit, and the PIC
// raises no further IRQ0 until the handler writes an EOI -- whatever the
// handler does with IF in between. Injection here used to consult only IF, read
// from the host context. But the guest runs in user mode, where IF cannot be
// cleared: a guest `cli` is emulated by editing the saved context and the
// kernel sets IF again on the way back. So a tick owed while the ISR ran was
// injected at the next safe point inside the ISR. pumpitea programs the PIT to
// 51.9 kHz for a while, a tick is always owed, and the nested frames overflowed
// the guest stack.
//
// The bit is set by an injection and cleared by an EOI written to port 0x20.
//
// Once the EOI is written a pending IRQ0 is deliverable again, and the real CPU
// takes it at the handler's closing `sti`. The engine does the same, which is
// how ticks owed during a long host call are caught up -- pumpit2a lost a
// quarter of its ticks without it. But an emulated handler is far slower than a
// real one, and at 51.9 kHz a tick is always owed, so delivering at every
// closing `sti` would chain handlers without end. So two `sti` deliveries may
// not follow each other: after one, the next tick waits for an ordinary safe
// point, which the interrupted code only reaches by running.
//
// Whether a handler has returned is not tracked from the stack: after `iret`
// the interrupted code is free to run deeper than the frame was. The stack is
// only the fallback that ends service for a handler that never writes an EOI
// (one that relies on a chained handler, say) -- the rule the JAMMA input
// timeline already uses to retire replay frames.
//
// Guest-thread only: injection and port I/O both run there.
//
// On by default. `REPIU_PIC_TIMER_IN_SERVICE=0` restores the earlier behaviour
// -- no in-service wait and no delivery at `sti` -- for A/B comparison.
[[nodiscard]] bool ResolvePicTimerInServiceEnabled(const char* value);
[[nodiscard]] bool PicTimerInServiceEnabled();

struct PicTimerInService
{
    bool active = false;
    // ESP of the in-service frame, for the no-EOI fallback.
    std::uint32_t frame_esp = 0;
    // The last injection was made at a guest `sti`.
    bool last_delivery_at_sti = false;
    // Task 751. The handler of the last injection has returned: its `iret` was
    // seen, or the guest stack was seen above the injected frame. The `sti`
    // chain rule is about a handler's own closing `sti`; once the handler has
    // returned, the next `sti` belongs to the interrupted code, and a
    // delivery there cannot chain. Without this, code that runs one
    // `cli`...`sti` section after another -- with the hold above leaving no
    // other moment to deliver -- got one tick and then none until the
    // backlog overflowed.
    bool last_delivery_returned = true;
    // Set by an emulated `sti`, consumed by the next injection attempt. The
    // attempt is not made inside the `sti` handler: the dispatcher first
    // reconciles AOT and trace state to a guest instruction boundary, and
    // redirecting before that left Win32 single-stepping into the handler.
    bool sti_request = false;
    // Injection attempts held back while IRQ0 was in service.
    std::uint32_t blocked_in_service_total = 0;
    // `sti` deliveries held back because the previous one was also at `sti`.
    std::uint32_t blocked_sti_chain_total = 0;
    std::uint32_t delivered_at_sti_total = 0;
    std::uint32_t cleared_by_eoi_total = 0;
    // Services ended by the handler returning without an EOI.
    std::uint32_t retired_by_stack_total = 0;

    // Task 751. The guest's own `cli`. The host context cannot carry it (user
    // mode keeps IF set), so it is kept here: set by an emulated `cli`, ended
    // by an emulated `sti` or by the `iret` of an injected frame. While it
    // holds no tick is injected, which is what a `cli`/`sti` pair around a
    // bit-banged transaction asks for -- pumpitpc's security check clocks its
    // CAT702 that way, and a timer handler running in the middle moved the
    // board's destination register under it.
    // Task 751. The injected frames whose handlers have not returned, newest
    // last, and whether this host lets the engine see a handler's `iret`.
    //
    // After its EOI and its `sti` a handler is interruptible again, and what
    // it runs from there -- a chained handler, a loop -- passes safe points.
    // At 51.9 kHz a tick is owed at every one of them, so handlers nested
    // inside handlers until the guest stack overflowed (the "tick storm":
    // 18,000 a second, no frame, a fault within seconds of starting). With
    // the frames known, a tick goes in only into interrupted code (no frame
    // standing) or, one level deep, at a handler's closing `sti`.
    //
    // The rule needs the `iret` to be seen, which is the case where `iret` is
    // emulated (Linux x64). Where it runs natively the stack test is all
    // there is, and the interrupted code is free to run deeper than the frame
    // was, so the rule stays off rather than stop the clock.
    static constexpr std::uint32_t kHandlerFrameCapacity = 4;
    std::uint32_t handler_frames[kHandlerFrameCapacity] = {};
    std::uint32_t handler_depth = 0;
    bool handler_return_seen = false;
    std::uint32_t blocked_nested_total = 0;

    // Task 751. The interrupted code's turn. When the outermost handler was
    // entered and when it returned, by the clock the hold uses; the interrupted
    // code runs for as long as the handler took before the next tick goes in.
    // An emulated handler (about 55 us) is slower than the 51.9 kHz stage's
    // period (19 us), so without a turn of its own the interrupted code never
    // reached the instruction that ends the stage.
    std::uint64_t outer_injection_ns = 0;
    std::uint64_t last_return_ns = 0;
    std::uint64_t last_handler_ns = 0;
    std::uint32_t blocked_by_turn_total = 0;
    std::uint64_t handler_max_ns = 0;

    // Task 751. Deliveries made right after a handler returned, since the
    // interrupted code last took one by running. See `kPostReturnChainLimit`.
    std::uint32_t post_return_chain = 0;
    std::uint32_t post_return_delivered_total = 0;
    std::uint32_t blocked_post_return_total = 0;

    bool cli_hold = false;
    std::uint64_t cli_hold_since_ns = 0;
    std::uint32_t cli_total = 0;
    std::uint32_t sti_total = 0;
    std::uint32_t blocked_by_cli_total = 0;
    // Holds ended by the limit below rather than by the guest.
    std::uint32_t cli_hold_expired_total = 0;
    std::uint64_t cli_hold_max_ns = 0;
};

// Task 751. How many ticks may go in back to back, each at the return of the
// handler before it, before the interrupted code has to run again.
//
// A tick owed when a handler returns is deliverable on the real machine, and
// taking it there is how the ticks a `cli` section held back are caught up.
// But an emulated handler is slower than the 51.9 kHz stage's period, so a
// tick is always owed there, and an unbounded chain never lets the
// interrupted code reach the instruction that ends the stage: 18,000
// handlers a second and no frame. Three is enough for a 10 ms section at
// 240 Hz and leaves the interrupted code a turn after every fourth handler.
inline constexpr std::uint32_t kPostReturnChainLimit = 3U;

// Where an injection attempt is made.
enum class TimerInjectionSite : std::uint8_t
{
    // A safe point, an emulated instruction, a gate: the interrupted code
    // got here by running.
    kOrdinary,
    // Right after a handler's `iret`.
    kAfterHandlerReturn,
};

// Below this tick period a delivery right after a handler's return is not
// made at all: the chain is for catching up at the game's own rate, and at a
// faster one a tick is always owed.
inline constexpr std::uint64_t kPostReturnChainMinimumPeriodNanoseconds =
    1'000'000ULL;

// True when an attempt at `site` must wait for the interrupted code to run:
// its turn after the last handler is not over, or the chain after a return
// has reached its limit or is not made at this tick period. Call only for an
// attempt that would otherwise inject, and follow a false answer with the
// injection: it counts the delivery.
bool PostReturnChainBlocksInjection(PicTimerInService* state,
                                    TimerInjectionSite site,
                                    std::uint64_t now_ns,
                                    std::uint64_t tick_period_ns);

// The moment a tick was injected, after `NotePicTimerInjected`.
void NoteTimerInjectionTime(PicTimerInService* state, std::uint64_t now_ns);

// Task 751. A hold older than this is taken to have ended unseen: `popfd` and,
// on i386, a native `iret` restore IF without the engine seeing them, and a
// hold that never ended would stop the guest's clock.
inline constexpr std::uint64_t kGuestCliHoldLimitNanoseconds = 100'000'000ULL;

// On by default. `REPIU_GUEST_CLI_HOLD=0` ignores the guest's `cli` as before.
[[nodiscard]] bool ResolveGuestCliHoldEnabled(const char* value);
[[nodiscard]] bool GuestCliHoldEnabled();

void NoteGuestCli(PicTimerInService* state, std::uint64_t now_ns);
void NoteGuestSti(PicTimerInService* state, std::uint64_t now_ns);
// An `iret` that popped the frame at `frame_esp`. Only the return of the
// injected frame counts: a handler that chains to the previous INT 8 handler
// sees that one's `iret` inside its own body, and the flags such a frame
// carries came from a guest `pushfd`, which shows IF set whatever the guest
// asked for. Taking that `iret` for the handler's own let its closing `sti`
// chain handlers until the stack overflowed.
void NoteGuestIret(PicTimerInService* state, std::uint32_t frame_esp,
                   std::uint64_t now_ns);
// True when the guest's `cli` holds an injection back.
bool GuestCliBlocksInjection(PicTimerInService* state, std::uint64_t now_ns);

void NotePicTimerInjected(PicTimerInService* state, std::uint32_t frame_esp,
                          bool at_sti);

// A write of `command` to the master PIC's command port (0x20). Returns true
// when it is an EOI that ends IRQ0's service: the non-specific EOI 0x20 or the
// specific EOI for IRQ0, 0x60.
bool NotePicCommand(PicTimerInService* state, std::uint8_t command);

// True when an IRQ0 injection must wait. `at_sti` says the attempt is made at a
// guest `sti` rather than at a safe point or boundary.
bool PicTimerBlocksInjection(PicTimerInService* state,
                             std::uint32_t current_esp, bool at_sti);

}  // namespace repiu::engine
