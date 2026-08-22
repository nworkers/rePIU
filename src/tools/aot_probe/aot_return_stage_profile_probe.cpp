#include "aot_return_stage_profile_probe.h"

#include "repiu/platform/win32/aot_return_patch_policy.h"
#include "repiu/platform/win32/aot_return_stage_profile.h"
#include "repiu/runtime/aot_code_cache.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::platform::win32::AotReturnStage;
using repiu::platform::win32::AotReturnStageCoveredCycles;
using repiu::platform::win32::AotReturnStageProfileEnabled;
using repiu::platform::win32::AotReturnStageScope;
using repiu::platform::win32::RankAotReturnStageSites;
using repiu::platform::win32::RecordAotReturnOuterSample;
using repiu::platform::win32::RecordAotReturnStageSample;
using repiu::platform::win32::ResolveAotReturnStageProfileEnabled;
using repiu::platform::win32::SnapshotAotReturnStageProfile;
using repiu::platform::win32::Win32AotReturnPatchPolicy;
using repiu::platform::win32::Win32AotReturnStageProfile;
using repiu::platform::win32::Win32AotReturnStageSiteObservation;
using repiu::platform::win32::kAotReturnStageCount;
using repiu::platform::win32::kAotReturnStageSiteReportCapacity;

std::size_t StageIndex(const AotReturnStage stage)
{
    return static_cast<std::size_t>(stage);
}

// Records the five stages of one return with the given per-stage cost starting
// at `base`, then closes an outer window `outer` cycles wide around them.
void RecordOneReturn(Win32AotReturnStageProfile* profile,
                     const std::uint64_t base,
                     const std::uint64_t per_stage,
                     const std::uint64_t outer)
{
    const std::uint64_t stage_total_at_entry = profile->stage_total_cycles;
    std::uint64_t cursor = base;
    for (std::uint32_t stage = 0; stage < kAotReturnStageCount; ++stage)
    {
        RecordAotReturnStageSample(profile,
                                   static_cast<AotReturnStage>(stage), cursor,
                                   cursor + per_stage);
        cursor += per_stage;
    }
    RecordAotReturnOuterSample(profile, base, base + outer,
                               stage_total_at_entry);
}

void AddSite(std::vector<repiu::runtime::AotDbtReturnDispatchSite>* sites,
             const std::uint32_t guest_source,
             const std::uint32_t miss_offset)
{
    repiu::runtime::AotDbtReturnDispatchSite site;
    site.guest_source = guest_source;
    site.miss_cache_offset = miss_offset;
    sites->push_back(site);
}

bool ProbeToggle()
{
    return !ResolveAotReturnStageProfileEnabled(nullptr) &&
        !ResolveAotReturnStageProfileEnabled("") &&
        !ResolveAotReturnStageProfileEnabled("0") &&
        !ResolveAotReturnStageProfileEnabled("off") &&
        !ResolveAotReturnStageProfileEnabled("false") &&
        !ResolveAotReturnStageProfileEnabled("yes") &&
        !ResolveAotReturnStageProfileEnabled("ON") &&
        ResolveAotReturnStageProfileEnabled("1") &&
        ResolveAotReturnStageProfileEnabled("on") &&
        ResolveAotReturnStageProfileEnabled("true");
}

bool ProbeStageAccounting()
{
    Win32AotReturnStageProfile profile;
    RecordOneReturn(&profile, 1000U, 10U, 100U);
    RecordOneReturn(&profile, 2000U, 20U, 200U);
    bool ok = profile.enabled && profile.outer_count == 2U &&
        profile.outer_cycles == 300U && profile.max_outer_cycles == 200U &&
        profile.stage_total_cycles == 150U;
    for (std::uint32_t stage = 0; stage < kAotReturnStageCount; ++stage)
    {
        ok = ok && profile.counts[stage] == 2U &&
            profile.cycles[stage] == 30U && profile.max_cycles[stage] == 20U;
    }
    // Coverage is the stage sum; the residual is what the two windows did not
    // explain: (100 - 50) + (200 - 100).
    const auto snapshot = SnapshotAotReturnStageProfile(profile);
    return ok && AotReturnStageCoveredCycles(snapshot) == 150U &&
        profile.residual_cycles == 150U &&
        profile.residual_clamp_count == 0U &&
        profile.clamped_sample_count == 0U;
}

