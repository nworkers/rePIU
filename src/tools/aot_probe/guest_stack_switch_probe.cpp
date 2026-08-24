#include "guest_stack_switch_probe.h"

#include "repiu/platform/fault_handler.h"
#include "repiu/platform/guest_stack_switch.h"
#include "repiu/platform/thunk_calling_convention.h"
#include "repiu/platform/virtual_memory.h"

#include <cstddef>
#include <cstdint>
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

// Task 503d-16. The call state, declared here rather than included.
//
// The engine's `StackSwitchCallState` lives in `execution/thread_context.h`,
// which drags in the whole Win32 execution layer -- the Glide backend, the
// audio outputs, the census tables. A probe that had to link all of that could
// not run on Linux at all, which is where the assembly under test is new.
//
// This is not a lookalike allowed to drift. Both structures are pinned to the
// same offsets in `guest_stack_switch.h`, by the same `static_assert`s, and the
// assembly reads its operands from that header too. Change a field there and
// whichever of the two does not follow stops compiling.
struct ProbeCallState
{
    std::uint32_t entry_address = 0;
    std::uint32_t initial_esp = 0;
    std::uint32_t host_esp = 0;
    std::uint32_t guest_return_esp = 0;
    std::uint32_t result_code = 0;
    std::uint32_t enable_single_step_trace = 0;
    std::uint32_t host_fs = 0;
    std::uint32_t host_ds = 0;
    std::uint32_t host_es = 0;
    std::uint32_t host_gs = 0;
    std::uint32_t host_ss = 0;
    std::uint32_t guest_stack_base = 0;
    std::uint32_t guest_stack_limit = 0;
    std::uint32_t host_stack_base = 0;
    std::uint32_t host_stack_limit = 0;
};

static_assert(offsetof(ProbeCallState, entry_address) ==
              REPIU_STACK_SWITCH_ENTRY_ADDRESS);
static_assert(offsetof(ProbeCallState, initial_esp) ==
              REPIU_STACK_SWITCH_INITIAL_ESP);
static_assert(offsetof(ProbeCallState, host_esp) ==
              REPIU_STACK_SWITCH_HOST_ESP);
static_assert(offsetof(ProbeCallState, guest_return_esp) ==
              REPIU_STACK_SWITCH_GUEST_RETURN_ESP);
static_assert(offsetof(ProbeCallState, result_code) ==
              REPIU_STACK_SWITCH_RESULT_CODE);
static_assert(offsetof(ProbeCallState, enable_single_step_trace) ==
              REPIU_STACK_SWITCH_SINGLE_STEP);
static_assert(offsetof(ProbeCallState, host_fs) == REPIU_STACK_SWITCH_HOST_FS);
static_assert(offsetof(ProbeCallState, host_ds) == REPIU_STACK_SWITCH_HOST_DS);
static_assert(offsetof(ProbeCallState, host_es) == REPIU_STACK_SWITCH_HOST_ES);
static_assert(offsetof(ProbeCallState, host_gs) == REPIU_STACK_SWITCH_HOST_GS);
static_assert(offsetof(ProbeCallState, host_ss) == REPIU_STACK_SWITCH_HOST_SS);
static_assert(offsetof(ProbeCallState, guest_stack_base) ==
              REPIU_STACK_SWITCH_GUEST_STACK_BASE);
static_assert(offsetof(ProbeCallState, guest_stack_limit) ==
              REPIU_STACK_SWITCH_GUEST_STACK_LIMIT);
static_assert(offsetof(ProbeCallState, host_stack_base) ==
              REPIU_STACK_SWITCH_HOST_STACK_BASE);
static_assert(offsetof(ProbeCallState, host_stack_limit) ==
              REPIU_STACK_SWITCH_HOST_STACK_LIMIT);

constexpr std::size_t kGuestStackBytes = 64U * 1024U;
constexpr std::size_t kPageBytes = 4096U;
constexpr std::uint8_t kGuardedMarker = 0x71U;

struct SwitchState
{
    std::uintptr_t guest_stack_low = 0;
    std::uintptr_t guest_stack_high = 0;
    ProbeCallState* call_state = nullptr;

    bool handler_ran = false;
    bool faulted_on_guest_stack = false;
    std::uint8_t* guarded_page = nullptr;
};

SwitchState g_switch;

}  // namespace
}  // namespace repiu::tools

