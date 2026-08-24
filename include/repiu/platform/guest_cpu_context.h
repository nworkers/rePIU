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

#if defined(_WIN32)

#include <windows.h>

namespace repiu::platform
{

using GuestCpuContext = CONTEXT;

}  // namespace repiu::platform

#else

namespace repiu::platform
{

// The x87 state, laid out and named like Windows' FLOATING_SAVE_AREA. Only
// StatusWord, TagWord, and RegisterArea are read today -- by the guest x87 push
// helper -- but the rest is kept so the shape is the documented one rather than
// whatever happened to be needed first.
struct GuestFloatingSaveArea
{
    std::uint32_t ControlWord = 0;
    std::uint32_t StatusWord = 0;
    std::uint32_t TagWord = 0;
    std::uint32_t ErrorOffset = 0;
    std::uint32_t ErrorSelector = 0;
    std::uint32_t DataOffset = 0;
    std::uint32_t DataSelector = 0;
    // Eight 80-bit registers, in FSAVE order. Indexed by byte, because the
    // caller walks it as `RegisterArea + top * 10`.
    std::uint8_t RegisterArea[80] = {};
    std::uint32_t Cr0NpxState = 0;
};

// Field names deliberately match Windows' CONTEXT, including its capitalisation,
// because matching them is the entire point.
struct GuestCpuContext
{
    // Windows uses this to say which parts of the structure a
    // GetThreadContext/SetThreadContext call should touch. Nothing reads it
    // here: a signal hands over the whole machine context at once. It exists so
    // the sites that set it need no edit when they move over.
    std::uint32_t ContextFlags = 0;
    std::uint32_t Edi = 0;
    std::uint32_t Esi = 0;
    std::uint32_t Ebx = 0;
    std::uint32_t Edx = 0;
    std::uint32_t Ecx = 0;
    std::uint32_t Eax = 0;
    std::uint32_t Ebp = 0;
    std::uint32_t Eip = 0;
    std::uint32_t Esp = 0;
    std::uint32_t EFlags = 0;
    std::uint32_t SegCs = 0;
    std::uint32_t SegDs = 0;
    std::uint32_t SegEs = 0;
    std::uint32_t SegFs = 0;
    std::uint32_t SegGs = 0;
    std::uint32_t SegSs = 0;
    // Hardware debug registers. Present so code that mentions them still
    // compiles, and always zero: Linux user space cannot write its own thread's
    // debug registers, which is why the linear-span optimisation that uses them
    // stays disabled there. See the design's note on that gap.
    std::uint32_t Dr0 = 0;
    std::uint32_t Dr1 = 0;
    std::uint32_t Dr2 = 0;
    std::uint32_t Dr3 = 0;
    std::uint32_t Dr6 = 0;
    std::uint32_t Dr7 = 0;
    GuestFloatingSaveArea FloatSave;
};

}  // namespace repiu::platform

#endif

namespace repiu::platform
{

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
// The host's own macros rather than their values, so this cannot drift from
// what the API actually expects.
inline constexpr std::uint32_t kGuestCpuContextIntegerControlSegments =
    static_cast<std::uint32_t>(
        CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS);
#else
inline constexpr std::uint32_t kGuestCpuContextIntegerControlSegments = 0U;
#endif

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

#if !defined(_WIN32)

// Copies the interrupted thread's registers out of a POSIX ucontext_t, and
// back. `host_context` is a `ucontext_t*`; it is taken as void* so this header
// stays free of <ucontext.h> for callers that only need the structure.
//
// Returning false means the host context was not the i386 shape this build
// expects, which a caller must treat as unrecoverable rather than resume from.
bool LoadGuestCpuContext(const void* host_context, GuestCpuContext* registers);
bool StoreGuestCpuContext(const GuestCpuContext& registers, void* host_context);

// Extracts the fault address and access direction from a POSIX siginfo_t and
// ucontext_t pair.
[[nodiscard]] GuestFaultInfo ReadGuestFaultInfo(const void* signal_info,
                                                const void* host_context);

#endif

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_GUEST_CPU_CONTEXT_H_