bool ProbeSnapshotCopy()
{
    Win32AotReturnStageProfile profile;
    RecordOneReturn(&profile, 500U, 7U, 90U);
    const auto snapshot = SnapshotAotReturnStageProfile(profile);
    bool ok = snapshot.enabled == profile.enabled &&
        snapshot.outer_cycles == profile.outer_cycles &&
        snapshot.outer_count == profile.outer_count &&
        snapshot.max_outer_cycles == profile.max_outer_cycles &&
        snapshot.residual_cycles == profile.residual_cycles &&
        snapshot.stage_total_cycles == profile.stage_total_cycles &&
        snapshot.residual_clamp_count == profile.residual_clamp_count &&
        snapshot.clamped_sample_count == profile.clamped_sample_count;
    for (std::uint32_t stage = 0; stage < kAotReturnStageCount; ++stage)
    {
        ok = ok && snapshot.cycles[stage] == profile.cycles[stage] &&
            snapshot.counts[stage] == profile.counts[stage] &&
            snapshot.max_cycles[stage] == profile.max_cycles[stage];
    }
    const Win32AotReturnStageProfile empty;
    const auto empty_snapshot = SnapshotAotReturnStageProfile(empty);
    return ok && !empty_snapshot.enabled &&
        AotReturnStageCoveredCycles(empty_snapshot) == 0U &&
        empty_snapshot.outer_count == 0U;
}

bool ProbeResidualClamp()
{
    Win32AotReturnStageProfile profile;
    // A window narrower than the stages recorded inside it must not underflow
    // the residual. It is counted instead.
    RecordOneReturn(&profile, 100U, 40U, 50U);
    const bool clamped = profile.residual_cycles == 0U &&
        profile.residual_clamp_count == 1U && profile.outer_count == 1U &&
        profile.outer_cycles == 50U;
    // An exactly covered window leaves no residual and is not a clamp.
    RecordOneReturn(&profile, 1000U, 10U, 50U);
    return clamped && profile.residual_cycles == 0U &&
        profile.residual_clamp_count == 1U && profile.outer_count == 2U;
}

bool ProbeSampleClamp()
{
    Win32AotReturnStageProfile profile;
    RecordAotReturnStageSample(&profile, AotReturnStage::kTargetRead, 900U,
                               800U);
    const bool stage_ok =
        profile.clamped_sample_count == 1U &&
        profile.counts[StageIndex(AotReturnStage::kTargetRead)] == 1U &&
        profile.cycles[StageIndex(AotReturnStage::kTargetRead)] == 0U &&
        profile.stage_total_cycles == 0U;
    RecordAotReturnOuterSample(&profile, 900U, 800U, 0U);
    const bool outer_ok = profile.clamped_sample_count == 2U &&
        profile.outer_count == 1U && profile.outer_cycles == 0U &&
        profile.residual_cycles == 0U;
    // A null profile is the disabled path and must stay inert.
    RecordAotReturnStageSample(nullptr, AotReturnStage::kContinuation, 0U, 1U);
    RecordAotReturnOuterSample(nullptr, 0U, 1U, 0U);
    return stage_ok && outer_ok;
}

bool ProbeDisabledScope()
{
    Win32AotReturnStageProfile profile;
    {
        const AotReturnStageScope scope(&profile,
                                        AotReturnStage::kPatchPolicy);
    }
    const bool recorded =
        profile.counts[StageIndex(AotReturnStage::kPatchPolicy)] != 0U;
    // The scope reads the gate rather than the caller: with the environment
    // unset it leaves the profile untouched, and with it set it records.
    return recorded == AotReturnStageProfileEnabled();
}

bool ProbeEmptyCensus()
{
    Win32AotReturnPatchPolicy policy;
    std::vector<repiu::runtime::AotDbtReturnDispatchSite> sites;
    std::vector<Win32AotReturnStageSiteObservation> observations;
    observations.push_back({});
    RankAotReturnStageSites(policy, sites, &observations);
    const bool empty_ok = observations.empty();

    // A site that never missed is not an observation.
    AddSite(&sites, 0x04010000U, 0x100U);
    policy.sites.resize(1U);
    RankAotReturnStageSites(policy, sites, &observations);
    const bool silent_ok = observations.empty();

    // A policy shorter than the plan is read only to the shorter of the two.
    AddSite(&sites, 0x04020000U, 0x200U);
    policy.sites[0].miss_count = 3U;
    RankAotReturnStageSites(policy, sites, &observations);
    RankAotReturnStageSites(policy, sites, nullptr);
    return empty_ok && silent_ok && observations.size() == 1U &&
        observations[0].guest_source == 0x04010000U;
}

