#include "repiu/engine/glide_setter_phase_timing.h"

#include <algorithm>
#include <cstdlib>

namespace repiu::engine
{
namespace
{

bool ReadGlideSetterPhaseProfileSetting()
{
    const char* value = std::getenv("REPIU_GLIDE_SETTER_PHASE");
    return value != nullptr && ResolveGlideSetterPhaseProfileEnabled(value);
}

std::uint64_t CounterDelta(
    Win32GlideSetterPhaseProfile* profile,
    std::uint64_t before,
    std::uint64_t after)
{
    if (after >= before)
    {
        return after - before;
    }
    ++profile->clamped_sample_count;
    return 0;
}

}  // namespace

bool ResolveGlideSetterPhaseProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideSetterPhaseProfileEnabled()
{
    static const bool enabled = ReadGlideSetterPhaseProfileSetting();
    return enabled;
}

void RecordGlideSetterPhaseSample(
    Win32GlideSetterPhaseProfile* profile,
    Win32GlideSetterPhaseKind kind,
    std::uint64_t entry_cycles,
    std::uint64_t apply_start_cycles,
    std::uint64_t error_start_cycles,
    std::uint64_t finish_cycles,
    std::uint32_t drain_iterations,
    bool error_reported)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    Win32GlideSetterPhaseEntry& entry =
        kind == Win32GlideSetterPhaseKind::kDepthMask
            ? profile->depth_mask
            : profile->alpha_blend;

    const std::uint64_t drain =
        CounterDelta(profile, entry_cycles, apply_start_cycles);
    const std::uint64_t apply =
        CounterDelta(profile, apply_start_cycles, error_start_cycles);
    const std::uint64_t error =
        CounterDelta(profile, error_start_cycles, finish_cycles);
    const std::uint64_t total =
        CounterDelta(profile, entry_cycles, finish_cycles);

    ++entry.call_count;
    entry.drain_cycles += drain;
    entry.apply_cycles += apply;
    entry.error_cycles += error;
    entry.total_cycles += total;
    entry.max_total_cycles = std::max(entry.max_total_cycles, total);
    entry.max_apply_cycles = std::max(entry.max_apply_cycles, apply);
    entry.max_error_cycles = std::max(entry.max_error_cycles, error);
    entry.drain_iteration_count += drain_iterations;
    if (error_reported)
    {
        ++entry.error_count;
    }
}

Win32GlideSetterPhaseSnapshot SnapshotGlideSetterPhaseTiming(
    const Win32GlideSetterPhaseProfile& profile)
{
    Win32GlideSetterPhaseSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    snapshot.depth_mask = profile.depth_mask;
    snapshot.alpha_blend = profile.alpha_blend;
    return snapshot;
}

}  // namespace repiu::engine
