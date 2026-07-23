#include "aot_dbt_return_dispatch.h"

#include "aot_runtime_dispatch.h"

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

const runtime::AotDbtReturnDispatchSite* FindDispatchSite(
    const ThreadContext* context, std::uint32_t miss_address)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        miss_address < context->aot_placement->base_address)
    {
        return nullptr;
    }
    const std::uint32_t offset =
        miss_address - context->aot_placement->base_address;
    for (const runtime::AotDbtReturnDispatchSite& site :
         context->aot_placement->dbt_return_dispatch_sites)
    {
        if (site.miss_cache_offset == offset)
        {
            return &site;
        }
    }
    return nullptr;
}

extern "C" void __stdcall ResolveAotDbtReturnMissFrame(
    ThreadContext* context, std::uint32_t* frame)
{
    if (frame == nullptr)
    {
        return;
    }
    const std::uint32_t guest_source = frame[kGuestSourceIndex];
    const std::uint32_t miss_address = frame[kMissAddressIndex];
    const runtime::AotDbtReturnDispatchSite* site =
        FindDispatchSite(context, miss_address);
    if (site == nullptr || site->guest_source != guest_source)
    {
        frame[kGuestSourceIndex] = miss_address + kFallbackFromMissBytes;
        return;
    }

    const std::uint32_t cache_base = context->aot_placement->base_address;
    frame[kGuestSourceIndex] = cache_base + site->fallback_cache_offset;
    context->aot_dbt_return_attempt_count.fetch_add(
        1, std::memory_order_relaxed);

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
    if (!HandleAotReturnTransfer(&exception_info, &guest_context, context))
    {
        context->aot_dbt_return_fallback_count.fetch_add(
            1, std::memory_order_relaxed);
        return;
    }

    frame[kSavedEflagsIndex] = guest_context.EFlags;
    frame[kGuestSourceIndex] = cache_base + site->success_cache_offset;
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

void* GetAotDbtReturnMissThunkAddress()
{
#if defined(_MSC_VER) && defined(_M_IX86)
    return reinterpret_cast<void*>(&AotDbtReturnMissThunk);
#else
    return nullptr;
#endif
}

}  // namespace repiu::platform::win32
