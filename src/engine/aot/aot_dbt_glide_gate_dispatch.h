#pragma once

#include "repiu/hle/glide_hle.h"

#include <cstdint>

namespace repiu::engine
{

struct ThreadContext;

struct Win32GlideGateDirectDispatchStats
{
    std::uint32_t patched_gate_count = 0;
    std::uint32_t verified_gate_count = 0;
    std::uint32_t resolved_target_count = 0;
    std::uint32_t relinked_cache_target_count = 0;
    std::uint32_t entry_count = 0;
    std::uint32_t success_count = 0;
    std::uint32_t target_miss_count = 0;
    std::uint32_t terminal_failure_count = 0;
    // Task 519: `relinked_cache_target_count` split by how the slot was found.
    // Content slots are matched against what the cache currently holds, so one
    // collected twice for the same site says the earlier write did not stick.
    // Fixup slots come from the static list and are rewritten every time
    // regardless, which is why the combined total cannot answer that.
    std::uint32_t relink_content_patch_count = 0;
    std::uint32_t relink_fixup_patch_count = 0;
};

bool ResolveWin32GlideGateDirectDispatchEnabled(const char* setting);

bool ResolveWin32GlideGateDirectTarget(
    const ThreadContext* context,
    std::uint32_t target,
    std::uint32_t* direct_target);

bool ActivateWin32GlideGateDirectTarget(
    ThreadContext* context,
    std::uint32_t cache_boundary_address,
    std::uint32_t gate_address);

bool PatchWin32GlideGatePlanForDirectDispatch(
    std::uint32_t gate_code_base,
    repiu::hle::GlideGatePlan* plan);

bool VerifyWin32GlideGateDirectDispatchImage(
    std::uint32_t gate_code_base,
    const repiu::hle::GlideGatePlan& plan);

Win32GlideGateDirectDispatchStats
ReadWin32GlideGateDirectDispatchStats();

void* GetWin32GlideGateDirectDispatchThunkAddress();

}  // namespace repiu::engine
