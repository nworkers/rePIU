#include "aot_dbt_direct_edge_dispatch.h"

#include "aot_runtime_dispatch.h"
#include "../execution/thread_context.h"

#include <cstddef>
#include "repiu/platform/thunk_calling_convention.h"

namespace repiu::engine
{
namespace
{

constexpr std::size_t kGuestTargetIndex = 9U;
constexpr std::size_t kDispatchAddressIndex = 10U;
constexpr std::uint32_t kFallbackFromDispatchBytes = 15U;

bool FindDispatchSite(
    const ThreadContext* context,
    std::uint32_t dispatch_address,
    runtime::AotDbtDirectEdgeDispatchSite* result)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        result == nullptr ||
        dispatch_address < context->aot_placement->base_address)
    {
        return false;
    }
    const std::uint32_t offset =
        dispatch_address - context->aot_placement->base_address;
    for (const runtime::AotDbtDirectEdgeDispatchSite& site :
         context->aot_placement->dbt_direct_edge_dispatch_sites)
    {
        if (site.dispatch_cache_offset == offset)
        {
            *result = site;
            return true;
        }
    }
    return false;
}

extern "C" void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtDirectEdgeFrame(
    ThreadContext* context, std::uint32_t* frame)
{
    if (frame == nullptr)
    {
        return;
    }
    const std::uint32_t guest_target = frame[kGuestTargetIndex];
    const std::uint32_t dispatch_address = frame[kDispatchAddressIndex];
    runtime::AotDbtDirectEdgeDispatchSite site;
    if (!FindDispatchSite(context, dispatch_address, &site) ||
        site.guest_target != guest_target)
    {
        frame[kGuestTargetIndex] =
            dispatch_address + kFallbackFromDispatchBytes;
        return;
    }

    const std::uint32_t cache_base = context->aot_placement->base_address;
    frame[kGuestTargetIndex] = cache_base + site.fallback_cache_offset;
    std::uint32_t cache_target = 0U;
    if (!ResolveAotTransferTarget(context, guest_target, &cache_target))
    {
        return;
    }
    frame[kGuestTargetIndex] = cache_base + site.success_cache_offset;
    frame[kDispatchAddressIndex] = cache_target;
}

#if defined(_MSC_VER) && defined(_M_IX86)
extern "C" __declspec(naked) void AotDbtDirectEdgeDispatchThunk()
{
    __asm
    {
        pushfd
        pushad
        cld
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
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call ResolveAotDbtDirectEdgeFrame
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
        add eax, 15
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
extern "C" void AotDbtDirectEdgeDispatchThunk();
#endif

}  // namespace

void* GetAotDbtDirectEdgeDispatchThunkAddress()
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    return reinterpret_cast<void*>(&AotDbtDirectEdgeDispatchThunk);
#else
    return nullptr;
#endif
}

bool FindAotDbtDirectEdgeFallbackTarget(
    const ThreadContext* context,
    std::uint32_t cache_address,
    std::uint32_t* guest_target)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        guest_target == nullptr ||
        cache_address < context->aot_placement->base_address)
    {
        return false;
    }
    const std::uint32_t offset =
        cache_address - context->aot_placement->base_address;
    for (const runtime::AotDbtDirectEdgeDispatchSite& site :
         context->aot_placement->dbt_direct_edge_dispatch_sites)
    {
        if (site.fallback_cache_offset == offset)
        {
            *guest_target = site.guest_target;
            return true;
        }
    }
    return false;
}

}  // namespace repiu::engine
