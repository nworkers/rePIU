#include "repiu/platform/win32/aot_return_patch_policy.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"

#include <algorithm>
#include <limits>

namespace repiu::platform::win32
{
namespace
{

void IncrementSaturating(std::uint32_t* value)
{
    if (*value != std::numeric_limits<std::uint32_t>::max())
    {
        ++*value;
    }
}

}  // namespace

void SyncAotReturnPatchPolicy(Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    placement->return_patch_policy.sites.resize(
        placement->dbt_return_dispatch_sites.size());
}

AotReturnPatchAction ObserveAotReturnPatchMiss(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t site_index,
    std::uint32_t guest_target)
{
    if (placement == nullptr ||
        site_index >= placement->dbt_return_dispatch_sites.size() ||
        placement->return_patch_policy.sites.size() !=
            placement->dbt_return_dispatch_sites.size())
    {
        return AotReturnPatchAction::kPatch;
    }

    Win32AotReturnPatchPolicy& policy = placement->return_patch_policy;
    Win32AotReturnPatchSiteState& state = policy.sites[site_index];
    IncrementSaturating(&policy.observation_count);
    IncrementSaturating(&state.miss_count);

    if (!state.megamorphic)
    {
        const auto end = state.targets.begin() + state.target_count;
        if (std::find(state.targets.begin(), end, guest_target) == end &&
            state.target_count < state.targets.size())
        {
            state.targets[state.target_count++] = guest_target;
        }
        if (state.miss_count >= kAotReturnMegamorphicMissThreshold &&
            state.target_count >= kAotReturnMegamorphicTargetCapacity)
        {
            state.megamorphic = true;
            IncrementSaturating(&policy.megamorphic_site_count);
        }
    }

    if (!state.megamorphic)
    {
        return AotReturnPatchAction::kPatch;
    }
    IncrementSaturating(&policy.bypass_count);
    IncrementSaturating(&state.bypass_count);
    return AotReturnPatchAction::kBypass;
}

}  // namespace repiu::platform::win32
