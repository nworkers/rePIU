#include "repiu/platform/win32/aot_return_stage_profile.h"

#include "repiu/platform/win32/aot_return_patch_policy.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/env_toggle.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include "repiu/platform/host_time.h"


namespace repiu::platform::win32
{
namespace
{

std::uint64_t StageIndex(const AotReturnStage stage)
{
    return static_cast<std::uint64_t>(stage);
}

}  // namespace

std::uint64_t ReadAotReturnStageCycles()
{
    return repiu::platform::ReadCycleCounter();
}

bool ResolveAotReturnStageProfileEnabled(const char* setting)
{
    return repiu::runtime::ResolveOptInToggle(setting);
}

bool AotReturnStageProfileEnabled()
{
    static const bool enabled = ResolveAotReturnStageProfileEnabled(
        std::getenv("REPIU_AOT_RETURN_STAGE_PROFILE"));
    return enabled;
}

void RecordAotReturnStageSample(Win32AotReturnStageProfile* profile,
                                const AotReturnStage stage,
                                const std::uint64_t start_cycles,
                                const std::uint64_t end_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    const auto index = static_cast<std::size_t>(StageIndex(stage));
    if (index >= profile->cycles.size())
    {
        return;
    }
    profile->enabled = true;
    if (end_cycles < start_cycles)
    {
        ++profile->clamped_sample_count;
        ++profile->counts[index];
        return;
    }
    const std::uint64_t cycles = end_cycles - start_cycles;
    profile->cycles[index] += cycles;
    ++profile->counts[index];
    profile->max_cycles[index] = std::max(profile->max_cycles[index], cycles);
    profile->stage_total_cycles += cycles;
}

void RecordAotReturnOuterSample(Win32AotReturnStageProfile* profile,
                                const std::uint64_t start_cycles,
                                const std::uint64_t end_cycles,
                                const std::uint64_t stage_total_at_entry)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->outer_count;
    if (end_cycles < start_cycles)
    {
        ++profile->clamped_sample_count;
        return;
    }
    const std::uint64_t cycles = end_cycles - start_cycles;
    profile->outer_cycles += cycles;
    profile->max_outer_cycles = std::max(profile->max_outer_cycles, cycles);
    // The stages of this one return, not of the run: an inner scope may have
    // clamped its own sample, so the delta is read rather than recomputed.
    const std::uint64_t stage_cycles =
        profile->stage_total_cycles >= stage_total_at_entry
        ? profile->stage_total_cycles - stage_total_at_entry
        : 0U;
    if (cycles < stage_cycles)
    {
        ++profile->residual_clamp_count;
        return;
    }
    profile->residual_cycles += cycles - stage_cycles;
}

Win32AotReturnStageSnapshot SnapshotAotReturnStageProfile(
    const Win32AotReturnStageProfile& profile)
{
    Win32AotReturnStageSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.cycles = profile.cycles;
    snapshot.counts = profile.counts;
    snapshot.max_cycles = profile.max_cycles;
    snapshot.stage_total_cycles = profile.stage_total_cycles;
    snapshot.outer_cycles = profile.outer_cycles;
    snapshot.outer_count = profile.outer_count;
    snapshot.max_outer_cycles = profile.max_outer_cycles;
    snapshot.residual_cycles = profile.residual_cycles;
    snapshot.residual_clamp_count = profile.residual_clamp_count;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    return snapshot;
}

std::uint64_t AotReturnStageCoveredCycles(
    const Win32AotReturnStageSnapshot& snapshot)
{
    std::uint64_t covered = 0;
    for (const std::uint64_t cycles : snapshot.cycles)
    {
        covered += cycles;
    }
    return covered;
}

void RankAotReturnStageSites(
    const Win32AotReturnPatchPolicy& policy,
    const std::vector<runtime::AotDbtReturnDispatchSite>& sites,
    std::vector<Win32AotReturnStageSiteObservation>* observations)
{
    if (observations == nullptr)
    {
        return;
    }
    observations->clear();
    const std::size_t site_count = std::min(policy.sites.size(), sites.size());
    for (std::size_t index = 0; index < site_count; ++index)
    {
        const Win32AotReturnPatchSiteState& state = policy.sites[index];
        if (state.miss_count == 0U)
        {
            continue;
        }
        Win32AotReturnStageSiteObservation observation;
        observation.site_index = static_cast<std::uint32_t>(index);
        observation.guest_source = sites[index].guest_source;
        observation.miss_cache_offset = sites[index].miss_cache_offset;
        observation.observation_count = state.miss_count;
        observation.distinct_target_count = state.target_count;
        observation.bypass_count = state.bypass_count;
        observation.megamorphic = state.megamorphic;
        observations->push_back(observation);
    }
    std::sort(observations->begin(), observations->end(),
              [](const Win32AotReturnStageSiteObservation& left,
                 const Win32AotReturnStageSiteObservation& right) {
                  if (left.observation_count != right.observation_count)
                  {
                      return left.observation_count > right.observation_count;
                  }
                  if (left.bypass_count != right.bypass_count)
                  {
                      return left.bypass_count > right.bypass_count;
                  }
                  return left.site_index < right.site_index;
              });
    if (observations->size() > kAotReturnStageSiteReportCapacity)
    {
        observations->resize(kAotReturnStageSiteReportCapacity);
    }
}

AotReturnStageScope::AotReturnStageScope(
    Win32AotReturnStageProfile* profile, const AotReturnStage stage)
{
    // The gate comes before the clock read so a disabled run never issues
    // RDTSC on the return path.
    if (profile == nullptr || !AotReturnStageProfileEnabled())
    {
        return;
    }
    profile_ = profile;
    stage_ = stage;
    start_cycles_ = ReadAotReturnStageCycles();
}

void AotReturnStageScope::Close()
{
    if (profile_ == nullptr)
    {
        return;
    }
    RecordAotReturnStageSample(profile_, stage_, start_cycles_,
                               ReadAotReturnStageCycles());
    profile_ = nullptr;
}

AotReturnStageScope::~AotReturnStageScope()
{
    Close();
}

AotReturnOuterScope::AotReturnOuterScope(Win32AotReturnStageProfile* profile)
{
    if (profile == nullptr || !AotReturnStageProfileEnabled())
    {
        return;
    }
    // The DBT adapter and the VEH path both open an outer window around the
    // same resolver. Only the outer frame attributes, so one return is never
    // counted twice and the adapter's site lookup stays inside its window.
    if (profile->outer_depth != 0U)
    {
        return;
    }
    profile_ = profile;
    owns_depth_ = true;
    ++profile->outer_depth;
    stage_total_at_entry_ = profile->stage_total_cycles;
    start_cycles_ = ReadAotReturnStageCycles();
}

AotReturnOuterScope::~AotReturnOuterScope()
{
    if (profile_ == nullptr)
    {
        return;
    }
    RecordAotReturnOuterSample(profile_, start_cycles_,
                               ReadAotReturnStageCycles(),
                               stage_total_at_entry_);
    if (owns_depth_ && profile_->outer_depth != 0U)
    {
        --profile_->outer_depth;
    }
}

}  // namespace repiu::platform::win32
