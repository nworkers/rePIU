#ifndef REPIU_PLATFORM_LINUX_GUEST_CPU_CONTEXT_H_
#define REPIU_PLATFORM_LINUX_GUEST_CPU_CONTEXT_H_

// Task 758. The POSIX half of guest_cpu_context.h, used by the Linux and web
// builds. Include guest_cpu_context.h, not this.

#include <cstdint>

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


inline constexpr bool HardwareDebugRegistersAvailable()
{
    // Linux user space cannot write its own thread's debug registers; only a
    // ptrace-attached process can set another's. If that ever becomes worth
    // doing, this is the one place that changes.
    return false;
}

inline constexpr std::uint32_t kGuestCpuContextIntegerControlSegments = 0U;

// Copies the interrupted thread's registers out of a POSIX ucontext_t, and
// back. `host_context` is a `ucontext_t*`; it is taken as void* so this header
// stays free of <ucontext.h> for callers that only need the structure.
//
// Returning false means the host context was not the i386 shape this build
// expects, which a caller must treat as unrecoverable rather than resume from.
bool LoadGuestCpuContext(const void* host_context, GuestCpuContext* registers);
bool StoreGuestCpuContext(const GuestCpuContext& registers, void* host_context);
// Reads the native instruction pointer without reducing it to the guest ABI's
// 32-bit Eip field. Intended for read-only signal-context diagnostics.
[[nodiscard]] std::uintptr_t ReadHostInstructionPointer(const void* host_context);
// Writes the native host instruction pointer without applying the guest
// context's 32-bit register contract.
bool StoreHostInstructionPointer(std::uintptr_t address, void* host_context);

// Extracts the fault address and access direction from a POSIX siginfo_t and
// ucontext_t pair.
[[nodiscard]] GuestFaultInfo ReadGuestFaultInfo(const void* signal_info,
                                                const void* host_context);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_LINUX_GUEST_CPU_CONTEXT_H_
