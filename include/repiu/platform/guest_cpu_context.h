#ifndef REPIU_PLATFORM_GUEST_CPU_CONTEXT_H_
#define REPIU_PLATFORM_GUEST_CPU_CONTEXT_H_

#include <cstdint>

// Task 503a. The guest's register state, named the same way on every host.
//
// The Win32 execution engine reads and writes this state through Windows'
// CONTEXT at roughly 900 field accesses -- Eip alone appears 328 times, Eax 191,
// EFlags 157. Introducing a new accessor API would mean editing every one of
// them, and that edit would itself be the most likely source of regressions in
// a port whose whole value rests on the engine still behaving identically.
//
// So the field names stay and only the type changes: an alias on Windows, a
// structure with the same member names elsewhere. Existing code compiles
// unchanged on both, and the platform difference collapses into the two
// conversion functions below.
//
// See docs/design/20260822-503-linux-execution-engine.md.

namespace repiu::platform
{

// What a fault reports beyond the registers. Windows carries these in the
// exception record's parameters; Linux splits them between siginfo and the
// machine context, so they are collected here rather than at each use.
struct GuestFaultInfo
{
    bool valid = false;
    bool write_access = false;
    // An instruction fetch from a page that does not permit execution, which is
    // a third case and not merely "not a write".
    //
    // Added in Task 503d-5, from a call site the original pair could not have
    // expressed: the HLE boundary asks specifically whether an *execute* fault
    // landed in the AOT cache's address range. Windows reports it as access
    // kind 8; Linux sets the instruction-fetch bit in the page-fault error
    // code.
    bool execute_access = false;
    std::uint32_t fault_address = 0;
};

}  // namespace repiu::platform

// Task 758. Selection point: the definitions are per platform.
//
// Each platform header defines, in repiu::platform:
//
//   GuestCpuContext  -- an alias of CONTEXT on Windows, a structure with the
//                       same member names elsewhere (see above).
//   HardwareDebugRegistersAvailable() -- described below.
//   kGuestCpuContextIntegerControlSegments -- described below.
//
// and, on POSIX hosts, the ucontext_t conversions (Load/StoreGuestCpuContext,
// Read/StoreHostInstructionPointer, ReadGuestFaultInfo).
//
// HardwareDebugRegistersAvailable:
// Task 503d-23. Whether a write to the `Dr` fields reaches the hardware.
//
// This is not a question about performance, and asking it is not optional. The
// engine has three paths that release the guest to run **natively** -- they
// clear the trap flag -- and arrange for it to come back by arming a hardware
// breakpoint on the return address. Those are two halves of one decision.
//
// Where a `Dr` write is discarded, only the first half happens. The guest is
// released with single-step off and **nothing armed to bring it back**, so it
// runs until it happens to fault; a region whose body faults on nothing runs
// forever. Measured on Linux before this predicate existed: entry/return/cancel
// of `18/0/17` -- eighteen releases, not one return, and the one that was never
// cancelled either is the stall.
//
// The `returned` test on that path reads `Dr6`, so where these are inert the
// engine cannot even recognise a return that did happen. It is not a degraded
// mode; it is a mode with no exit.
//
// So every such path asks this first. The design settled it before any of this
// was written -- Linux keeps the debug-register features disabled -- and this is
// where that decision now lives, next to the fields it is about.
//
// kGuestCpuContextIntegerControlSegments:
// Task 503d-11. What the engine puts in `ContextFlags` when it fills a context
// by hand.
//
// On Windows those are the bits telling GetThreadContext and SetThreadContext
// which parts of the structure to touch, and the engine asks for the integer,
// control, and segment registers -- everything it fills in. Elsewhere the field
// is inert, as 503a recorded when it kept the field but not its meaning, so the
// value is zero.
//
// Named rather than written out at each of the five sites, because the sites
// otherwise carry a conditional apiece for a value none of them reads back.
#if defined(_WIN32)
#include "repiu/platform/win32/guest_cpu_context_win32.h"
#else
#include "repiu/platform/linux/guest_cpu_context.h"
#endif

#endif  // REPIU_PLATFORM_GUEST_CPU_CONTEXT_H_
