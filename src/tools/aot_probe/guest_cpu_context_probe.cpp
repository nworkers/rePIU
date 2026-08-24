#include "guest_cpu_context_probe.h"

#include "repiu/platform/guest_cpu_context.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

#if !defined(_WIN32)
#include <csignal>
#include <ucontext.h>
#endif

namespace repiu::tools
{
namespace
{

using repiu::platform::GuestCpuContext;

// Eight 80-bit x87 registers. Named here rather than taken from sizeof, so a
// structure that silently shrank would fail the probe instead of passing it.
constexpr std::size_t kRegisterAreaSize = 80;

// Distinct values so a field copied from or to the wrong slot shows up as a
// mismatch rather than as a plausible-looking zero.
constexpr std::uint32_t kEdi = 0x11110001U;
constexpr std::uint32_t kEsi = 0x22220002U;
constexpr std::uint32_t kEbx = 0x33330003U;
constexpr std::uint32_t kEdx = 0x44440004U;
constexpr std::uint32_t kEcx = 0x55550005U;
constexpr std::uint32_t kEax = 0x66660006U;
constexpr std::uint32_t kEbp = 0x77770007U;
constexpr std::uint32_t kEip = 0x88880008U;
constexpr std::uint32_t kEsp = 0x99990009U;
constexpr std::uint32_t kFlags = 0x00000246U;
constexpr std::uint32_t kContextFlags = 0x0001000FU;
constexpr std::uint32_t kControlWord = 0x0000037FU;
constexpr std::uint32_t kStatusWord = 0x00003800U;
constexpr std::uint32_t kTagWord = 0x0000FFFFU;

// A pattern that differs in every byte, so a copy that is short, shifted, or
// reversed cannot pass.
std::uint8_t RegisterByte(const std::size_t index)
{
    return static_cast<std::uint8_t>(0xA0U + index);
}

void Fill(GuestCpuContext* registers)
{
    registers->Edi = kEdi;
    registers->Esi = kEsi;
    registers->Ebx = kEbx;
    registers->Edx = kEdx;
    registers->Ecx = kEcx;
    registers->Eax = kEax;
    registers->Ebp = kEbp;
    registers->Eip = kEip;
    registers->Esp = kEsp;
    registers->EFlags = kFlags;
    registers->ContextFlags = kContextFlags;
    registers->FloatSave.ControlWord = kControlWord;
    registers->FloatSave.StatusWord = kStatusWord;
    registers->FloatSave.TagWord = kTagWord;
    for (std::size_t index = 0; index < kRegisterAreaSize; ++index)
    {
        registers->FloatSave.RegisterArea[index] = RegisterByte(index);
    }
}

bool Matches(const GuestCpuContext& registers)
{
    return registers.Edi == kEdi && registers.Esi == kEsi &&
        registers.Ebx == kEbx && registers.Edx == kEdx &&
        registers.Ecx == kEcx && registers.Eax == kEax &&
        registers.Ebp == kEbp && registers.Eip == kEip &&
        registers.Esp == kEsp && registers.EFlags == kFlags;
}

bool FloatSaveMatches(const GuestCpuContext& registers)
{
    if (registers.FloatSave.ControlWord != kControlWord ||
        registers.FloatSave.StatusWord != kStatusWord ||
        registers.FloatSave.TagWord != kTagWord)
    {
        return false;
    }
    for (std::size_t index = 0; index < kRegisterAreaSize; ++index)
    {
        if (registers.FloatSave.RegisterArea[index] != RegisterByte(index))
        {
            return false;
        }
    }
    return true;
}

// The shared half: every host must expose the same field names holding the
// values written to them. On Windows this is CONTEXT itself, which is the point
// -- the engine's existing field accesses have to keep compiling.
bool ProbeFieldSet()
{
    GuestCpuContext registers{};
    Fill(&registers);
    if (!Matches(registers) || !FloatSaveMatches(registers) ||
        registers.ContextFlags != kContextFlags ||
        sizeof(registers.FloatSave.RegisterArea) != kRegisterAreaSize)
    {
        return false;
    }
    // The debug-register fields must exist everywhere too, because code that
    // mentions them is compiled on both hosts even when the feature using them
    // is disabled.
    registers.Dr0 = 0xDEADU;
    registers.Dr6 = 0xBEEFU;
    registers.Dr7 = 0xF00DU;
    return registers.Dr0 == 0xDEADU && registers.Dr6 == 0xBEEFU &&
        registers.Dr7 == 0xF00DU;
}

#if !defined(_WIN32)

bool ProbeUcontextRoundTrip()
{
    ucontext_t host{};
    // The kernel points fpregs at a save area it owns; a hand-built context has
    // to supply one, and the null case is checked separately below.
    _libc_fpstate x87{};
    host.uc_mcontext.fpregs = &x87;

    GuestCpuContext written{};
    Fill(&written);
    if (!repiu::platform::StoreGuestCpuContext(written, &host))
    {
        return false;
    }
    GuestCpuContext read{};
    if (!repiu::platform::LoadGuestCpuContext(&host, &read))
    {
        return false;
    }
    if (!Matches(read) || !FloatSaveMatches(read))
    {
        return false;
    }
    // Both stack-pointer slots must agree after a store, or a later read of the
    // same context would report a stale pointer.
    if (static_cast<std::uint32_t>(host.uc_mcontext.gregs[REG_ESP]) != kEsp ||
        static_cast<std::uint32_t>(host.uc_mcontext.gregs[REG_UESP]) != kEsp)
    {
        return false;
    }
    // A context with no x87 save area is not an error. The general registers
    // still move; the x87 half stays as the caller left it, which for a fresh
    // structure is zero.
    ucontext_t without_x87{};
    GuestCpuContext read_without_x87{};
    if (!repiu::platform::StoreGuestCpuContext(written, &without_x87) ||
        !repiu::platform::LoadGuestCpuContext(&without_x87,
                                              &read_without_x87) ||
        !Matches(read_without_x87) ||
        read_without_x87.FloatSave.StatusWord != 0U)
    {
        return false;
    }

    // Null arguments are refused rather than dereferenced: a signal handler
    // that lost its context must stop, not resume from garbage.
    GuestCpuContext ignored{};
    return !repiu::platform::LoadGuestCpuContext(nullptr, &ignored) &&
        !repiu::platform::LoadGuestCpuContext(&host, nullptr) &&
        !repiu::platform::StoreGuestCpuContext(written, nullptr);
}

bool ProbeFaultInfo()
{
    ucontext_t host{};
    siginfo_t info{};
    constexpr std::uint32_t kFaultAddress = 0x04123456U;
    info.si_addr = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(kFaultAddress));

