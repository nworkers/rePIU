#include "repiu/platform/guest_cpu_context.h"

// Task 757. The x86-64 machine-context conversion. CMake builds this file only
// when pointers are eight bytes; the check below makes a wrong pick fail here
// rather than link a conversion for the wrong register file.
#if !defined(__x86_64__)
#error "src/platform/linux/x64/ is the x86-64 Linux platform layer"
#endif

#include <csignal>
#include <cstddef>
#include <cstring>
#include <ucontext.h>

namespace repiu::platform
{
namespace
{

// The guest context remains 32-bit on every host. This adapter reads the low
// halves of the host registers; it does not make raw 32-bit guest code
// executable in x64 long mode.
std::uint32_t Register(const mcontext_t& machine, const int index)
{
    return static_cast<std::uint32_t>(machine.gregs[index]);
}

// The x87 stack top, from status-word bits 11..13. Both save formats store the
// register *contents* in stack-relative ST(0)..ST(7) order while indexing the
// tag by *physical* register R0..R7, so the contents of physical register `j`
// are at `_st[(j - top) & 7]`.
std::size_t FloatingStackTop(const _libc_fpstate& source)
{
    return static_cast<std::size_t>((source.swd >> 11U) & 0x07U);
}

// The FSAVE tag for one register's contents. The caller has already decided the
// register is in use; this only says which kind of value it holds.
//
// The sign bit is masked out of the exponent first. It is bit 15 of the same
// word, and leaving it in would misread every negative value: -0.0 would miss
// the zero case and a negative NaN or infinity would miss the special case.
// The three rules below are the ones the Linux kernel's `twd_fxsr_to_i387`
// applies to the same bytes.
std::uint16_t ClassifyFloatingTag(const _libc_fpxreg& source)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&source);
    std::uint64_t significand = 0;
    std::uint16_t exponent = 0;
    std::memcpy(&significand, bytes, sizeof(significand));
    std::memcpy(&exponent, bytes + sizeof(significand), sizeof(exponent));
    exponent &= 0x7FFFU;
    if (exponent == 0x7FFFU)
    {
        return 0x02U;  // Special: infinity or NaN.
    }
    if (exponent == 0U)
    {
        return significand == 0U ? 0x01U   // Zero.
                                 : 0x02U;  // Special: denormal.
    }
    return (significand & (UINT64_C(1) << 63U)) != 0U
        ? 0x00U   // Valid.
        : 0x02U;  // Special: unnormal.
}

void LoadFloatingSave(const _libc_fpstate& source,
                      GuestFloatingSaveArea* target)
{
    target->ControlWord = source.cwd;
    target->StatusWord = source.swd;
    target->ErrorOffset = static_cast<std::uint32_t>(source.rip);
    target->DataOffset = static_cast<std::uint32_t>(source.rdp);
    // Task 707. The abridged tag is read before anything is classified.
    //
    // FXSAVE keeps one bit per physical register -- set for in use, clear for
    // empty -- where FSAVE keeps two bits with four states. Expanding the
    // four-state word from the register contents alone cannot recover `empty`,
    // because an empty register and a register holding zero hold the same
    // bytes. Reading `ftw` first is what keeps that distinction, and losing it
    // is what marked all eight registers in use, overflowed the guest's x87
    // stack on its next FLD, and turned every later result into a NaN.
    const std::size_t top = FloatingStackTop(source);
    target->TagWord = 0U;
    for (std::size_t physical = 0; physical < 8U; ++physical)
    {
        const bool in_use =
            ((source.ftw >> physical) & 0x01U) != 0U;
        const std::size_t stack_index = (physical - top) & 0x07U;
        const std::uint16_t tag = in_use
            ? ClassifyFloatingTag(source._st[stack_index])
            : 0x03U;
        target->TagWord |= static_cast<std::uint32_t>(tag) << (physical * 2U);
    }
    for (std::size_t index = 0; index < 8U; ++index)
    {
        std::memcpy(target->RegisterArea + index * 10U,
                    &source._st[index], 10U);
    }
}

