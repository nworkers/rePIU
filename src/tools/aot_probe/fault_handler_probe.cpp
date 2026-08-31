#include "fault_handler_probe.h"

#include "repiu/platform/fault_handler.h"
#include "repiu/platform/virtual_memory.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace repiu::tools
{
namespace
{

using repiu::platform::FaultDisposition;
using repiu::platform::FaultEvent;
using repiu::platform::FaultKind;
using repiu::platform::MemoryProtection;
using repiu::platform::MemoryReservation;

// This probe deliberately causes real faults and resumes from them, which is
// the one thing that cannot be checked any other way. Everything it does is
// therefore written so the handler always makes progress: a data fault is
// resolved by granting access, and a fault in the code buffer is resolved by
// moving Eip forward. A handler that resumed without changing anything would
// re-run the faulting instruction forever, and a hung probe is far worse than a
// failing one.

constexpr std::size_t kPageBytes = 4096U;
constexpr std::uint32_t kTrapFlag = 0x100U;
constexpr std::uint32_t kExpectedReturn = 0x5A5A1234U;
constexpr std::uint8_t kReadMarker = 0x5AU;
constexpr std::uint8_t kWriteMarker = 0x77U;
constexpr std::size_t kReadOffset = 16U;
constexpr std::size_t kWriteOffset = 32U;
// If the handler is entered more times than any scenario needs, something is
// looping. The escape hatch below fires instead of hanging.
constexpr int kMaxHandlerEntries = 64;

enum class Stage
{
    kIdle,
    kReadFault,
    kWriteFault,
    kBreakpoint,
    kSingleStep,
    kFinished,
};

struct ProbeState
{
    Stage stage = Stage::kIdle;
    int entries = 0;
    bool unexpected = false;

    std::uint8_t* data_page = nullptr;
    std::uint8_t* code_page = nullptr;

    bool read_fault_seen = false;
    bool read_fault_address_matched = false;
    bool read_fault_reported_read = false;

    bool write_fault_seen = false;
    bool write_fault_address_matched = false;
    bool write_fault_reported_write = false;

    // Whether the host's separately reported faulting-instruction address ever
    // disagrees with Eip. If it never does, ten call sites that read it can
    // simply read Eip instead.
    int instruction_address_checks = 0;
    int instruction_address_matches = 0;

    bool breakpoint_seen = false;
    // Both hosts must report Eip on the int3 byte. Windows does so natively and
    // the Linux backend rewinds to match; this is the assertion that keeps them
    // agreeing.
    bool breakpoint_eip_on_int3 = false;
    bool single_step_seen = false;
    bool single_step_eip_matched = false;
};

ProbeState g_state;

// Task 549. The stage crosses into a signal handler, so it is stored and read
// as a volatile object rather than an ordinary global.
//
// It is the only field this probe hands *to* the handler between two library
// calls -- `data_page` and `code_page` are set before `InstallFaultHandler`, so
// the compiler has to have written them by then. `stage` is set immediately
// before the faulting access and set back immediately after, and what sits
// between the two stores is a `volatile` load of a completely different object.
// That orders nothing here: nothing the compiler can see reads `stage` in
// between, so the first store is dead and deleting it is correct.
//
// GCC did delete it at -O2. The handler then ran with the stage still `kIdle`,
// fell to the `default` arm, and answered `kNotHandled`; the fault layer
// restored SIG_DFL as it must, and the retried read took the process down with
// the page still unreadable. Linux i386 Release died exactly there while the
// same source passed on a -O0 tree, which is what a dead store looks like from
// outside -- a fault handler that does nothing, on one build only.
void PublishStage(ProbeState* state, const Stage stage)
{
    *static_cast<volatile Stage*>(&state->stage) = stage;
}

Stage ReadStage(const ProbeState* state)
{
    return *static_cast<const volatile Stage*>(&state->stage);
}

FaultDisposition OnFault(FaultEvent* event, void* user_data)
{
    auto* state = static_cast<ProbeState*>(user_data);
    if (event == nullptr || state == nullptr || event->registers == nullptr)
    {
        return FaultDisposition::kNotHandled;
    }
    ++state->entries;
    if (state->entries > kMaxHandlerEntries)
    {
        // Escape hatch. Point at the `ret` and clear the trap flag, which ends
        // whatever loop this is with the process intact.
        event->registers->Eip = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(state->code_page) + 6U);
        event->registers->EFlags &= ~kTrapFlag;
        state->unexpected = true;
        return FaultDisposition::kResume;
    }

    const Stage stage = ReadStage(state);

    // Measured on every fault, whatever the scenario, because the claim is
    // about all of them and not just the one being exercised.
    if (stage != Stage::kIdle && stage != Stage::kFinished)
    {
        ++state->instruction_address_checks;
        if (event->instruction_address == event->registers->Eip)
        {
            ++state->instruction_address_matches;
        }
    }

    const auto data_base =
        reinterpret_cast<std::uintptr_t>(state->data_page);

    switch (stage)
    {
        case Stage::kReadFault:
        {
            if (event->kind != FaultKind::kAccessViolation)
            {
                state->unexpected = true;
                return FaultDisposition::kNotHandled;
            }
            state->read_fault_seen = true;
            state->read_fault_address_matched = event->access.valid &&
                event->access.fault_address ==
                    static_cast<std::uint32_t>(data_base + kReadOffset);
            state->read_fault_reported_read =
                event->access.valid && !event->access.write_access;
            // Granting access is what lets the faulting read complete on the
            // retry, so progress is guaranteed.
            repiu::platform::ProtectMemory(state->data_page, kPageBytes,
                                           MemoryProtection::kReadWrite,
                                           nullptr);
            return FaultDisposition::kResume;
        }
        case Stage::kWriteFault:
        {
            if (event->kind != FaultKind::kAccessViolation)
            {
                state->unexpected = true;
                return FaultDisposition::kNotHandled;
            }
            state->write_fault_seen = true;
            state->write_fault_address_matched = event->access.valid &&
                event->access.fault_address ==
                    static_cast<std::uint32_t>(data_base + kWriteOffset);
            state->write_fault_reported_write =
                event->access.valid && event->access.write_access;
            repiu::platform::ProtectMemory(state->data_page, kPageBytes,
                                           MemoryProtection::kReadWrite,
                                           nullptr);
            return FaultDisposition::kResume;
        }
        case Stage::kBreakpoint:
        {
            if (event->kind != FaultKind::kBreakpoint)
            {
                state->unexpected = true;
                return FaultDisposition::kNotHandled;
            }
            state->breakpoint_seen = true;
            const auto code_base =
                reinterpret_cast<std::uintptr_t>(state->code_page);
            state->breakpoint_eip_on_int3 =
                event->registers->Eip == static_cast<std::uint32_t>(code_base);
            // Advancing past the int3 is the handler's job on both hosts:
            // resuming unchanged would re-execute it forever.
            event->registers->Eip =
                static_cast<std::uint32_t>(code_base + 1U);
            // Arming the trap flag here is exactly how the engine begins a
            // single-step run, so the next instruction must trap.
            event->registers->EFlags |= kTrapFlag;
            PublishStage(state, Stage::kSingleStep);
            return FaultDisposition::kResume;
        }
        case Stage::kSingleStep:
        {
            // Clearing the trap flag comes first in every path below: leaving
            // it set would trap again on the next instruction, and again after
            // that.
            event->registers->EFlags &= ~kTrapFlag;
            PublishStage(state, Stage::kFinished);
            if (event->kind != FaultKind::kSingleStep)
            {
                state->unexpected = true;
                return FaultDisposition::kResume;
            }
            state->single_step_seen = true;
            // One instruction executed: the five-byte `mov eax, imm32` that
            // follows the int3, so Eip is now on the `ret`.
            const auto code_base =
                reinterpret_cast<std::uintptr_t>(state->code_page);
            state->single_step_eip_matched =
                event->registers->Eip ==
                static_cast<std::uint32_t>(code_base + 6U);
            return FaultDisposition::kResume;
        }
        case Stage::kIdle:
        case Stage::kFinished:
        default:
            // Nothing was expected, so this fault belongs to someone else --
            // a C++ exception passing through a vectored handler, for one.
            return FaultDisposition::kNotHandled;
    }
}

bool ProbeInstallRefusals()
{
    // Removing without installing is a caller error, not a no-op success.
    if (repiu::platform::RemoveFaultHandler())
    {
        return false;
    }
    if (repiu::platform::InstallFaultHandler(nullptr, nullptr))
    {
        return false;
    }
    if (!repiu::platform::InstallFaultHandler(&OnFault, &g_state))
    {
        return false;
    }
    // Two handlers disagreeing about who owns a fault is not worth supporting.
    const bool second_refused =
        !repiu::platform::InstallFaultHandler(&OnFault, &g_state);
    const bool removed = repiu::platform::RemoveFaultHandler();
    return second_refused && removed;
}

bool ProbeDataFaults()
{
    const MemoryReservation page = repiu::platform::ReserveMemory(
        nullptr, kPageBytes, true, MemoryProtection::kReadWrite);
    if (!page.valid)
    {
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(page.base);
    bytes[kReadOffset] = kReadMarker;

    g_state = ProbeState{};
    g_state.data_page = bytes;
    if (!repiu::platform::InstallFaultHandler(&OnFault, &g_state))
    {
        repiu::platform::ReleaseMemory(page.base, page.size);
        return false;
    }

    bool ok = true;

    // A read of memory that cannot be read. The handler grants access and the
    // read completes, which is the same shape as the engine handling a guest
    // access to a page it had protected.
    ok = ok && repiu::platform::ProtectMemory(bytes, kPageBytes,
                                              MemoryProtection::kNoAccess,
                                              nullptr);
    PublishStage(&g_state, Stage::kReadFault);
    const std::uint8_t observed = *static_cast<volatile std::uint8_t*>(
        static_cast<void*>(bytes + kReadOffset));
    PublishStage(&g_state, Stage::kIdle);
    ok = ok && g_state.read_fault_seen && g_state.read_fault_address_matched &&
        g_state.read_fault_reported_read && observed == kReadMarker;

    // A write to memory that may only be read. The direction the host reports
    // is what tells self-modifying-code detection a write happened at all.
    ok = ok && repiu::platform::ProtectMemory(bytes, kPageBytes,
                                              MemoryProtection::kReadOnly,
                                              nullptr);
    PublishStage(&g_state, Stage::kWriteFault);
    *static_cast<volatile std::uint8_t*>(
        static_cast<void*>(bytes + kWriteOffset)) = kWriteMarker;
    PublishStage(&g_state, Stage::kIdle);
    ok = ok && g_state.write_fault_seen &&
        g_state.write_fault_address_matched &&
        g_state.write_fault_reported_write &&
        bytes[kWriteOffset] == kWriteMarker;

    ok = ok && !g_state.unexpected;
    // Two faults were taken, and the host's reported instruction address
    // agreed with Eip on both.
    ok = ok && g_state.instruction_address_checks == 2 &&
        g_state.instruction_address_matches ==
            g_state.instruction_address_checks;
    ok = repiu::platform::RemoveFaultHandler() && ok;
    ok = repiu::platform::ReleaseMemory(page.base, page.size) && ok;
    return ok;
}

// The engine's own mechanism, end to end: hit a planted int3, arm the trap
// flag from inside the handler, take the single step that follows, disarm, and
// let the code run to completion.
bool ProbeBreakpointAndSingleStep()
{
    const MemoryReservation page = repiu::platform::ReserveMemory(
        nullptr, kPageBytes, true, MemoryProtection::kReadWrite);
    if (!page.valid)
    {
        return false;
    }
    auto* code = static_cast<std::uint8_t*>(page.base);

    // int3
    // mov eax, 0x5A5A1234
    // ret
    const std::uint8_t program[] = {
        0xCCU,
        0xB8U, 0x34U, 0x12U, 0x5AU, 0x5AU,
        0xC3U,
    };
    std::memcpy(code, program, sizeof(program));

    bool ok = repiu::platform::ProtectMemory(
        code, kPageBytes, MemoryProtection::kExecuteReadWrite, nullptr);
    if (!ok)
    {
        repiu::platform::ReleaseMemory(page.base, page.size);
        return false;
    }

    g_state = ProbeState{};
    g_state.code_page = code;
    if (!repiu::platform::InstallFaultHandler(&OnFault, &g_state))
    {
        repiu::platform::ReleaseMemory(page.base, page.size);
        return false;
    }

    PublishStage(&g_state, Stage::kBreakpoint);
    using Program = std::uint32_t (*)();
    Program entry = nullptr;
    std::memcpy(&entry, &code, sizeof(entry));
    const std::uint32_t returned = entry();
    PublishStage(&g_state, Stage::kIdle);

    // Reported individually, because "the trap round trip failed" says nothing
    // about which half of it did.
    std::cout << "  breakpoint_seen=" << (g_state.breakpoint_seen ? 1 : 0)
              << " breakpoint_eip_on_int3="
              << (g_state.breakpoint_eip_on_int3 ? 1 : 0)
              << " single_step_seen=" << (g_state.single_step_seen ? 1 : 0)
              << " single_step_eip_matched="
              << (g_state.single_step_eip_matched ? 1 : 0)
              << " returned=0x" << std::hex << returned << std::dec
              << " unexpected=" << (g_state.unexpected ? 1 : 0)
              << " entries=" << g_state.entries
              << " instruction_address_matches="
              << g_state.instruction_address_matches << "/"
              << g_state.instruction_address_checks << "\n";

    ok = ok && g_state.instruction_address_checks == 2 &&
        g_state.instruction_address_matches ==
            g_state.instruction_address_checks;
    ok = ok && g_state.breakpoint_seen && g_state.breakpoint_eip_on_int3 &&
        g_state.single_step_seen && g_state.single_step_eip_matched &&
        returned == kExpectedReturn && !g_state.unexpected;

    ok = repiu::platform::RemoveFaultHandler() && ok;
    ok = repiu::platform::ReleaseMemory(page.base, page.size) && ok;
    return ok;
}

}  // namespace

bool RunFaultHandlerProbe()
{
    const bool refusals_ok = ProbeInstallRefusals();
    const bool data_ok = ProbeDataFaults();
    const bool trap_ok = ProbeBreakpointAndSingleStep();
    const bool all = refusals_ok && data_ok && trap_ok;

    std::cout << "fault_handler_install_refusals="
              << (refusals_ok ? "true" : "false")
              << "\nfault_handler_data_faults=" << (data_ok ? "true" : "false")
              << "\nfault_handler_breakpoint_single_step="
              << (trap_ok ? "true" : "false")
              << "\nfault_handler_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