    host.uc_mcontext.gregs[REG_ERR] = 0x2;  // page-fault write bit
    const auto write_fault =
        repiu::platform::ReadGuestFaultInfo(&info, &host);
    host.uc_mcontext.gregs[REG_ERR] = 0x0;
    const auto read_fault = repiu::platform::ReadGuestFaultInfo(&info, &host);
    const auto absent = repiu::platform::ReadGuestFaultInfo(nullptr, &host);

    return write_fault.valid && write_fault.write_access &&
        write_fault.fault_address == kFaultAddress && read_fault.valid &&
        !read_fault.write_access &&
        read_fault.fault_address == kFaultAddress && !absent.valid;
}

#endif

}  // namespace

bool RunGuestCpuContextProbe()
{
    const bool field_set_ok = ProbeFieldSet();
#if defined(_WIN32)
    // Windows has no conversion to test: the type is CONTEXT, and the host
    // hands the engine one directly.
    const bool round_trip_ok = true;
    const bool fault_info_ok = true;
#else
    const bool round_trip_ok = ProbeUcontextRoundTrip();
    const bool fault_info_ok = ProbeFaultInfo();
#endif
    const bool all = field_set_ok && round_trip_ok && fault_info_ok;
    std::cout << "guest_cpu_context_fields="
              << (field_set_ok ? "true" : "false")
              << "\nguest_cpu_context_round_trip="
              << (round_trip_ok ? "true" : "false")
              << "\nguest_cpu_context_fault_info="
              << (fault_info_ok ? "true" : "false")
              << "\nguest_cpu_context_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
