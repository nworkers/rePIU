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
};

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
