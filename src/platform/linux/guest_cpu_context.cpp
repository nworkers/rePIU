#include "repiu/platform/guest_cpu_context.h"

#if !defined(_WIN32)

#include <csignal>
#include <cstring>
#include <ucontext.h>

namespace repiu::platform
{
namespace
{

// The guest context remains 32-bit on every host. The x64 adapter below reads
// the low halves of the host registers; it does not make raw 32-bit guest code
// executable in x64 long mode.
#if defined(__i386__) || defined(__x86_64__)
constexpr bool kSupportedMachineContext = true;
#else
constexpr bool kSupportedMachineContext = false;
#endif

std::uint32_t Register(const mcontext_t& machine, const int index)
{
    return static_cast<std::uint32_t>(machine.gregs[index]);
}

#if defined(__i386__)

// glibc's _libc_fpstate is the FSAVE image, which is the same thing Windows
// calls FLOATING_SAVE_AREA -- same word order, same 16-bit-encoded tag word,
// same eight 80-bit registers -- so the copy is field for field rather than a
// reformat. The registers themselves move as bytes because that is how the
// caller indexes them.
void LoadFloatingSave(const _libc_fpstate& source, GuestFloatingSaveArea* target)
{
    target->ControlWord = static_cast<std::uint32_t>(source.cw);
    target->StatusWord = static_cast<std::uint32_t>(source.sw);
    target->TagWord = static_cast<std::uint32_t>(source.tag);
    target->ErrorOffset = static_cast<std::uint32_t>(source.ipoff);
    target->ErrorSelector = static_cast<std::uint32_t>(source.cssel);
    target->DataOffset = static_cast<std::uint32_t>(source.dataoff);
    target->DataSelector = static_cast<std::uint32_t>(source.datasel);
    std::memcpy(target->RegisterArea, source._st, sizeof(target->RegisterArea));
}

void StoreFloatingSave(const GuestFloatingSaveArea& source,
                       _libc_fpstate* target)
{
    target->cw = source.ControlWord;
    target->sw = source.StatusWord;
    target->tag = source.TagWord;
    target->ipoff = source.ErrorOffset;
    target->cssel = source.ErrorSelector;
    target->dataoff = source.DataOffset;
    target->datasel = source.DataSelector;
    std::memcpy(target->_st, source.RegisterArea, sizeof(source.RegisterArea));
}

#elif defined(__x86_64__)

std::uint16_t ClassifyFloatingTag(const _libc_fpxreg& source)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&source);
    std::uint64_t significand = 0;
    std::uint16_t exponent = 0;
    std::memcpy(&significand, bytes, sizeof(significand));
    std::memcpy(&exponent, bytes + sizeof(significand), sizeof(exponent));
    if (exponent == 0U && significand == 0U)
    {
        return 0x01U;
    }
    if (exponent == 0U || exponent == 0x7FFFU ||
        (significand & (UINT64_C(1) << 63U)) == 0U)
    {
        return 0x02U;
    }
    return 0x00U;
}

void LoadFloatingSave(const _libc_fpstate& source,
                      GuestFloatingSaveArea* target)
{
    target->ControlWord = source.cwd;
    target->StatusWord = source.swd;
    target->ErrorOffset = static_cast<std::uint32_t>(source.rip);
    target->DataOffset = static_cast<std::uint32_t>(source.rdp);
    target->TagWord = 0U;
    for (std::size_t index = 0; index < 8U; ++index)
    {
        std::memcpy(target->RegisterArea + index * 10U,
                    &source._st[index], 10U);
        const std::uint16_t tag = ClassifyFloatingTag(source._st[index]);
        target->TagWord |= static_cast<std::uint32_t>(tag) << (index * 2U);
    }
}

void StoreFloatingSave(const GuestFloatingSaveArea& source,
                       _libc_fpstate* target)
{
    target->cwd = static_cast<std::uint16_t>(source.ControlWord);
    target->swd = static_cast<std::uint16_t>(source.StatusWord);
    target->ftw = 0U;
    target->rip = source.ErrorOffset;
    target->rdp = source.DataOffset;
    for (std::size_t index = 0; index < 8U; ++index)
    {
        const std::uint16_t tag = static_cast<std::uint16_t>(
            (source.TagWord >> (index * 2U)) & 0x03U);
        if (tag != 0x03U)
        {
            target->ftw |= static_cast<std::uint16_t>(1U << index);
        }
        std::memset(&target->_st[index], 0, sizeof(target->_st[index]));
        std::memcpy(&target->_st[index],
                    source.RegisterArea + index * 10U,
                    10U);
    }
}

#endif

}  // namespace

