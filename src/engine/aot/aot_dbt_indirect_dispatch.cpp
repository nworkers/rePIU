#include "aot_dbt_indirect_dispatch.h"

#include "aot_dbt_call_step_probe.h"
#include "aot_runtime_dispatch.h"

#include <cstddef>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/fault_handler.h"
#include "repiu/platform/thunk_calling_convention.h"

namespace repiu::engine
{
namespace
{

// The `FF /2` / `FF /4` host-dispatch miss tail pushes three metadata slots
// (return address, miss address, guest source) before the thunk's pushfd/pushad,
// so the saved-frame layout matches the return dispatcher except for one extra
// slot. Frame indices are in dword units from the pushad base.
constexpr std::size_t kSavedEspIndex = 3U;
constexpr std::size_t kSavedEflagsIndex = 8U;
constexpr std::size_t kGuestSourceIndex = 9U;
constexpr std::size_t kMissAddressIndex = 10U;
// popfd(1) + push*3(15) + jmp(5) = 21 bytes from the miss tail to the fallback
// continuation, mirrored by the assembly fail-safe below.
constexpr std::uint32_t kGuestMetadataBytes = 12U;
constexpr std::uint32_t kFallbackFromMissBytes = 21U;

bool FindDispatchSite(
    const ThreadContext* context,
    std::uint32_t miss_address,
    runtime::AotDbtIndirectDispatchSite* result)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        result == nullptr ||
        miss_address < context->aot_placement->base_address)
    {
        return false;
    }
    const std::uint32_t offset =
        miss_address - context->aot_placement->base_address;
    for (const runtime::AotDbtIndirectDispatchSite& site :
         context->aot_placement->dbt_indirect_dispatch_sites)
    {
        if (site.miss_cache_offset == offset)
        {
            *result = site;
            return true;
        }
    }
    return false;
}

extern "C" void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtIndirectMissFrame(
    ThreadContext* context, std::uint32_t* frame)
{
    if (frame == nullptr)
    {
        return;
    }
    if (context != nullptr)
    {
        context->aot_dbt_indirect_entry_count.fetch_add(
            1U, std::memory_order_relaxed);
    }
    const std::uint32_t guest_source = frame[kGuestSourceIndex];
    const std::uint32_t miss_address = frame[kMissAddressIndex];
    runtime::AotDbtIndirectDispatchSite site;
    if (!FindDispatchSite(context, miss_address, &site) ||
        site.guest_source != guest_source)
    {
        frame[kGuestSourceIndex] = miss_address + kFallbackFromMissBytes;
        RecordAotDbtIndirectFallback(
            context, AotDbtDispatchFallbackReason::kInvalidSite);
        return;
    }

    const std::uint32_t cache_base = context->aot_placement->base_address;
    frame[kGuestSourceIndex] = cache_base + site.fallback_cache_offset;

    // The baked success continuation (`C3` for a call, `C2 04 00` for a jump)
    // assumes the guest instruction's kind still matches the site. If a self
    // modification changed the guest `FF /digit`, the call-path return-address
    // write could otherwise land on a frame slot, so fail closed here.
    const auto* guest_bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(guest_source));
    const std::uint8_t expected_operation = site.is_call ? 2U : 4U;
    if (guest_bytes[0] != 0xFFU ||
        ((guest_bytes[1] >> 3U) & 0x07U) != expected_operation)
    {
        RecordAotDbtIndirectFallback(
            context, AotDbtDispatchFallbackReason::kInvalidInstruction);
        return;
    }

    repiu::platform::GuestCpuContext guest_context{};
    guest_context.Edi = frame[0];
    guest_context.Esi = frame[1];
    guest_context.Ebp = frame[2];
    guest_context.Esp = frame[kSavedEspIndex] + 4U + kGuestMetadataBytes;
    guest_context.Ebx = frame[4];
    guest_context.Edx = frame[5];
    guest_context.Ecx = frame[6];
    guest_context.Eax = frame[7];
    guest_context.EFlags = frame[kSavedEflagsIndex];
    guest_context.Eip = guest_source;

    repiu::platform::FaultEvent fault;
    fault.kind = repiu::platform::FaultKind::kBreakpoint;
    fault.instruction_address = miss_address;
    fault.registers = &guest_context;
    context->aot_reentry_cache_address = miss_address;
    context->aot_reentry_pending = true;
    AotDbtDispatchFallbackReason fallback_reason =
        AotDbtDispatchFallbackReason::kUnknown;
    if (!HandleAotIndirectTransfer(
            fault, context, &fallback_reason,
            Win32AotTransferOrigin::kHost))
    {
        RecordAotDbtIndirectFallback(context, fallback_reason);
        return;
    }

    frame[kSavedEflagsIndex] = guest_context.EFlags;
    if (site.is_call && context->aot_call_depth != 0U)
    {
        const ThreadContext::AotCallFrame& call_frame =
            context->aot_call_frames[context->aot_call_depth - 1U];
        MaybeArmAotDbtCallStepProbe(
            context, Win32AotTransferOrigin::kHost, site,
            call_frame.trace_sequence,
            call_frame.target,
            static_cast<std::uint32_t>(guest_context.Eip),
            call_frame.fallthrough, call_frame.entry_esp,
            &frame[kSavedEflagsIndex]);
    }
    frame[kGuestSourceIndex] = cache_base + site.success_cache_offset;
    // The resolved cache target becomes the return-slot the success `ret`
    // (`C3` / `C2 04 00`) pops after the thunk transfers control.
    frame[kMissAddressIndex] = guest_context.Eip;
    context->aot_dbt_indirect_success_count.fetch_add(
        1, std::memory_order_relaxed);
}

