#pragma once

#include <cstdint>

namespace repiu::platform::win32
{

// Task 421. The game reads its music position through MSCDEX IOCTL 12, which
// returns `CdAudioWaveOut::current_lba()`. A user reported notes and the BGA
// jumping out of sync with the music, and a single exit-time snapshot of that
// value cannot say whether the position froze, moved backwards, or simply ran
// at the wrong rate. This is the time series that separates those.
//
// Sampled from the poll thread, never from the audio worker: a starved worker
// could not report its own starvation, which is precisely one of the candidates
// (design section 2, candidate B).
// See docs/design/20260805-421-cd-audio-position-reporting.md.

constexpr std::uint32_t kWin32CdAudioPositionCensusCapacity = 4096U;
constexpr std::uint32_t kWin32CdAudioPositionCensusDefaultMilliseconds = 100U;

// One reading of everything the position is derived from, so a wrong value can
// be attributed rather than guessed at.
struct Win32CdAudioPositionEntry
{
    std::uint32_t wall_milliseconds = 0;
    // What the guest would read right now.
    std::uint32_t current_lba = 0;
    // The decode cursor and the bytes still waiting in the SDL stream: their
    // difference is what produces `current_lba`.
    std::uint32_t queued_lba = 0;
    std::uint32_t stream_bytes = 0;
    std::uint32_t start_lba = 0;
    std::uint32_t end_lba = 0;
    // Worker loop iterations since the previous sample. Zero across a sample
    // interval means the worker did not run, which is candidate B.
    std::uint32_t worker_iterations = 0;
    // Times the worker found the stream empty while playback had more to give.
    std::uint32_t underruns = 0;
    std::uint32_t generation = 0;
    bool playing = false;
    bool paused = false;
};

struct Win32CdAudioPositionCensus
{
    bool enabled = false;
    std::uint32_t interval_milliseconds =
        kWin32CdAudioPositionCensusDefaultMilliseconds;
    std::uint32_t total_samples = 0;
    std::uint32_t overflow_count = 0;
    // Readings whose `current_lba` was below the previous one. A nonzero count
    // is candidate D on its own, since the position must never move backwards
    // while playing.
    std::uint32_t regression_count = 0;
    std::uint32_t entry_count = 0;
    Win32CdAudioPositionEntry entries[kWin32CdAudioPositionCensusCapacity];
};

bool CdAudioPositionCensusEnabled();
std::uint32_t CdAudioPositionCensusIntervalMilliseconds();

void RecordCdAudioPosition(Win32CdAudioPositionCensus* census,
                           const Win32CdAudioPositionEntry& entry);

// Writes the series to `build/cd_audio_position_census.txt` (or the path in
// `REPIU_CD_AUDIO_POSITION_CENSUS_DUMP`) and reports what it wrote. Called on
// teardown, after the guest thread has stopped.
bool WriteCdAudioPositionCensusDump(
    const Win32CdAudioPositionCensus& census,
    std::uint32_t* written_entry_count,
    std::uint32_t* regression_count);

}  // namespace repiu::platform::win32