bool ProbeTopNOrdering()
{
    Win32AotReturnPatchPolicy policy;
    std::vector<repiu::runtime::AotDbtReturnDispatchSite> sites;
    constexpr std::uint32_t kSiteCount = 20U;
    policy.sites.resize(kSiteCount);
    for (std::uint32_t index = 0; index < kSiteCount; ++index)
    {
        AddSite(&sites, 0x04000000U + index * 0x100U, 0x1000U + index * 0x10U);
        policy.sites[index].miss_count = kSiteCount - index;
        policy.sites[index].target_count = index % 8U;
        policy.sites[index].bypass_count = index;
        policy.sites[index].megamorphic = index % 2U == 0U;
    }
    std::vector<Win32AotReturnStageSiteObservation> observations;
    RankAotReturnStageSites(policy, sites, &observations);
    bool ok = observations.size() == kAotReturnStageSiteReportCapacity;
    for (std::size_t index = 0; ok && index < observations.size(); ++index)
    {
        const auto& observation = observations[index];
        const auto expected = static_cast<std::uint32_t>(index);
        ok = observation.site_index == expected &&
            observation.observation_count == kSiteCount - expected &&
            observation.guest_source == 0x04000000U + expected * 0x100U &&
            observation.miss_cache_offset == 0x1000U + expected * 0x10U &&
            observation.bypass_count == expected &&
            observation.distinct_target_count == expected % 8U &&
            observation.megamorphic == (expected % 2U == 0U);
        if (index != 0U)
        {
            ok = ok && observations[index - 1U].observation_count >=
                observation.observation_count;
        }
    }

    // Equal observation counts rank by bypasses first and site index last.
    Win32AotReturnPatchPolicy tie_policy;
    std::vector<repiu::runtime::AotDbtReturnDispatchSite> tie_sites;
    tie_policy.sites.resize(3U);
    for (std::uint32_t index = 0; index < 3U; ++index)
    {
        AddSite(&tie_sites, 0x04100000U + index, index);
        tie_policy.sites[index].miss_count = 5U;
    }
    tie_policy.sites[2].bypass_count = 4U;
    std::vector<Win32AotReturnStageSiteObservation> tie_observations;
    RankAotReturnStageSites(tie_policy, tie_sites, &tie_observations);
    const bool tie_ok = tie_observations.size() == 3U &&
        tie_observations[0].site_index == 2U &&
        tie_observations[1].site_index == 0U &&
        tie_observations[2].site_index == 1U;
    return ok && tie_ok;
}

}  // namespace

bool RunAotReturnStageProfileProbe()
{
    const bool toggle_ok = ProbeToggle();
    const bool accounting_ok = ProbeStageAccounting();
    const bool snapshot_ok = ProbeSnapshotCopy();
    const bool residual_clamp_ok = ProbeResidualClamp();
    const bool sample_clamp_ok = ProbeSampleClamp();
    const bool disabled_ok = ProbeDisabledScope();
    const bool empty_census_ok = ProbeEmptyCensus();
    const bool top_n_ok = ProbeTopNOrdering();
    const bool all = toggle_ok && accounting_ok && snapshot_ok &&
        residual_clamp_ok && sample_clamp_ok && disabled_ok &&
        empty_census_ok && top_n_ok;
    std::cout << "return_stage_toggle=" << (toggle_ok ? "true" : "false")
              << "\nreturn_stage_accounting="
              << (accounting_ok ? "true" : "false")
              << "\nreturn_stage_snapshot="
              << (snapshot_ok ? "true" : "false")
              << "\nreturn_stage_residual_clamp="
              << (residual_clamp_ok ? "true" : "false")
              << "\nreturn_stage_sample_clamp="
              << (sample_clamp_ok ? "true" : "false")
              << "\nreturn_stage_disabled="
              << (disabled_ok ? "true" : "false")
              << "\nreturn_stage_empty_census="
              << (empty_census_ok ? "true" : "false")
              << "\nreturn_stage_top_n=" << (top_n_ok ? "true" : "false")
              << "\nreturn_stage_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
