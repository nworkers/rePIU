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


}  // namespace

// Task 759. GetAotDbtDirectEdgeDispatchThunkAddress is in
// aot_dbt_dispatch_thunks_direct.cpp and aot_dbt_dispatch_thunks_cache.cpp.

bool FindAotDbtDirectEdgeFallbackTarget(
    const ThreadContext* context,
    std::uint32_t cache_address,
    std::uint32_t* guest_target,
    std::uint32_t* guest_source)
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
            if (guest_source != nullptr)
            {
                *guest_source = site.guest_source;
            }
            return true;
        }
    }
    return false;
}

}  // namespace repiu::engine