#if defined(_MSC_VER) && defined(_M_IX86)
extern "C" __declspec(naked) void AotDbtIndirectMissThunk()
{
    __asm
    {
        pushfd
        pushad
        mov esi, esp
        mov ecx, dword ptr [g_repiu_active_thread_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_dbt_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_dbt_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_dbt_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        // The C++ resolver clobbers x87/MMX/SSE state that the guest may hold
        // live across this indirect call (Glide init is FP-heavy). The VEH path
        // preserves it through the OS exception context; reproduce that here by
        // saving and restoring it around the call. edi survives the stdcall.
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call ResolveAotDbtIndirectMissFrame
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_dbt_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_dbt_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        mov eax, dword ptr [esp + 40]
        add eax, 21
        mov dword ptr [esp + 36], eax
        popad
        popfd
        ret
    }
}
#endif

#if !defined(_MSC_VER) && defined(__i386__)
// Task 503d-12: the same thunk on Linux, one instantiation of the shared
// bridge macro in src/platform/linux/aot_dbt_dispatch_thunks.S. GCC has no
// naked functions on x86, so only the declaration is here.
extern "C" void AotDbtIndirectMissThunk();
#endif

}  // namespace

void RecordAotDbtIndirectFallback(
    ThreadContext* context,
    AotDbtDispatchFallbackReason reason)
{
    if (context == nullptr)
    {
        return;
    }
    std::uint32_t index = static_cast<std::uint32_t>(reason);
    if (index >= kAotDbtDispatchFallbackReasonCount)
    {
        index = static_cast<std::uint32_t>(
            AotDbtDispatchFallbackReason::kUnknown);
    }
    context->aot_dbt_indirect_fallback_count.fetch_add(
        1U, std::memory_order_relaxed);
    context->aot_dbt_indirect_fallback_reason_counts[index].fetch_add(
        1U, std::memory_order_relaxed);
}

void* GetAotDbtIndirectMissThunkAddress()
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    return reinterpret_cast<void*>(&AotDbtIndirectMissThunk);
#else
    return nullptr;
#endif
}

}  // namespace repiu::engine
