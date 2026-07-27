#include "repiu/platform/win32/single_step_hotspot_profile.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace repiu::platform::win32
{
namespace
{

std::uint64_t ReadProfileCycles()
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

bool ReadSingleStepHotspotProfileSetting()
{
    const char* value = std::getenv("REPIU_SINGLE_STEP_HOTSPOT_PROFILE");
    return value != nullptr &&
        ResolveSingleStepHotspotProfileEnabled(value);
}

std::uint32_t HashGuestAddress(std::uint32_t guest_address)
{
    static_assert(
        (kWin32SingleStepHotspotCapacity &
         (kWin32SingleStepHotspotCapacity - 1U)) == 0U);
    return (guest_address * 2654435761U) &
        (kWin32SingleStepHotspotCapacity - 1U);
}

Win32SingleStepHotspotSample MakeSample(
    const Win32SingleStepHotspotEntry& entry)
{
    Win32SingleStepHotspotSample sample;
    sample.guest_address = entry.guest_address;
    sample.sample_count = entry.sample_count;
    sample.total_cycles = entry.total_cycles;
    sample.max_cycles = entry.max_cycles;
    sample.outcome_counts = entry.outcome_counts;
    sample.outcome_cycles = entry.outcome_cycles;
    sample.stage_counts = entry.stage_counts;
    sample.stage_cycles = entry.stage_cycles;
    return sample;
}

template <typename CountArray, typename CycleArray>
void AccumulateStageTally(const Win32SingleStepStageTally* stages,
                          CountArray* counts,
                          CycleArray* cycles)
{
    if (stages == nullptr)
    {
        return;
    }
    for (std::uint32_t index = 0;
         index < kSingleStepProfileStageCount; ++index)
    {
        (*counts)[index] += stages->counts[index];
        (*cycles)[index] += stages->cycles[index];
    }
}

}  // namespace

bool ResolveSingleStepHotspotProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool SingleStepHotspotProfileEnabled()
{
    static const bool enabled = ReadSingleStepHotspotProfileSetting();
    return enabled;
}

void RecordSingleStepHotspot(
    Win32SingleStepHotspotProfile* profile,
    std::uint32_t guest_address,
    std::uint64_t cycles,
    SingleStepProfileOutcome outcome,
    const Win32SingleStepStageTally* stages)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->total_sample_count;
    profile->total_cycles += cycles;
    profile->max_cycles = std::max(profile->max_cycles, cycles);
    AccumulateStageTally(stages, &profile->stage_counts,
                         &profile->stage_cycles);

    std::uint32_t outcome_index = static_cast<std::uint32_t>(outcome);
    if (outcome_index >= kSingleStepProfileOutcomeCount)
    {
        outcome_index = static_cast<std::uint32_t>(
            SingleStepProfileOutcome::kTrapFlagRearm);
    }
    ++profile->outcome_counts[outcome_index];
    profile->outcome_cycles[outcome_index] += cycles;

    const std::uint32_t first = HashGuestAddress(guest_address);
    for (std::uint32_t probe = 0;
         probe < kWin32SingleStepHotspotCapacity; ++probe)
    {
        Win32SingleStepHotspotEntry& entry =
            profile->entries[
                (first + probe) &
                (kWin32SingleStepHotspotCapacity - 1U)];
        if (!entry.occupied)
        {
            entry.occupied = true;
            entry.guest_address = guest_address;
            ++profile->distinct_guest_count;
        }
        if (entry.guest_address != guest_address)
        {
            continue;
        }
        ++entry.sample_count;
        entry.total_cycles += cycles;
        entry.max_cycles = std::max(entry.max_cycles, cycles);
        ++entry.outcome_counts[outcome_index];
        entry.outcome_cycles[outcome_index] += cycles;
        AccumulateStageTally(stages, &entry.stage_counts,
                             &entry.stage_cycles);
        return;
    }
    ++profile->overflow_count;
}

