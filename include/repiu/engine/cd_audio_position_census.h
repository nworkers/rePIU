#pragma once

#include <cstdint>

namespace repiu::engine
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

constexpr std::uint32_t kCdAudioPositionCensusCapacity = 4096U;
constexpr std::uint32_t kCdAudioPositionCensusDefaultMilliseconds = 100U;

// One reading of everything the position is derived from, so a wrong value can
// be attributed rather than guessed at.
struct CdAudioPositionEntry
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
    // Task 430: timer ticks owed and actually injected since the previous
    // sample. The music runs on real time and the guest's own clock runs on
    // injected ticks, so their difference across this interval is the rate at
    // which the two drift apart. Deltas rather than totals, for the same reason
    // `worker_iterations` is: a run-long total cannot say *when* the loss
    // happened, which is the whole question.
    std::uint32_t timer_ticks_due = 0;
    std::uint32_t timer_ticks_injected = 0;
    // Task 431: the opportunity side of the same interval. `safe_point_traps`
    // is where 99% of injections actually happen, so it measures how often the
    // guest reached one; `ticks_coalesced_in_gate` is how many of the losses
    // fell in the Glide gate, where no safe point exists to reach.
    std::uint32_t safe_point_traps = 0;
    // Both halves of the ratio, because `due - injected` is *not* the interval's
    // coalesced count: an injection here can consume a tick armed in the
    // previous interval, so that subtraction understates the denominator and
    // the in-gate share can read above 100%.
    std::uint32_t ticks_coalesced = 0;
    std::uint32_t ticks_coalesced_in_gate = 0;
    bool playing = false;
    bool paused = false;
};

struct CdAudioPositionCensus
{
    bool enabled = false;
    std::uint32_t interval_milliseconds =
        kCdAudioPositionCensusDefaultMilliseconds;
    std::uint32_t total_samples = 0;
    std::uint32_t overflow_count = 0;
    // Readings whose `current_lba` was below the previous one. A nonzero count
    // is candidate D on its own, since the position must never move backwards
    // while playing.
    std::uint32_t regression_count = 0;
    std::uint32_t entry_count = 0;
    CdAudioPositionEntry entries[kCdAudioPositionCensusCapacity];
};

bool CdAudioPositionCensusEnabled();
std::uint32_t CdAudioPositionCensusIntervalMilliseconds();

void RecordCdAudioPosition(CdAudioPositionCensus* census,
                           const CdAudioPositionEntry& entry);

// Writes the series to `build/cd_audio_position_census.txt` (or the path in
// `REPIU_CD_AUDIO_POSITION_CENSUS_DUMP`) and reports what it wrote. Called on
// teardown, after the guest thread has stopped.
bool WriteCdAudioPositionCensusDump(
    const CdAudioPositionCensus& census,
    std::uint32_t* written_entry_count,
    std::uint32_t* regression_count);

}  // namespace repiu::engine
