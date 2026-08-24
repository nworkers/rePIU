#ifndef REPIU_PLATFORM_FAULT_HANDLER_H_
#define REPIU_PLATFORM_FAULT_HANDLER_H_

#include "repiu/platform/guest_cpu_context.h"

#include <cstdint>

// Task 503c. Taking delivery of a fault, and resuming from it.
//
// The engine runs the guest's code natively and learns what it did by faulting:
// an `INT 21` arrives as an access violation and is decoded from the bytes at
// EIP, a page it protected against writes announces self-modifying code, and
// single-stepping under the trap flag is how it walks instructions it cannot
// translate. Windows delivers all of that through one vectored handler.
//
// Linux delivers the same events as signals. The correspondence is closer than
// it looks -- SIGSEGV for both kinds of access violation, SIGTRAP for both the
// single step and the breakpoint -- so what differs is the plumbing, not the
// model. This header is that plumbing, and nothing above it needs to know which
// host it is on.
//
// Three things are settled here rather than at each use:
//
//   * Resuming with modified registers is the normal path, not an error path.
//     It is what Windows calls EXCEPTION_CONTINUE_EXECUTION and what Linux does
//     by editing the machine context and returning from the handler.
//
//   * The handler runs on its own stack on Linux (sigaltstack), because the
//     guest stack may be the very thing that is damaged, or may be mid-switch.
//     Windows has no equivalent problem: its vectored handler already runs on a
//     stack the kernel provided.
//
//   * Async-signal-safety does not apply. Every fault here is synchronous, so
//     ordinary code may run in the callback. That would change if timer
//     injection ever moved onto a signal.
//
// See docs/design/20260822-503-linux-execution-engine.md.

namespace repiu::platform
{

enum class FaultKind : std::uint8_t
{
    // A memory access that could not be performed -- and, on both hosts, how a
    // guest `INT n` arrives, which is by far the most common case.
    kAccessViolation,
    // One instruction executed with the trap flag set.
    kSingleStep,
    // An `INT3`, whether the guest's own or one the engine planted.
    //
    // Eip names the `int3` byte itself, on every host. That is Windows' own
    // convention; Linux reports the following instruction and the Linux backend
    // rewinds to match, because the engine reads the byte at Eip to work out
    // which boundary it hit. A handler that wants to continue past the
    // breakpoint therefore has to advance Eip itself -- resuming unchanged
    // re-executes the `int3`.
    kBreakpoint,
    kIllegalInstruction,
    kIntegerDivideByZero,
    kPrivilegedInstruction,
    // Delivered but not one of the above. Passed on rather than guessed at.
    kOther,
};

struct FaultEvent
{
    FaultKind kind = FaultKind::kOther;
    // Meaningful for kAccessViolation. `valid` is false for every other kind.
    GuestFaultInfo access;
    // The host's own name for what happened: a Windows exception code, or a
    // signal number elsewhere.
    //
    // Deliberately host-specific, and deliberately not for deciding anything.
    // `kind` is what control flow reads. This exists because the engine records
    // the raw value in its crash report, its exception census, its one-slot
    // history, and the shared telemetry a second process reads -- and rounding
    // those to `kind` would throw away detail that has been useful in
    // diagnosing real faults.
    std::uint32_t host_code = 0;
    // Where the faulting instruction is.
    //
    // Windows reports this separately, as the exception record's
    // ExceptionAddress, and the engine reads it at ten places. Linux reports no
    // such thing -- for SIGSEGV `si_addr` is the *data* address that could not
    // be accessed, which is a different question. So Linux fills this from Eip,
    // and the probe asserts on both hosts that the two agree, which is what
    // lets those ten call sites move to Eip rather than needing a Linux
    // counterpart invented for them.
    std::uint32_t instruction_address = 0;
    // The interrupted registers, and writable: changing Eip here is how a
    // handler steps over an instruction it emulated, and changing EFlags is how
    // it arms or disarms the trap flag. On Windows this points straight at the
    // CONTEXT the kernel supplied, so there is no copy; on Linux it points at a
    // structure converted from the machine context and written back on resume.
    GuestCpuContext* registers = nullptr;
};

enum class FaultDisposition : std::uint8_t
{
    // Continue at the address in `registers`, with whatever was written there.
    kResume,
    // Not ours. The host carries on looking for a handler, which for an
    // unhandled fault means the process dies -- which is the correct outcome
    // for a fault this engine did not cause and cannot explain.
    kNotHandled,
};

using FaultCallback = FaultDisposition (*)(FaultEvent* event, void* user_data);

#if defined(_WIN32)
// Transitional, for Task 503d-4.
//
// The engine's dispatcher still receives Windows' EXCEPTION_POINTERS, while the
// handlers below it have moved to FaultEvent. This builds the one from the
// other, in the same place the classification already lives, so there is no
// second copy of it to drift.
//
// It goes away when the dispatcher itself takes a FaultEvent -- at which point
// nothing between the signal and the handlers knows what an EXCEPTION_POINTERS
// is.
[[nodiscard]] FaultEvent MakeFaultEventFromWin32(
    struct _EXCEPTION_POINTERS* exception_info);
#endif

// Installs `callback` for the faults above. One at a time: a second install
// without a remove is refused, because two handlers disagreeing about who owns
// a fault is not a situation worth supporting.
//
// On Linux the alternate signal stack is per-thread, so this must be called on
// the thread that will fault. On Windows a vectored handler is process-wide and
// the callback is responsible for ignoring threads it does not own.
bool InstallFaultHandler(FaultCallback callback, void* user_data);

// Removes the handler and, on Linux, restores the previous signal actions.
bool RemoveFaultHandler();

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_FAULT_HANDLER_H_
