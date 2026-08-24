#include "stack_bridge_probe.h"

#include "repiu/platform/fault_handler.h"
#include "repiu/platform/thunk_calling_convention.h"
#include "repiu/platform/virtual_memory.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

#if defined(_WIN32)
#include <intrin.h>
#endif

namespace repiu::tools
{
namespace
{

using repiu::platform::FaultDisposition;
using repiu::platform::FaultEvent;
using repiu::platform::FaultKind;
using repiu::platform::MemoryProtection;
using repiu::platform::MemoryReservation;

// The frame `pusha` leaves behind, in the order it pushes: EDI first at the
// lowest address, EAX last. Slot 3 is the stack pointer as it was before the
// push, which is what makes the layout self-checking.
constexpr std::size_t kFrameEdi = 0;
constexpr std::size_t kFrameEsp = 3;
constexpr std::size_t kFrameEax = 7;
constexpr std::size_t kFrameSlots = 8;
constexpr std::size_t kFrameBytes = kFrameSlots * sizeof(std::uint32_t);

constexpr std::uint32_t kResolverReturnMarker = 0xC0FFEE01U;
// Task 503d-12: what the harness returns when the refusal path redirected it.
constexpr std::uint32_t kFallbackMarker = 0xFA11BACCU;
constexpr std::size_t kHostStackBytes = 64U * 1024U;
constexpr std::size_t kPageBytes = 4096U;
constexpr std::uint8_t kGuardedMarker = 0x3DU;

struct BridgeState
{
    bool resolver_ran = false;
    bool context_matched = false;
    bool frame_self_consistent = false;
    bool ran_on_host_stack = false;

    // The fault scenario.
    bool touched_guarded_page = false;
    bool fault_seen_on_host_stack = false;
    std::uint8_t guarded_value = 0;

    void* expected_context = nullptr;
    std::uintptr_t host_stack_low = 0;
    std::uintptr_t host_stack_high = 0;
    std::uint8_t* guarded_page = nullptr;
    bool expect_fault = false;
};

BridgeState g_bridge;

}  // namespace
}  // namespace repiu::tools

// The bridge reads these by name from assembly, so they are plain C symbols.
// The two stack-bound pairs exist on both hosts and are read only by the
// Windows thunk; that asymmetry is the finding this probe records, not an
// oversight.
extern "C" {
void* g_repiu_bridge_context = nullptr;
std::uint32_t g_repiu_bridge_host_esp = 0;
std::uint32_t g_repiu_bridge_host_stack_base = 0;
std::uint32_t g_repiu_bridge_host_stack_limit = 0;
std::uint32_t g_repiu_bridge_guest_stack_base = 0;
std::uint32_t g_repiu_bridge_guest_stack_limit = 0;

// Task 503d-12: stdcall, as the five shipped resolvers are, so the macro is
// expanded here against the same calling convention it is expanded against
// there.
void REPIU_THUNK_RESOLVER_CALL RepiuStackBridgeProbeResolver(
    void* context, std::uint32_t* frame);

// Declared as returning a value so the caller can read EAX: the resolver writes
// a marker into the saved EAX slot, `popa` applies it, and the return value is
// then proof that editing the frame reaches the guest's registers.
std::uint32_t RepiuStackBridgeProbeThunk();

// Task 503d-12: the refusal that resumes past the pushed dispatch address, and
// the site-shaped caller it has to land inside. Only the harness is called from
// C++; the thunk is named here because the harness calls it by symbol.
std::uint32_t RepiuStackBridgeProbeFallbackThunk();
std::uint32_t RepiuStackBridgeFallbackHarness();
}

