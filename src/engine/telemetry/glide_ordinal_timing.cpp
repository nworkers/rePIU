#include "repiu/engine/glide_ordinal_timing.h"

#include <algorithm>
#include <cstdlib>

namespace repiu::engine
{
namespace
{

bool ReadGlideOrdinalTimingProfileSetting()
{
    const char* value = std::getenv("REPIU_GLIDE_ORDINAL_TIME_PROFILE");
    return value != nullptr &&
        ResolveGlideOrdinalTimingProfileEnabled(value);
}

template <typename T>
T CounterDelta(Win32GlideOrdinalTimingProfile* profile,
               T before,
               T after)
{
    if (after >= before)
    {
        return after - before;
    }
    ++profile->clamped_sample_count;
    return 0;
}

Win32GlideOrdinalTimingEntry* FindEntry(
    Win32GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal)
{
    if (profile == nullptr)
    {
        return nullptr;
    }
    profile->enabled = true;
    if (ordinal >= profile->entries.size())
    {
        ++profile->overflow_count;
        return nullptr;
    }
    return &profile->entries[ordinal];
}

}  // namespace

bool ResolveGlideOrdinalTimingProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideOrdinalTimingProfileEnabled()
{
    static const bool enabled = ReadGlideOrdinalTimingProfileSetting();
    return enabled;
}

void RecordGlideOrdinalGateTime(
    Win32GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal,
    std::uint64_t gate_cycles)
{
    Win32GlideOrdinalTimingEntry* entry = FindEntry(profile, ordinal);
    if (entry == nullptr)
    {
        return;
    }

    ++entry->count;
    entry->gate_cycles += gate_cycles;
    entry->max_gate_cycles = std::max(entry->max_gate_cycles, gate_cycles);
}

void RecordGlideOrdinalRendezvous(
    Win32GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal,
    std::uint64_t enter_cycles,
    std::uint64_t publish_cycles,
    std::uint64_t host_start_cycles,
    std::uint64_t host_finish_cycles,
    std::uint64_t resume_cycles)
{
    Win32GlideOrdinalTimingEntry* entry = FindEntry(profile, ordinal);
    if (entry == nullptr)
    {
        return;
    }
    ++entry->rendezvous_count;
    entry->queue_cycles +=
        CounterDelta(profile, enter_cycles, publish_cycles);
    entry->wake_cycles +=
        CounterDelta(profile, publish_cycles, host_start_cycles);
    entry->work_cycles +=
        CounterDelta(profile, host_start_cycles, host_finish_cycles);
    entry->complete_cycles +=
        CounterDelta(profile, host_finish_cycles, resume_cycles);
    entry->backend_total_cycles +=
        CounterDelta(profile, enter_cycles, resume_cycles);
}

void RecordGlideOrdinalDirectWork(
    Win32GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal,
    std::uint64_t cycles)
{
    Win32GlideOrdinalTimingEntry* entry = FindEntry(profile, ordinal);
    if (entry == nullptr)
    {
        return;
    }
    ++entry->direct_count;
    entry->direct_work_cycles += cycles;
}

Win32GlideOrdinalTimingSnapshot SnapshotGlideOrdinalTiming(
    const Win32GlideOrdinalTimingProfile& profile)
{
    Win32GlideOrdinalTimingSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.entries = profile.entries;
    snapshot.overflow_count = profile.overflow_count;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    for (const Win32GlideOrdinalTimingEntry& entry : profile.entries)
    {
        if (entry.count == 0U && entry.rendezvous_count == 0U &&
            entry.direct_count == 0U)
        {
            continue;
        }
        ++snapshot.active_entry_count;
        snapshot.completed_gate_count += entry.count;
        snapshot.gate_cycles += entry.gate_cycles;
        snapshot.rendezvous_count += entry.rendezvous_count;
        snapshot.queue_cycles += entry.queue_cycles;
        snapshot.wake_cycles += entry.wake_cycles;
        snapshot.work_cycles += entry.work_cycles;
        snapshot.complete_cycles += entry.complete_cycles;
        snapshot.residual_cycles += entry.residual_cycles;
        snapshot.backend_total_cycles += entry.backend_total_cycles;
        snapshot.direct_count += entry.direct_count;
        snapshot.direct_work_cycles += entry.direct_work_cycles;
    }
    return snapshot;
}

}  // namespace repiu::engine