bool LoadGuestCpuContext(const void* host_context, GuestCpuContext* registers)
{
    if (host_context == nullptr || registers == nullptr ||
        !kSupportedMachineContext)
    {
        return false;
    }
#if defined(__i386__)
    const auto* context = static_cast<const ucontext_t*>(host_context);
    const mcontext_t& machine = context->uc_mcontext;
    registers->Edi = Register(machine, REG_EDI);
    registers->Esi = Register(machine, REG_ESI);
    registers->Ebx = Register(machine, REG_EBX);
    registers->Edx = Register(machine, REG_EDX);
    registers->Ecx = Register(machine, REG_ECX);
    registers->Eax = Register(machine, REG_EAX);
    registers->Ebp = Register(machine, REG_EBP);
    registers->Eip = Register(machine, REG_EIP);
    // REG_UESP is the stack pointer as it was in user mode. REG_ESP exists too
    // and matches it for the synchronous faults this engine handles, but UESP
    // is the one defined to mean the interrupted thread's stack.
    registers->Esp = Register(machine, REG_UESP);
    registers->EFlags = Register(machine, REG_EFL);
    registers->SegCs = Register(machine, REG_CS);
    registers->SegDs = Register(machine, REG_DS);
    registers->SegEs = Register(machine, REG_ES);
    registers->SegFs = Register(machine, REG_FS);
    registers->SegGs = Register(machine, REG_GS);
    registers->SegSs = Register(machine, REG_SS);
    // The kernel leaves fpregs null when the thread has no FPU state to save,
    // which leaves the x87 fields at their zero-initialised values rather than
    // making the whole load fail.
    if (context->uc_mcontext.fpregs != nullptr)
    {
        LoadFloatingSave(*context->uc_mcontext.fpregs, &registers->FloatSave);
    }
    return true;
#elif defined(__x86_64__)
    const auto* context = static_cast<const ucontext_t*>(host_context);
    const mcontext_t& machine = context->uc_mcontext;
    registers->Edi = Register(machine, REG_RDI);
    registers->Esi = Register(machine, REG_RSI);
    registers->Ebx = Register(machine, REG_RBX);
    registers->Edx = Register(machine, REG_RDX);
    registers->Ecx = Register(machine, REG_RCX);
    registers->Eax = Register(machine, REG_RAX);
    registers->Ebp = Register(machine, REG_RBP);
    registers->Eip = Register(machine, REG_RIP);
    registers->Esp = Register(machine, REG_RSP);
    registers->EFlags = Register(machine, REG_EFL);
    const std::uint64_t selectors = static_cast<std::uint64_t>(
        machine.gregs[REG_CSGSFS]);
    registers->SegCs = static_cast<std::uint32_t>(selectors & 0xFFFFU);
    registers->SegGs = static_cast<std::uint32_t>((selectors >> 16U) & 0xFFFFU);
    registers->SegFs = static_cast<std::uint32_t>((selectors >> 32U) & 0xFFFFU);
    registers->SegDs = 0U;
    registers->SegEs = 0U;
    registers->SegSs = 0U;
    if (context->uc_mcontext.fpregs != nullptr)
    {
        LoadFloatingSave(*context->uc_mcontext.fpregs, &registers->FloatSave);
    }
    return true;
#else
    return false;
#endif
}