// Read by the guest-side assembly by name, so plain C symbols.
extern "C" {

std::uint32_t g_repiu_guest_switch_probe_esp = 0;
std::uint32_t g_repiu_guest_switch_probe_state_arg = 0;
std::uint32_t g_repiu_guest_switch_probe_guarded = 0;
std::uint32_t g_repiu_guest_switch_probe_resumed = 0;

void RepiuGuestStackSwitchProbeEntry();
void RepiuGuestStackSwitchProbeFaultEntry();

// The engine's own entries, under test. `RecoverHostStackException` is declared
// as returning a value here so the caller can read EAX: leaving zero there is
// the whole of its job, and a void declaration would hide whether it did it.
std::uint32_t REPIU_THUNK_RESOLVER_CALL CallGuestEntryWithStack(void* state);
void REPIU_THUNK_RESOLVER_CALL RecoverGuestStackException();
std::uint32_t RecoverHostStackException();

}

#if defined(_MSC_VER) && defined(_M_IX86)
// The Windows half of the guest-side stubs, matching the GAS file beside this
// one. Both hosts run the same C++ below against the same contract, which is
// what makes the two implementations of the switch comparable rather than
// merely both present.
extern "C" __declspec(naked) void RepiuGuestStackSwitchProbeEntry()
{
    __asm
    {
        mov eax, [esp + 4]
        mov g_repiu_guest_switch_probe_state_arg, eax
        mov g_repiu_guest_switch_probe_esp, esp
        mov ebx, 0DEADBEEFh
        mov esi, 0DEADBEEFh
        mov edi, 0DEADBEEFh
        ret
    }
}

extern "C" __declspec(naked) void RepiuGuestStackSwitchProbeFaultEntry()
{
    __asm
    {
        mov g_repiu_guest_switch_probe_esp, esp
        mov eax, g_repiu_guest_switch_probe_guarded
        mov al, byte ptr [eax]
        mov g_repiu_guest_switch_probe_resumed, 1
        ret
    }
}
#endif

namespace repiu::tools
{
namespace
{

// What the engine's own handler does when a guest fault cannot be resumed: it
// stops being a return from the fault and becomes a return from the switch.
// Written out here rather than called, because `RecoverToHost` lives inside the
// trampoline and this probe deliberately does not link it.
FaultDisposition OnGuestFault(FaultEvent* event, void* user_data)
{
    auto* state = static_cast<SwitchState*>(user_data);
    if (event == nullptr || state == nullptr || event->registers == nullptr ||
        event->kind != FaultKind::kAccessViolation ||
        state->call_state == nullptr)
    {
        return FaultDisposition::kNotHandled;
    }
    state->handler_ran = true;
    const std::uintptr_t faulting_esp =
        static_cast<std::uintptr_t>(event->registers->Esp);
    state->faulted_on_guest_stack = faulting_esp >= state->guest_stack_low &&
        faulting_esp < state->guest_stack_high;

    using RegisterField = decltype(event->registers->Eip);
    event->registers->Eip = static_cast<RegisterField>(
        reinterpret_cast<std::uintptr_t>(&RecoverGuestStackException));
    event->registers->Ecx = static_cast<RegisterField>(
        reinterpret_cast<std::uintptr_t>(state->call_state));
    event->registers->Esp =
        static_cast<RegisterField>(state->call_state->host_esp);
    // The trap and direction flags are cleared for the same reason the engine
    // clears them: what resumes is host code, and it inherits neither.
    event->registers->EFlags &= ~static_cast<RegisterField>(
        REPIU_STACK_SWITCH_TRAP_FLAG);
    event->registers->EFlags &= ~static_cast<RegisterField>(0x00000400U);
    return FaultDisposition::kResume;
}

void ArmGuestStack(const MemoryReservation& guest_stack, ProbeCallState* state)
{
    const auto low = reinterpret_cast<std::uintptr_t>(guest_stack.base);
    const std::uintptr_t high = low + guest_stack.size;
    g_switch.guest_stack_low = low;
    g_switch.guest_stack_high = high;
    g_switch.call_state = state;

    // Sixteen bytes of slack below the top, as the loader leaves for the guest.
    state->initial_esp = static_cast<std::uint32_t>(high - 16U);
    state->guest_stack_base = static_cast<std::uint32_t>(high);
    state->guest_stack_limit = static_cast<std::uint32_t>(low);

    g_repiu_guest_switch_probe_esp = 0;
    g_repiu_guest_switch_probe_state_arg = 0;
    g_repiu_guest_switch_probe_resumed = 0;
}

// The switch itself: onto the guest's stack, into its entry point, and back
// with the caller's frame as it was.
bool ProbeSwitchAndReturn()
{
    const MemoryReservation guest_stack = repiu::platform::ReserveMemory(
        nullptr, kGuestStackBytes, true, MemoryProtection::kReadWrite);
    if (!guest_stack.valid)
    {
        return false;
    }

    g_switch = SwitchState{};
    ProbeCallState state;
    ArmGuestStack(guest_stack, &state);
    state.entry_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&RepiuGuestStackSwitchProbeEntry));

