#include "direct_edge_dispatch_probe.h"

#include <iostream>

#if defined(_WIN32)
#include "repiu/engine/aot_code_cache_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "../../engine/aot/aot_dbt_direct_edge_dispatch.h"
#include "../../engine/execution/thread_context.h"

#include <cstdint>
#include <cstring>
#include <memory>
#endif

namespace repiu::tools
{
#if defined(_WIN32)
namespace
{

constexpr std::uint32_t kGuestSource = 0x00121000U;
constexpr std::uint32_t kGuestTarget = kGuestSource + 1U;

std::uint32_t ReadUint32(const std::uint8_t* bytes, std::uint32_t offset)
{
    std::uint32_t value = 0U;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

std::int32_t ReadInt32(const std::uint8_t* bytes, std::uint32_t offset)
{
    std::int32_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

runtime::AotTranslationPlan MakePlan(bool map_target)
{
    runtime::AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestSource;
    runtime::AotBasicBlock source;
    source.guest_address = kGuestSource;
    runtime::AotInstructionRecord copy;
    copy.guest_address = kGuestSource;
    copy.kind = runtime::AotInstructionKind::kCopy;
    copy.length = 1U;
    copy.bytes = {0x90U};
    source.instructions.push_back(copy);
    plan.blocks.push_back(source);
    if (map_target)
    {
        runtime::AotBasicBlock target;
        target.guest_address = kGuestTarget;
        runtime::AotInstructionRecord stop;
        stop.guest_address = kGuestTarget;
        stop.kind = runtime::AotInstructionKind::kReturn;
        stop.length = 1U;
        stop.bytes = {0xC3U};
        target.instructions.push_back(stop);
        plan.blocks.push_back(target);
    }
    return plan;
}

bool ValidateImage(const runtime::AotCodeCacheImage& image)
{
    if (!image.valid || image.dbt_direct_edge_dispatch_sites.size() != 1U ||
        image.fixups.size() != 1U || !image.fixups[0].resolved)
    {
        return false;
    }
    const runtime::AotDbtDirectEdgeDispatchSite& site =
        image.dbt_direct_edge_dispatch_sites[0];
    const runtime::AotCodeCacheFixup& fixup = image.fixups[0];
    if (site.guest_source != kGuestSource ||
        site.guest_target != kGuestTarget ||
        site.dispatch_cache_offset + 21U > image.bytes.size() ||
        image.bytes[site.dispatch_cache_offset] != 0x68U ||
        image.bytes[site.dispatch_cache_offset + 5U] != 0x68U ||
        image.bytes[site.dispatch_cache_offset + 10U] != 0xE9U ||
        image.bytes[site.fallback_cache_offset] != 0xCCU ||
        image.bytes[site.success_cache_offset] != 0xC3U)
    {
        return false;
    }
    const std::uint32_t patched_target = fixup.cache_patch_offset + 4U +
        static_cast<std::uint32_t>(
            ReadInt32(image.bytes.data(), fixup.cache_patch_offset));
    return patched_target == site.dispatch_cache_offset;
}

bool ValidatePlacement(
    const engine::Win32AotCodeCachePlacement& placement)
{
    if (!placement.placed ||
        placement.dbt_direct_edge_dispatch_sites.size() != 1U)
    {
        return false;
    }
    const runtime::AotDbtDirectEdgeDispatchSite& site =
        placement.dbt_direct_edge_dispatch_sites[0];
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(placement.base_address));
    const std::uint32_t dispatch =
        placement.base_address + site.dispatch_cache_offset;
    const std::uint32_t next = placement.base_address +
        site.thunk_displacement_offset + 4U;
    const std::uint32_t resolved_thunk = next +
        static_cast<std::uint32_t>(
            ReadInt32(bytes, site.thunk_displacement_offset));
    const std::uint32_t thunk = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(
            engine::GetAotDbtDirectEdgeDispatchThunkAddress()));
    return ReadUint32(bytes, site.dispatch_address_immediate_offset) ==
               dispatch &&
        resolved_thunk == thunk;
}

}  // namespace
#endif

bool RunAotDbtDirectEdgeDispatchProbe()
{
#if !defined(_WIN32)
    std::cout << "dbt_direct_edge_dispatch_probe_skipped=true\n";
    return true;
#else
    runtime::AotCodeCacheImage disabled;
    const bool disabled_rejects =
        !runtime::BuildAotCodeCacheImage(MakePlan(false), &disabled) &&
        disabled.message ==
            "direct control-flow target is outside the cache";

    runtime::AotCodeCacheBuildOptions options;
    options.enable_dbt_direct_edge_dispatch = true;
    runtime::AotCodeCacheImage image;
    const bool emitted = runtime::BuildAotCodeCacheImage(
        MakePlan(false), options, &image) && ValidateImage(image);

    runtime::AotCodeCacheImage mapped;
    const bool mapped_stays_direct = runtime::BuildAotCodeCacheImage(
        MakePlan(true), options, &mapped) &&
        mapped.dbt_direct_edge_dispatch_sites.empty();

    engine::Win32AotCodeCachePlacement placement;
    const bool placed = emitted &&
        engine::PlaceWin32AotCodeCache(image, &placement) &&
        placement.placed && placement.dbt_direct_edge_dispatch_enabled &&
        ValidatePlacement(placement);

    auto context = std::make_unique<engine::ThreadContext>();
    context->aot_placement = &placement;
    std::uint32_t fallback_target = 0U;
    const bool fallback = placed &&
        engine::FindAotDbtDirectEdgeFallbackTarget(
            context.get(),
            placement.base_address +
                placement.dbt_direct_edge_dispatch_sites[0]
                    .fallback_cache_offset,
            &fallback_target) &&
        fallback_target == kGuestTarget;

    const bool all = disabled_rejects && emitted && mapped_stays_direct &&
        placed && fallback;
    std::cout << "dbt_direct_edge_dispatch_disabled_rejects="
              << (disabled_rejects ? "true" : "false")
              << "\ndbt_direct_edge_dispatch_emitted="
              << (emitted ? "true" : "false")
              << "\ndbt_direct_edge_dispatch_mapped_direct="
              << (mapped_stays_direct ? "true" : "false")
              << "\ndbt_direct_edge_dispatch_placement="
              << (placed ? "true" : "false")
              << "\ndbt_direct_edge_dispatch_fallback="
              << (fallback ? "true" : "false")
              << "\ndbt_direct_edge_dispatch_all="
              << (all ? "true" : "false") << "\n";

    engine::ReleaseWin32AotCodeCache(&placement);
    return all;
#endif
}

}  // namespace repiu::tools