Win32SingleStepHotspotProfileSnapshot SnapshotSingleStepHotspotProfile(
    const Win32SingleStepHotspotProfile& profile)
{
    Win32SingleStepHotspotProfileSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.total_sample_count = profile.total_sample_count;
    snapshot.distinct_guest_count = profile.distinct_guest_count;
    snapshot.overflow_count = profile.overflow_count;
    snapshot.total_cycles = profile.total_cycles;
    snapshot.max_cycles = profile.max_cycles;
    snapshot.outcome_counts = profile.outcome_counts;
    snapshot.outcome_cycles = profile.outcome_cycles;
    snapshot.stage_counts = profile.stage_counts;
    snapshot.stage_cycles = profile.stage_cycles;

    std::vector<Win32SingleStepHotspotSample> samples;
    samples.reserve(profile.distinct_guest_count);
    for (const Win32SingleStepHotspotEntry& entry : profile.entries)
    {
        if (entry.occupied)
        {
            samples.push_back(MakeSample(entry));
        }
    }

    std::sort(
        samples.begin(), samples.end(),
        [](const auto& left, const auto& right) {
            if (left.sample_count != right.sample_count)
            {
                return left.sample_count > right.sample_count;
            }
            if (left.total_cycles != right.total_cycles)
            {
                return left.total_cycles > right.total_cycles;
            }
            return left.guest_address < right.guest_address;
        });
    snapshot.count_hotspot_count = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(samples.size()),
        kWin32SingleStepHotspotReportCapacity);
    for (std::uint32_t index = 0;
         index < snapshot.count_hotspot_count; ++index)
    {
        snapshot.count_hotspots[index] = samples[index];
        snapshot.top_count_coverage_count += samples[index].sample_count;
    }

    std::sort(
        samples.begin(), samples.end(),
        [](const auto& left, const auto& right) {
            if (left.total_cycles != right.total_cycles)
            {
                return left.total_cycles > right.total_cycles;
            }
            if (left.sample_count != right.sample_count)
            {
                return left.sample_count > right.sample_count;
            }
            return left.guest_address < right.guest_address;
        });
    snapshot.cycle_hotspot_count = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(samples.size()),
        kWin32SingleStepHotspotReportCapacity);
    for (std::uint32_t index = 0;
         index < snapshot.cycle_hotspot_count; ++index)
    {
        snapshot.cycle_hotspots[index] = samples[index];
        snapshot.top_cycle_coverage_cycles += samples[index].total_cycles;
    }
    return snapshot;
}

SingleStepHotspotCycleScope::SingleStepHotspotCycleScope(
    Win32SingleStepHotspotProfile* profile,
    std::uint32_t guest_address)
    : profile_(profile),
      guest_address_(guest_address),
      start_cycles_(profile != nullptr ? ReadProfileCycles() : 0U)
{
}

SingleStepHotspotCycleScope::~SingleStepHotspotCycleScope()
{
    if (profile_ == nullptr)
    {
        return;
    }
    const std::uint64_t end_cycles = ReadProfileCycles();
    RecordSingleStepHotspot(
        profile_, guest_address_, end_cycles - start_cycles_, outcome_,
        &stages_);
}

void SingleStepHotspotCycleScope::SetOutcome(
    SingleStepProfileOutcome outcome)
{
    outcome_ = outcome;
}

void SingleStepHotspotCycleScope::AddStageCycles(
    SingleStepProfileStage stage, std::uint64_t cycles)
{
    const std::uint32_t index = static_cast<std::uint32_t>(stage);
    if (index >= kSingleStepProfileStageCount)
    {
        return;
    }
    ++stages_.counts[index];
    stages_.cycles[index] += cycles;
}

SingleStepHotspotStageScope::SingleStepHotspotStageScope(
    SingleStepHotspotCycleScope& parent,
    SingleStepProfileStage stage)
    : parent_(parent.active() ? &parent : nullptr),
      stage_(stage),
      start_cycles_(parent.active() ? ReadProfileCycles() : 0U)
{
}

SingleStepHotspotStageScope::SingleStepHotspotStageScope(
    SingleStepHotspotCycleScope* parent,
    SingleStepProfileStage stage)
    : parent_(parent != nullptr && parent->active() ? parent : nullptr),
      stage_(stage),
      start_cycles_(parent_ != nullptr ? ReadProfileCycles() : 0U)
{
}

SingleStepHotspotStageScope::~SingleStepHotspotStageScope()
{
    if (parent_ == nullptr)
    {
        return;
    }
    const std::uint64_t end_cycles = ReadProfileCycles();
    parent_->AddStageCycles(stage_, end_cycles - start_cycles_);
}

}  // namespace repiu::platform::win32