void StoreFloatingSave(const GuestFloatingSaveArea& source,
                       _libc_fpstate* target)
{
    target->cwd = static_cast<std::uint16_t>(source.ControlWord);
    target->swd = static_cast<std::uint16_t>(source.StatusWord);
    target->rip = source.ErrorOffset;
    target->rdp = source.DataOffset;
    // The reverse direction needs no rotation: both tag words index the same
    // physical registers, and only the width changes. This is the kernel's
    // `twd_i387_to_fxsr` rule -- every state but `empty` becomes in use.
    target->ftw = 0U;
    for (std::size_t physical = 0; physical < 8U; ++physical)
    {
        const std::uint16_t tag = static_cast<std::uint16_t>(
            (source.TagWord >> (physical * 2U)) & 0x03U);
        if (tag != 0x03U)
        {
            target->ftw |= static_cast<std::uint16_t>(1U << physical);
        }
    }
    for (std::size_t index = 0; index < 8U; ++index)
    {
        std::memset(&target->_st[index], 0, sizeof(target->_st[index]));
        std::memcpy(&target->_st[index],
                    source.RegisterArea + index * 10U,
                    10U);
    }
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
    registers->Edi = Register(machine, REG_RDI);
    registers->Esi = Register(machine, REG_RSI);
    registers->Ebx = Register(machine, REG_RBX);
    registers->Edx = Register(machine, REG_RDX);
    registers->Ecx = Register(machine, REG_RCX);
    registers->Eax = Register(machine, REG_RAX);
    registers->Ebp = Register(machine, REG_RBP);
    registers->Eip = Register(machine, REG_RIP);
    // Task 577. Guest ESP is R15D, not RSP.
    //
    // On i386 these are one register and reading RSP is right. On x64 they are
    // two: Task 546's decision 3 keeps host RSP as the SysV stack and Task 558
    // puts guest ESP in R15D. The engine spends `Esp` as a guest address --
    // reading `[Esp+8]` off the guest stack, storing it as `guest_return_esp`,
    // testing it against the guest arena -- and host RSP is none of those
    // things.
    //
    // `Eip` above is deliberately still RIP. The engine treats a faulting `Eip`
    // as a *cache* address and translates it through the address map, which is
    // what it does on i386 too; the cache is placed below 4 GiB (Task 554), so
    // the truncation is lossless and the value is the one the engine expects.
    registers->Esp = Register(machine, REG_R15);
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
}

bool StoreGuestCpuContext(const GuestCpuContext& registers, void* host_context)
{
    if (host_context == nullptr)
    {
        return false;
    }
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
    // Task 577. Guest ESP goes back to R15, and host RSP is not written at all.
    //
    // Not writing RSP is the safety condition. The engine *modifies* `Esp` --
    // a `ZYDIS_REGISTER_ESP` write, `RecoverToHost`'s `context->Esp` -- and the
    // kernel resumes on this context. Sending a guest value into RSP would move
    // the host's stack pointer to a guest address.
    //
    // Zero-extended rather than merged, unlike every register above. Task 558's
    // invariant is that R15's upper half is zero, because an access through
    // guest ESP is emitted as `[r15]` where the whole 64-bit register is the
    // address; the emitter keeps it so by writing `lea r15d, ...` and letting
    // the hardware zero-extend. `merge` would leave the upper half as it found
    // it, which assumes the invariant instead of maintaining it.
    machine.gregs[REG_R15] = static_cast<greg_t>(
        static_cast<std::uint64_t>(registers.Esp));
    machine.gregs[REG_EFL] = merge(machine.gregs[REG_EFL], registers.EFlags);
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
        context->uc_mcontext.gregs[REG_RIP]);
}

bool StoreHostInstructionPointer(const std::uintptr_t address,
                                 void* host_context)
{
    if (host_context == nullptr || address == 0U)
    {
        return false;
    }
    auto* context = static_cast<ucontext_t*>(host_context);
    context->uc_mcontext.gregs[REG_RIP] = static_cast<greg_t>(address);
    return true;
}

}  // namespace repiu::platform