extern "C" void REPIU_THUNK_RESOLVER_CALL RepiuStackBridgeProbeResolver(
    void* context, std::uint32_t* frame)
{
    auto& state = repiu::tools::g_bridge;
    state.resolver_ran = true;
    state.context_matched = context == state.expected_context;

    // `pusha` stores the pre-push stack pointer in slot 3, so it must equal the
    // address one past the end of the frame. Nothing else pins the layout down
    // this cheaply, and a wrong layout would have the resolver editing the
    // wrong register.
    state.frame_self_consistent = frame != nullptr &&
        frame[repiu::tools::kFrameEsp] ==
            static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(frame) +
                repiu::tools::kFrameBytes);

    // Whether the switch happened at all: a local of this function lives on
    // whatever stack is current.
    volatile std::uint8_t here = 0;
    const auto local = reinterpret_cast<std::uintptr_t>(&here);
    state.ran_on_host_stack =
        local >= state.host_stack_low && local < state.host_stack_high;

    if (state.expect_fault && state.guarded_page != nullptr)
    {
        // The point of the exercise: take a fault here, on the switched stack.
        // On Windows this is what the TIB stack bounds are swapped for; on
        // Linux nothing was swapped, so if the handler still runs and resumes,
        // nothing needed to be.
        state.guarded_value = *static_cast<volatile std::uint8_t*>(
            static_cast<void*>(state.guarded_page));
        state.touched_guarded_page = true;
    }

    if (frame != nullptr)
    {
        frame[repiu::tools::kFrameEax] = repiu::tools::kResolverReturnMarker;
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
// The Windows half of the same contract, kept beside the macro it mirrors. It
// is the shape the five shipped dispatch thunks use, including the TIB writes
// the Linux version has no counterpart for.
extern "C" __declspec(naked) std::uint32_t RepiuStackBridgeProbeThunk()
{
    __asm
    {
        pushfd
        pushad
        cld
        mov esi, esp
        mov ecx, dword ptr [g_repiu_bridge_context]
        test ecx, ecx
        jz done
        mov eax, dword ptr [g_repiu_bridge_host_esp]
        test eax, eax
        jz done

        mov edx, dword ptr [g_repiu_bridge_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_bridge_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call RepiuStackBridgeProbeResolver
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_bridge_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_bridge_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi

    done:
        popad
        popfd
        ret
    }
}

// Task 503d-12. The refusal that resumes rather than returns, with the same
// fifteen-byte count and the same slots as the shipped thunks use, and the
// site-shaped caller it lands inside.
extern "C" __declspec(naked) std::uint32_t RepiuStackBridgeProbeFallbackThunk()
{
    __asm
    {
        pushfd
        pushad
        cld
        mov esi, esp
        mov ecx, dword ptr [g_repiu_bridge_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_bridge_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_bridge_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_bridge_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call RepiuStackBridgeProbeResolver
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_bridge_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_bridge_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        mov eax, dword ptr [esp + 40]
        add eax, 15
        mov dword ptr [esp + 36], eax
        popad
        popfd
        ret
    }
}

extern "C" __declspec(naked) std::uint32_t RepiuStackBridgeFallbackHarness()
{
    __asm
    {
        mov eax, offset landing
        sub eax, 15
        push eax
        call RepiuStackBridgeProbeFallbackThunk
        // The bridge was crossed and returned normally. eax carries whatever
        // the resolver left in the frame's saved EAX.
        add esp, 4
        ret

    landing:
        // Reached by the refusal path, with the pushed dispatch address still
        // on the stack as it is at a real site's fallback label.
        add esp, 4
        mov eax, 0FA11BACCh
        ret
    }
}
#endif

namespace repiu::tools
{
namespace
{

FaultDisposition OnBridgeFault(FaultEvent* event, void* user_data)
{
    auto* state = static_cast<BridgeState*>(user_data);
    if (event == nullptr || state == nullptr ||
        event->kind != FaultKind::kAccessViolation ||
        state->guarded_page == nullptr)
    {
        return FaultDisposition::kNotHandled;
    }
    state->fault_seen_on_host_stack = true;
    // Granting access lets the faulting read complete, so the resolver -- and
    // with it the bridge -- carries on rather than looping.
    repiu::platform::ProtectMemory(state->guarded_page, kPageBytes,
                                   MemoryProtection::kReadWrite, nullptr);
    return FaultDisposition::kResume;
}

// Points the bridge at a freshly reserved host stack and, on Windows, at the
// stack bounds the TIB has to be told about.
bool ArmBridge(const MemoryReservation& host_stack)
{
    const auto low = reinterpret_cast<std::uintptr_t>(host_stack.base);
    const std::uintptr_t high = low + host_stack.size;
    g_bridge.host_stack_low = low;
    g_bridge.host_stack_high = high;
    g_bridge.expected_context = &g_bridge;

    g_repiu_bridge_context = &g_bridge;
    // Sixteen bytes of slack below the top, then the bridge aligns for itself.
    g_repiu_bridge_host_esp = static_cast<std::uint32_t>(high - 16U);
    g_repiu_bridge_host_stack_base = static_cast<std::uint32_t>(high);
    g_repiu_bridge_host_stack_limit = static_cast<std::uint32_t>(low);
#if defined(_WIN32)
    // The stack the probe itself is running on plays the guest's part, so its
    // bounds are what the thunk must put back.
    g_repiu_bridge_guest_stack_base = __readfsdword(4);
    g_repiu_bridge_guest_stack_limit = __readfsdword(8);
#endif
    return true;
}

void DisarmBridge()
{
    g_repiu_bridge_context = nullptr;
    g_repiu_bridge_host_esp = 0;
}

bool ProbeBridgeContract()
{
    const MemoryReservation host_stack = repiu::platform::ReserveMemory(
        nullptr, kHostStackBytes, true, MemoryProtection::kReadWrite);
    if (!host_stack.valid)
    {
        return false;
    }

    g_bridge = BridgeState{};
    ArmBridge(host_stack);

    // A canary either side of the call: the bridge restores the caller's stack
    // pointer by hand, and getting that wrong corrupts whatever is nearby
    // rather than crashing outright.
    volatile std::uint32_t canary = 0x1BADD00DU;
    const std::uint32_t returned = RepiuStackBridgeProbeThunk();

    bool ok = g_bridge.resolver_ran && g_bridge.context_matched &&
        g_bridge.frame_self_consistent && g_bridge.ran_on_host_stack &&
        returned == kResolverReturnMarker && canary == 0x1BADD00DU;

    // With no context the bridge must refuse, leaving the registers untouched
    // -- so the marker from the previous call must not reappear.
    DisarmBridge();
    g_bridge.resolver_ran = false;
    const std::uint32_t refused = RepiuStackBridgeProbeThunk();
    ok = ok && !g_bridge.resolver_ran && refused != kResolverReturnMarker;

    ok = repiu::platform::ReleaseMemory(host_stack.base, host_stack.size) && ok;
    return ok;
}

// Task 503d-12. What four of the five shipped thunks do when there is no
// context: rather than returning to the site, they resume a fixed number of
// bytes past the dispatch address the site pushed. Getting that arithmetic or
// either frame slot wrong lands generated code in the middle of an instruction,
// and nothing about the refusal path is exercised by the contract probe above.
bool ProbeRefusalFallback()
{
    const MemoryReservation host_stack = repiu::platform::ReserveMemory(
        nullptr, kHostStackBytes, true, MemoryProtection::kReadWrite);
    if (!host_stack.valid)
    {
        return false;
    }

    // Armed first: the same instantiation must still cross the bridge normally,
    // so the refusal path is shown to be the only thing that changed.
    g_bridge = BridgeState{};
    ArmBridge(host_stack);
    volatile std::uint32_t canary = 0x0D15EA5EU;
    const std::uint32_t crossed = RepiuStackBridgeFallbackHarness();
    bool ok = g_bridge.resolver_ran && g_bridge.ran_on_host_stack &&
        g_bridge.frame_self_consistent && crossed == kResolverReturnMarker &&
        canary == 0x0D15EA5EU;

    // Disarmed: the harness pushed an address fifteen bytes short of its
    // landing pad, so only a refusal that read slot 10, added fifteen, and wrote
    // slot 9 arrives there.
    DisarmBridge();
    g_bridge.resolver_ran = false;
    const std::uint32_t redirected = RepiuStackBridgeFallbackHarness();
    ok = ok && !g_bridge.resolver_ran && redirected == kFallbackMarker &&
        canary == 0x0D15EA5EU;

    ok = repiu::platform::ReleaseMemory(host_stack.base, host_stack.size) && ok;
    return ok;
}

// The claim under test: on Linux nothing has to be told where the stack is for
// a fault taken on the switched stack to be delivered and resumed from.
bool ProbeFaultOnHostStack()
{
    const MemoryReservation host_stack = repiu::platform::ReserveMemory(
        nullptr, kHostStackBytes, true, MemoryProtection::kReadWrite);
    if (!host_stack.valid)
    {
        return false;
    }
    const MemoryReservation guarded = repiu::platform::ReserveMemory(
        nullptr, kPageBytes, true, MemoryProtection::kReadWrite);
    if (!guarded.valid)
    {
        repiu::platform::ReleaseMemory(host_stack.base, host_stack.size);
        return false;
    }

    auto* guarded_bytes = static_cast<std::uint8_t*>(guarded.base);
    guarded_bytes[0] = kGuardedMarker;

    g_bridge = BridgeState{};
    ArmBridge(host_stack);
    g_bridge.guarded_page = guarded_bytes;
    g_bridge.expect_fault = true;

    bool ok = repiu::platform::InstallFaultHandler(&OnBridgeFault, &g_bridge);
    ok = ok && repiu::platform::ProtectMemory(guarded_bytes, kPageBytes,
                                              MemoryProtection::kNoAccess,
                                              nullptr);
    volatile std::uint32_t canary = 0x5EED1234U;
    const std::uint32_t returned = RepiuStackBridgeProbeThunk();

    ok = ok && g_bridge.resolver_ran && g_bridge.ran_on_host_stack &&
        g_bridge.fault_seen_on_host_stack && g_bridge.touched_guarded_page &&
        g_bridge.guarded_value == kGuardedMarker &&
        returned == kResolverReturnMarker && canary == 0x5EED1234U;

    DisarmBridge();
    ok = repiu::platform::RemoveFaultHandler() && ok;
    ok = repiu::platform::ReleaseMemory(guarded.base, guarded.size) && ok;
    ok = repiu::platform::ReleaseMemory(host_stack.base, host_stack.size) && ok;
    return ok;
}

}  // namespace

bool RunStackBridgeProbe()
{
    const bool contract_ok = ProbeBridgeContract();
    const bool refusal_ok = ProbeRefusalFallback();
    const bool fault_ok = ProbeFaultOnHostStack();
    const bool all = contract_ok && refusal_ok && fault_ok;
    std::cout << "stack_bridge_contract=" << (contract_ok ? "true" : "false")
              << "\nstack_bridge_refusal_fallback="
              << (refusal_ok ? "true" : "false")
              << "\nstack_bridge_fault_on_host_stack="
              << (fault_ok ? "true" : "false")
              << "\nstack_bridge_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