bool StoreGuestCpuContext(const GuestCpuContext& registers, void* host_context)
{
    if (host_context == nullptr || !kSupportedMachineContext)
    {
        return false;
    }
#if defined(__i386__)
    auto* context = static_cast<ucontext_t*>(host_context);
    mcontext_t& machine = context->uc_mcontext;
    machine.gregs[REG_EDI] = static_cast<greg_t>(registers.Edi);
    machine.gregs[REG_ESI] = static_cast<greg_t>(registers.Esi);
    machine.gregs[REG_EBX] = static_cast<greg_t>(registers.Ebx);
    machine.gregs[REG_EDX] = static_cast<greg_t>(registers.Edx);
    machine.gregs[REG_ECX] = static_cast<greg_t>(registers.Ecx);
    machine.gregs[REG_EAX] = static_cast<greg_t>(registers.Eax);
    machine.gregs[REG_EBP] = static_cast<greg_t>(registers.Ebp);
    machine.gregs[REG_EIP] = static_cast<greg_t>(registers.Eip);
    // Both stack-pointer slots are written. The kernel restores from UESP, but
    // leaving ESP stale would mislead anything that reads the context again --
    // including this process's own crash reporting.
    machine.gregs[REG_UESP] = static_cast<greg_t>(registers.Esp);
    machine.gregs[REG_ESP] = static_cast<greg_t>(registers.Esp);
    machine.gregs[REG_EFL] = static_cast<greg_t>(registers.EFlags);
    // Segment registers are deliberately not written back. The guest's
    // selectors are restored by the engine's own segment handling, and a signal
    // return that changes CS or SS is a fault, not a resume.
    if (context->uc_mcontext.fpregs != nullptr)
    {
        StoreFloatingSave(registers.FloatSave, context->uc_mcontext.fpregs);
    }
    return true;
#elif defined(__x86_64__)
    auto* context = static_cast<ucontext_t*>(host_context);
    mcontext_t& machine = context->uc_mcontext;
    // Task 549. The low half is written and the host's upper half is kept.
    //
    // Assigning a `std::uint32_t` to a `greg_t` zeroes bits 32..63, and on this
    // host those bits are not spare: the pages this process executes on and
    // faults in sit far above 4 GiB. A signal resume that wrote a truncated RIP
    // back therefore returned to an address that had never been mapped,
    // refaulted at once, and went on doing that -- which is what a Linux x64
    // core-probe run looked like from the outside, and why the run before this
    // one never reached the probes after `fault_handler`.
    //
    // Writing only 32 bits is also the whole of what an edit through this
    // structure may mean. `GuestCpuContext` is a fixed 32-bit contract on every
    // host, so it can say what the low half of a register becomes and nothing
    // about the half above it. Task 546 states the same rule from the other
    // side: host RIP is not guest EIP.
    const auto merge = [](const greg_t host, const std::uint32_t low) {
        return static_cast<greg_t>(
            (static_cast<std::uint64_t>(host) & UINT64_C(0xFFFFFFFF00000000)) |
            static_cast<std::uint64_t>(low));
    };
    machine.gregs[REG_RDI] = merge(machine.gregs[REG_RDI], registers.Edi);
    machine.gregs[REG_RSI] = merge(machine.gregs[REG_RSI], registers.Esi);
    machine.gregs[REG_RBX] = merge(machine.gregs[REG_RBX], registers.Ebx);
    machine.gregs[REG_RDX] = merge(machine.gregs[REG_RDX], registers.Edx);
    machine.gregs[REG_RCX] = merge(machine.gregs[REG_RCX], registers.Ecx);
    machine.gregs[REG_RAX] = merge(machine.gregs[REG_RAX], registers.Eax);
    machine.gregs[REG_RBP] = merge(machine.gregs[REG_RBP], registers.Ebp);
    machine.gregs[REG_RIP] = merge(machine.gregs[REG_RIP], registers.Eip);
    machine.gregs[REG_RSP] = merge(machine.gregs[REG_RSP], registers.Esp);
    machine.gregs[REG_EFL] = merge(machine.gregs[REG_EFL], registers.EFlags);
    if (context->uc_mcontext.fpregs != nullptr)
    {
        StoreFloatingSave(registers.FloatSave, context->uc_mcontext.fpregs);
    }
    return true;
#else
    return false;
#endif
}

GuestFaultInfo ReadGuestFaultInfo(const void* signal_info,
                                  const void* host_context)
{
    GuestFaultInfo info;
    if (signal_info == nullptr || !kSupportedMachineContext)
    {
        return info;
    }
#if defined(__i386__)
    const auto* siginfo = static_cast<const siginfo_t*>(signal_info);
    info.fault_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(siginfo->si_addr));
    if (host_context != nullptr)
    {
        const auto* context = static_cast<const ucontext_t*>(host_context);
        // Bit 1 of the page-fault error code is the write flag, which is where
        // Linux keeps the direction Windows reports as ExceptionInformation[0].
        constexpr std::uint32_t kPageFaultWrite = 0x2U;
        // Bit 4 is set when the access was an instruction fetch, which is what
        // Windows reports as access kind 8.
        constexpr std::uint32_t kPageFaultInstructionFetch = 0x10U;
        const std::uint32_t error = Register(context->uc_mcontext, REG_ERR);
        info.write_access = (error & kPageFaultWrite) != 0U;
        info.execute_access = (error & kPageFaultInstructionFetch) != 0U;
    }
    info.valid = true;
    return info;
#elif defined(__x86_64__)
    const auto* siginfo = static_cast<const siginfo_t*>(signal_info);
    info.fault_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(siginfo->si_addr));
    if (host_context != nullptr)
    {
        const auto* context = static_cast<const ucontext_t*>(host_context);
        constexpr std::uint32_t kPageFaultWrite = 0x2U;
        constexpr std::uint32_t kPageFaultInstructionFetch = 0x10U;
        const std::uint32_t error = Register(context->uc_mcontext, REG_ERR);
        info.write_access = (error & kPageFaultWrite) != 0U;
        info.execute_access = (error & kPageFaultInstructionFetch) != 0U;
    }
    info.valid = true;
    return info;
#else
    return info;
#endif
}

}  // namespace repiu::platform

#endif
