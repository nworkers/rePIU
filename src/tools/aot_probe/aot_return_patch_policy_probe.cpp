#include "aot_return_patch_policy_probe.h"

#include "repiu/engine/aot_code_cache.h"
#include "repiu/engine/aot_return_patch_policy.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

using repiu::engine::AotReturnPatchAction;
using repiu::engine::ObserveAotReturnPatchMiss;
using repiu::engine::SyncAotReturnPatchPolicy;
using repiu::engine::AotCodeCachePlacement;
using repiu::engine::kAotReturnMegamorphicMissThreshold;

void AddSite(AotCodeCachePlacement* placement,
             std::uint32_t guest_source,
             std::uint32_t miss_offset)
{
    repiu::runtime::AotDbtReturnDispatchSite site;
    site.guest_source = guest_source;
    site.miss_cache_offset = miss_offset;
    placement->dbt_return_dispatch_sites.push_back(site);
}

bool ObserveRepeatedTargets(AotCodeCachePlacement* placement,
                            std::uint32_t site_index,
                            std::uint32_t target_count,
                            std::uint32_t observations,
                            AotReturnPatchAction expected_last)
{
    AotReturnPatchAction action = AotReturnPatchAction::kPatch;
    for (std::uint32_t index = 0; index < observations; ++index)
    {
        action = ObserveAotReturnPatchMiss(
            placement, site_index,
            0x03000000U + (index % target_count) * 0x100U);
    }
    return action == expected_last;
}

}  // namespace

bool RunAotReturnPatchPolicyProbe()
{
    AotCodeCachePlacement unavailable;
    AddSite(&unavailable, 0x04010000U, 0x100U);
    const bool unavailable_ok = ObserveAotReturnPatchMiss(
        &unavailable, 0U, 0x03010000U) == AotReturnPatchAction::kPatch &&
        unavailable.return_patch_policy.observation_count == 0U;

    AotCodeCachePlacement monomorphic;
    AddSite(&monomorphic, 0x04020000U, 0x200U);
    SyncAotReturnPatchPolicy(&monomorphic);
    const bool monomorphic_ok = ObserveRepeatedTargets(
        &monomorphic, 0U, 1U, kAotReturnMegamorphicMissThreshold * 2U,
        AotReturnPatchAction::kPatch) &&
        monomorphic.return_patch_policy.megamorphic_site_count == 0U;

    AotCodeCachePlacement four_way;
    AddSite(&four_way, 0x04030000U, 0x300U);
    SyncAotReturnPatchPolicy(&four_way);
    const bool four_way_ok = ObserveRepeatedTargets(
        &four_way, 0U, 4U, kAotReturnMegamorphicMissThreshold * 2U,
        AotReturnPatchAction::kPatch) &&
        four_way.return_patch_policy.megamorphic_site_count == 0U;

    AotCodeCachePlacement megamorphic;
    AddSite(&megamorphic, 0x04040000U, 0x400U);
    AddSite(&megamorphic, 0x04050000U, 0x500U);
    SyncAotReturnPatchPolicy(&megamorphic);
    const bool before_threshold_ok = ObserveRepeatedTargets(
        &megamorphic, 0U, 8U, kAotReturnMegamorphicMissThreshold - 1U,
        AotReturnPatchAction::kPatch);
    const bool classify_ok = ObserveAotReturnPatchMiss(
        &megamorphic, 0U, 0x03000700U) == AotReturnPatchAction::kBypass &&
        megamorphic.return_patch_policy.megamorphic_site_count == 1U &&
        megamorphic.return_patch_policy.bypass_count == 1U;
    const bool persistent_ok = ObserveAotReturnPatchMiss(
        &megamorphic, 0U, 0x0300F000U) == AotReturnPatchAction::kBypass &&
        megamorphic.return_patch_policy.megamorphic_site_count == 1U &&
        megamorphic.return_patch_policy.bypass_count == 2U;
    const bool isolated_ok = ObserveAotReturnPatchMiss(
        &megamorphic, 1U, 0x03020000U) == AotReturnPatchAction::kPatch &&
        !megamorphic.return_patch_policy.sites[1].megamorphic;

    AddSite(&megamorphic, 0x04060000U, 0x600U);
    SyncAotReturnPatchPolicy(&megamorphic);
    const bool append_ok =
        megamorphic.return_patch_policy.sites.size() == 3U &&
        megamorphic.return_patch_policy.sites[0].megamorphic &&
        !megamorphic.return_patch_policy.sites[2].megamorphic &&
        ObserveAotReturnPatchMiss(
            &megamorphic, 2U, 0x03030000U) == AotReturnPatchAction::kPatch;

    const bool all = unavailable_ok && monomorphic_ok && four_way_ok &&
        before_threshold_ok && classify_ok && persistent_ok && isolated_ok &&
        append_ok;
    std::cout
        << "return_patch_policy_unavailable="
        << (unavailable_ok ? "true" : "false")
        << "\nreturn_patch_policy_monomorphic="
        << (monomorphic_ok ? "true" : "false")
        << "\nreturn_patch_policy_four_way="
        << (four_way_ok ? "true" : "false")
        << "\nreturn_patch_policy_before_threshold="
        << (before_threshold_ok ? "true" : "false")
        << "\nreturn_patch_policy_classify="
        << (classify_ok ? "true" : "false")
        << "\nreturn_patch_policy_persistent="
        << (persistent_ok ? "true" : "false")
        << "\nreturn_patch_policy_isolated="
        << (isolated_ok ? "true" : "false")
        << "\nreturn_patch_policy_append="
        << (append_ok ? "true" : "false")
        << "\nreturn_patch_policy_all=" << (all ? "true" : "false")
        << "\n";
    return all;
}

}  // namespace repiu::tools