    // The switch restores the caller's stack pointer by hand. Getting that
    // wrong corrupts what is nearby rather than crashing outright.
    volatile std::uint32_t canary = 0x2BADF00DU;
    const std::uint32_t returned = CallGuestEntryWithStack(&state);

    const auto entered_esp =
        static_cast<std::uintptr_t>(g_repiu_guest_switch_probe_esp);
    bool ok = returned == REPIU_STACK_SWITCH_RETURNED &&
        state.result_code == 0U && canary == 0x2BADF00DU;
    // The entry really ran, and it ran on the stack it was given.
    ok = ok && entered_esp >= g_switch.guest_stack_low &&
        entered_esp < g_switch.guest_stack_high;
    // The call state reached the entry as the argument beneath the return
    // address, which is how the guest's own return path finds it again.
    ok = ok && g_repiu_guest_switch_probe_state_arg ==
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&state));
    // Host state was recorded before the switch, in both places: the call state
    // for the crash report, the globals for recovery.
    ok = ok && state.host_esp != 0U &&
        g_repiu_dbt_host_esp == state.host_esp &&
        g_recovery_host_ds == state.host_ds &&
        g_recovery_host_es == state.host_es &&
        g_recovery_host_fs == state.host_fs &&
        g_recovery_host_gs == state.host_gs;
    // The guest pushed nothing it did not pop, so the stack came back where it
    // started -- the switch pushed the call state before the call and popped it
    // again after.
    ok = ok && state.guest_return_esp == state.initial_esp;

    ok = repiu::platform::ReleaseMemory(guest_stack.base, guest_stack.size) &&
        ok;
    return ok;
}

// The reason this sub-stage exists: a fault taken on the guest stack, where the
// engine does not resume the guest but returns from the switch instead.
bool ProbeRecoverFromGuestFault()
{
    const MemoryReservation guest_stack = repiu::platform::ReserveMemory(
        nullptr, kGuestStackBytes, true, MemoryProtection::kReadWrite);
    if (!guest_stack.valid)
    {
        return false;
    }
    const MemoryReservation guarded = repiu::platform::ReserveMemory(
        nullptr, kPageBytes, true, MemoryProtection::kReadWrite);
    if (!guarded.valid)
    {
        repiu::platform::ReleaseMemory(guest_stack.base, guest_stack.size);
        return false;
    }
    auto* guarded_bytes = static_cast<std::uint8_t*>(guarded.base);
    guarded_bytes[0] = kGuardedMarker;

    g_switch = SwitchState{};
    ProbeCallState state;
    ArmGuestStack(guest_stack, &state);
    state.entry_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(
            &RepiuGuestStackSwitchProbeFaultEntry));
    g_switch.guarded_page = guarded_bytes;
    g_repiu_guest_switch_probe_guarded = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(guarded_bytes));

    bool ok = repiu::platform::InstallFaultHandler(&OnGuestFault, &g_switch);
    ok = ok && repiu::platform::ProtectMemory(guarded_bytes, kPageBytes,
                                              MemoryProtection::kNoAccess,
                                              nullptr);

    volatile std::uint32_t canary = 0x5A5AC0DEU;
    const std::uint32_t returned = CallGuestEntryWithStack(&state);

    ok = ok && returned == REPIU_STACK_SWITCH_RECOVERED &&
        canary == 0x5A5AC0DEU;
    ok = ok && g_switch.handler_ran && g_switch.faulted_on_guest_stack;
    // The guest did not carry on past the fault. Recovery took its place, which
    // is the difference between this and simply granting the access.
    ok = ok && g_repiu_guest_switch_probe_resumed == 0U;

    ok = repiu::platform::RemoveFaultHandler() && ok;
    ok = repiu::platform::ReleaseMemory(guarded.base, guarded.size) && ok;
    ok = repiu::platform::ReleaseMemory(guest_stack.base, guest_stack.size) &&
        ok;
    return ok;
}

// The third entry is a single instruction pair, and its whole contract is the
// value it leaves behind: zero, so that host code redirected here reports the
// fault unhandled rather than whatever happened to be in EAX.
bool ProbeHostStackRecoveryReturnsZero()
{
    return RecoverHostStackException() == 0U;
}

}  // namespace

bool RunGuestStackSwitchProbe()
{
    const bool switch_ok = ProbeSwitchAndReturn();
    const bool recover_ok = ProbeRecoverFromGuestFault();
    const bool host_recover_ok = ProbeHostStackRecoveryReturnsZero();
    const bool all = switch_ok && recover_ok && host_recover_ok;
    std::cout << "guest_stack_switch_call=" << (switch_ok ? "true" : "false")
              << "\nguest_stack_switch_recover="
              << (recover_ok ? "true" : "false")
              << "\nguest_stack_switch_host_recover="
              << (host_recover_ok ? "true" : "false")
              << "\nguest_stack_switch_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
