#include "repiu/engine/cd_audio_position_census.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace repiu::engine
{

namespace
{

// Resolved once. This census only reads counters the audio path already keeps,
// so it adds no work to the worker, but the gate stays so a measurement run is
// always a deliberate choice.
bool ResolveEnabled()
{
    const char* value = std::getenv("REPIU_CD_AUDIO_POSITION_CENSUS");
    return value != nullptr && std::strcmp(value, "0") != 0;
}

std::uint32_t ResolveInterval()
{
    const char* value = std::getenv("REPIU_CD_AUDIO_POSITION_CENSUS_MS");
    if (value == nullptr)
    {
        return kWin32CdAudioPositionCensusDefaultMilliseconds;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || parsed < 1UL || parsed > 10000UL)
    {
        return kWin32CdAudioPositionCensusDefaultMilliseconds;
    }
    return static_cast<std::uint32_t>(parsed);
}

}  // namespace

bool CdAudioPositionCensusEnabled()
{
    static const bool enabled = ResolveEnabled();
    return enabled;
}

std::uint32_t CdAudioPositionCensusIntervalMilliseconds()
{
    static const std::uint32_t interval = ResolveInterval();
    return interval;
}

void RecordCdAudioPosition(Win32CdAudioPositionCensus* census,
                           const Win32CdAudioPositionEntry& entry)
{
    if (census == nullptr)
    {
        return;
    }
    ++census->total_samples;

    // A position that moves backwards while playing is a defect on its own, so
    // it is counted even after the ring fills and stops keeping entries.
    if (census->entry_count != 0U && entry.playing)
    {
        const Win32CdAudioPositionEntry& previous =
            census->entries[census->entry_count - 1U];
        if (previous.playing && previous.generation == entry.generation &&
            entry.current_lba < previous.current_lba)
        {
            ++census->regression_count;
        }
    }

    if (census->entry_count >= kWin32CdAudioPositionCensusCapacity)
    {
        ++census->overflow_count;
        return;
    }
    census->entries[census->entry_count] = entry;
    ++census->entry_count;
}

bool WriteCdAudioPositionCensusDump(
    const Win32CdAudioPositionCensus& census,
    std::uint32_t* written_entry_count,
    std::uint32_t* regression_count)
{
    if (written_entry_count != nullptr)
    {
        *written_entry_count = 0U;
    }
    if (regression_count != nullptr)
    {
        *regression_count = census.regression_count;
    }
    if (!census.enabled || census.entry_count == 0U)
    {
        return false;
    }

    std::filesystem::path path = "build/cd_audio_position_census.txt";
    if (const char* value =
            std::getenv("REPIU_CD_AUDIO_POSITION_CENSUS_DUMP"))
    {
        if (std::strcmp(value, "1") != 0 && value[0] != '\0')
        {
            path = value;
        }
    }
    std::error_code error;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }

    out << "# Task 421 CD audio position census\n"
        << "# interval_ms=" << census.interval_milliseconds
        << " samples=" << census.total_samples
        << " entries=" << census.entry_count
        << " overflow=" << census.overflow_count
        << " regressions=" << census.regression_count << "\n"
        << "# wall_ms current_lba queued_lba stream_bytes start_lba end_lba "
           "worker_iterations underruns generation playing paused "
           "delta_lba ticks_due ticks_injected tick_lag_ms safe_point_traps "
           "ticks_coalesced ticks_coalesced_in_gate\n";
    std::uint32_t previous_lba = 0;
    std::uint64_t cumulative_due = 0;
    std::uint64_t cumulative_injected = 0;
    for (std::uint32_t index = 0; index < census.entry_count; ++index)
    {
        const Win32CdAudioPositionEntry& entry = census.entries[index];
        // The signed delta is what a reader actually scans for: a negative
        // value is a position moving backwards, and a zero across a playing
        // interval is the freeze that precedes a jump.
        const std::int64_t delta = index == 0U
            ? 0
            : static_cast<std::int64_t>(entry.current_lba) -
                static_cast<std::int64_t>(previous_lba);
        previous_lba = entry.current_lba;

        // Task 430: how far the guest's tick-driven clock has fallen behind
        // real time by this sample. The tick period is recovered from the data
        // -- elapsed wall over ticks owed -- rather than assumed to be 240Hz,
        // because the guest is free to reprogram the divisor and a hard-coded
        // rate would silently produce a wrong answer if it ever did.
        cumulative_due += entry.timer_ticks_due;
        cumulative_injected += entry.timer_ticks_injected;
        const std::uint64_t deficit = cumulative_due > cumulative_injected
            ? cumulative_due - cumulative_injected
            : 0U;
        const std::uint64_t lag_milliseconds = cumulative_due != 0U
            ? deficit * entry.wall_milliseconds / cumulative_due
            : 0U;

        out << entry.wall_milliseconds << ' ' << entry.current_lba << ' '
            << entry.queued_lba << ' ' << entry.stream_bytes << ' '
            << entry.start_lba << ' ' << entry.end_lba << ' '
            << entry.worker_iterations << ' ' << entry.underruns << ' '
            << entry.generation << ' ' << (entry.playing ? 1 : 0) << ' '
            << (entry.paused ? 1 : 0) << ' ' << delta << ' '
            << entry.timer_ticks_due << ' ' << entry.timer_ticks_injected
            << ' ' << lag_milliseconds << ' ' << entry.safe_point_traps << ' '
            << entry.ticks_coalesced << ' ' << entry.ticks_coalesced_in_gate
            << '\n';
    }
    out.flush();
    if (!out.good())
    {
        return false;
    }
    if (written_entry_count != nullptr)
    {
        *written_entry_count = census.entry_count;
    }
    return true;
}

}  // namespace repiu::engine
