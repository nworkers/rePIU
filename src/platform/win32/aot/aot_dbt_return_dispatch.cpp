#include "aot_dbt_return_dispatch.h"

#include "aot_runtime_dispatch.h"

#include "repiu/platform/win32/aot_return_dispatch_site_index.h"
#include "repiu/platform/win32/aot_return_stage_profile.h"

#include <cstddef>

namespace repiu::platform::win32
{
namespace
{

constexpr std::size_t kSavedEspIndex = 3U;
constexpr std::size_t kSavedEflagsIndex = 8U;
constexpr std::size_t kGuestSourceIndex = 9U;
constexpr std::size_t kMissAddressIndex = 10U;
constexpr std::uint32_t kGuestMetadataBytes = 8U;
constexpr std::uint32_t kFallbackFromMissBytes = 16U;

bool FindDispatchSite(
    const ThreadContext* context,
    std::uint32_t miss_address,
    runtime::AotDbtReturnDispatchSite* result,
    std::uint32_t* result_index)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        result == nullptr || result_index == nullptr ||
        miss_address < context->aot_placement->base_address)
    {
        return false;
    }
    Win32AotCodeCachePlacement* placement = context->aot_placement;
    const std::uint32_t offset =
        miss_address - context->aot_placement->base_address;
    EnsureAotReturnDispatchSiteIndex(placement);
    const AotReturnDispatchSiteLookup indexed =
        LookupAotReturnDispatchSiteIndex(*placement, offset);
    if (indexed.usable)
    {
        if (!indexed.found)
        {
            ++placement->return_dispatch_site_index.lookup_count;
            return false;
        }
        if (indexed.site_index < placement->dbt_return_dispatch_sites.size())
        {
            const runtime::AotDbtReturnDispatchSite& site =
                placement->dbt_return_dispatch_sites[indexed.site_index];
            if (site.miss_cache_offset == offset)
            {
                *result = site;
                *result_index = indexed.site_index;
                ++placement->return_dispatch_site_index.lookup_count;
                return true;
            }
        }
    }
    ++placement->return_dispatch_site_index.fallback_scan_count;
    for (std::size_t index = 0;
         index < placement->dbt_return_dispatch_sites.size(); ++index)
    {
        const runtime::AotDbtReturnDispatchSite& site =
            placement->dbt_return_dispatch_sites[index];
        if (site.miss_cache_offset == offset)
        {
            *result = site;
            *result_index = static_cast<std::uint32_t>(index);
            return true;
        }
    }
    return false;
}

extern "C" void __stdcall ResolveAotDbtReturnMissFrame(
    ThreadContext* context, std::uint32_t* frame)
{
    if (frame == nullptr)
    {
        return;
    }
    Win32AotReturnStageProfile* stage_profile =
        context != nullptr ? &context->aot_return_stage_profile : nullptr;
    // Task 482: the adapter is the outer window of a DBT-path return, so the
    // dispatch-site lookup and the frame marshalling below are attributed
    // rather than left in the resolver's residual.
    const AotReturnOuterScope outer_stage(stage_profile);
    AotReturnStageScope entry_stage(stage_profile,
                                    AotReturnStage::kEntryValidation);
    if (context != nullptr)
    {
        context->aot_dbt_return_entry_count.fetch_add(
            1U, std::memory_order_relaxed);
    }
    const std::uint32_t guest_source = frame[kGuestSourceIndex];
    const std::uint32_t miss_address = frame[kMissAddressIndex];
    runtime::AotDbtReturnDispatchSite site;
    std::uint32_t site_index = 0;
    if (!FindDispatchSite(context, miss_address, &site, &site_index) ||
        site.guest_source != guest_source)
    {
        frame[kGuestSourceIndex] = miss_address + kFallbackFromMissBytes;
        RecordAotDbtReturnFallback(
            context, AotDbtDispatchFallbackReason::kInvalidSite);
        return;
    }

    const std::uint32_t cache_base = context->aot_placement->base_address;
    frame[kGuestSourceIndex] = cache_base + site.fallback_cache_offset;

    CONTEXT guest_context{};
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

    EXCEPTION_RECORD exception_record{};
    exception_record.ExceptionCode = EXCEPTION_BREAKPOINT;
    exception_record.ExceptionAddress = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(miss_address));
    EXCEPTION_POINTERS exception_info{&exception_record, &guest_context};
    context->aot_reentry_cache_address = miss_address;
    context->aot_reentry_pending = true;
    AotDbtDispatchFallbackReason fallback_reason =
        AotDbtDispatchFallbackReason::kUnknown;
    entry_stage.Close();
    if (!HandleAotReturnTransfer(
            &exception_info, &guest_context, context, &fallback_reason,
            Win32AotTransferOrigin::kHost, site_index))
    {
        RecordAotDbtReturnFallback(context, fallback_reason);
        return;
    }

    const AotReturnStageScope continuation_stage(
        stage_profile, AotReturnStage::kContinuation);
    frame[kSavedEflagsIndex] = guest_context.EFlags;
    frame[kGuestSourceIndex] = cache_base + site.success_cache_offset;
    frame[kMissAddressIndex] = guest_context.Eip;
    context->aot_dbt_return_success_count.fetch_add(
        1, std::memory_order_relaxed);
}

#if defined(_MSC_VER) && defined(_M_IX86)
extern "C" __declspec(naked) void AotDbtReturnMissThunk()
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
        push esi
        push ecx
        call ResolveAotDbtReturnMissFrame

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
        add eax, 16
        mov dword ptr [esp + 36], eax
        popad
        popfd
        ret
    }
}
#endif

}  // namespace

void RecordAotDbtReturnFallback(
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
    context->aot_dbt_return_fallback_count.fetch_add(
        1U, std::memory_order_relaxed);
    context->aot_dbt_return_fallback_reason_counts[index].fetch_add(
        1U, std::memory_order_relaxed);
}

void* GetAotDbtReturnMissThunkAddress()
{
#if defined(_MSC_VER) && defined(_M_IX86)
    return reinterpret_cast<void*>(&AotDbtReturnMissThunk);
#else
    return nullptr;
#endif
}

}  // namespace repiu::platform::win32
