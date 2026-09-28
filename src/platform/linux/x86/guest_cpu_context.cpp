#include "repiu/platform/guest_cpu_context.h"

// Task 757. The i386 machine-context conversion. CMake builds this file only
// when pointers are four bytes; the check below makes a wrong pick fail here
// rather than link a conversion for the wrong register file.
#if !defined(__i386__)
#error "src/platform/linux/x86/ is the i386 Linux platform layer"
#endif

#include <csignal>
#include <cstddef>
#include <cstring>
#include <ucontext.h>

namespace repiu::platform
{
namespace
{

std::uint32_t Register(const mcontext_t& machine, const int index)
{
    return static_cast<std::uint32_t>(machine.gregs[index]);
}

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

}  // namespace

bool LoadGuestCpuContext(const void* host_context, GuestCpuContext* registers)
{
    if (host_context == nullptr || registers == nullptr)
    {
        return false;
    }
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
}

bool StoreGuestCpuContext(const GuestCpuContext& registers, void* host_context)
{
    if (host_context == nullptr)
    {
        return false;
    }
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
}

std::uintptr_t ReadHostInstructionPointer(const void* host_context)
{
    if (host_context == nullptr)
    {
        return 0U;
    }
    const auto* context = static_cast<const ucontext_t*>(host_context);
    return static_cast<std::uintptr_t>(
        context->uc_mcontext.gregs[REG_EIP]);
}

bool StoreHostInstructionPointer(const std::uintptr_t address,
                                 void* host_context)
{
    if (host_context == nullptr || address == 0U)
    {
        return false;
    }
    auto* context = static_cast<ucontext_t*>(host_context);
    context->uc_mcontext.gregs[REG_EIP] = static_cast<greg_t>(address);
    return true;
}

}  // namespace repiu::platform
