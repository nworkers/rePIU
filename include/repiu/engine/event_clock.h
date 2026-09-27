#ifndef REPIU_ENGINE_EVENT_CLOCK_H_
#define REPIU_ENGINE_EVENT_CLOCK_H_

#include <cstdint>

namespace repiu::engine
{

// Task 754. The clock the timer tick schedule and the input timeline run on.
//
// It used to be SDL's (`SDL_GetTicksNS`), which on Linux reads
// CLOCK_MONOTONIC_RAW, while everything else -- the MP3 playback clock, the
// swap pacer, and the sound server the audio is consumed by -- runs on
// CLOCK_MONOTONIC. The two are the same rate on a machine whose clock is left
// alone. On WSL2 the time synchronisation service slews CLOCK_MONOTONIC by
// whole percents (-1% to -7% was measured against the Windows counter, in
// bursts of seconds), so the guest's 240 Hz ticks ran up to 9% fast against
// the music they are supposed to keep time with: pumpit8's arrows ran ahead
// and were pulled back when the game resynchronised to the MP3 position.
//
// The schedule now follows the steady clock, the one the audio follows.
// `REPIU_EVENT_CLOCK=sdl` restores SDL's.
inline bool ResolveEventClockUsesSteady(const char* value)
{
    if (value == nullptr)
    {
        return true;
    }
    const bool sdl = (value[0] == 's' || value[0] == 'S') &&
        (value[1] == 'd' || value[1] == 'D') &&
        (value[2] == 'l' || value[2] == 'L') && value[3] == '\0';
    const bool raw = (value[0] == 'r' || value[0] == 'R') &&
        (value[1] == 'a' || value[1] == 'A') &&
        (value[2] == 'w' || value[2] == 'W') && value[3] == '\0';
    return !(sdl || raw);
}

// An event carries a timestamp of the source clock (SDL's). Its age is
// measured on that clock and taken off the target clock's present, so the
// error is the clocks' rate difference over a few milliseconds.
inline std::uint64_t TranslateEventTimestamp(std::uint64_t event_timestamp,
                                             std::uint64_t source_now,
                                             std::uint64_t target_now)
{
    const std::uint64_t age =
        source_now > event_timestamp ? source_now - event_timestamp : 0U;
    return target_now > age ? target_now - age : 0U;
}

// How far the source clock ran from the target clock over an interval, in
// parts per million of the target's: positive when the source ran faster.
inline std::int64_t ClockDivergencePpm(std::uint64_t source_elapsed,
                                       std::uint64_t target_elapsed)
{
    if (target_elapsed == 0U)
    {
        return 0;
    }
    const std::int64_t difference = static_cast<std::int64_t>(source_elapsed) -
        static_cast<std::int64_t>(target_elapsed);
    return static_cast<std::int64_t>(
        static_cast<double>(difference) * 1000000.0 /
        static_cast<double>(target_elapsed));
}

struct HostClockDivergenceSnapshot
{
    std::uint64_t windows = 0;
    std::uint64_t windows_over_limit = 0;
    std::int64_t total_ppm = 0;
    std::int64_t worst_window_ppm = 0;
};

// Samples both clocks and judges them a window at a time, because the slew
// comes in bursts that a whole run's average hides.
class HostClockDivergenceMeter
{
public:
    static constexpr std::uint64_t kWindowNanoseconds = 1000000000ULL;
    // Half a percent: 1.2 ticks a second at 240 Hz.
    static constexpr std::int64_t kLimitPpm = 5000;

    // Returns true when this sample closed the first window over the limit.
    bool Sample(std::uint64_t source_now, std::uint64_t target_now)
    {
        if (!started_)
        {
            started_ = true;
            source_origin_ = source_now;
            target_origin_ = target_now;
            source_window_ = source_now;
            target_window_ = target_now;
            return false;
        }
        if (target_now < target_window_ || source_now < source_window_ ||
            target_now - target_window_ < kWindowNanoseconds)
        {
            return false;
        }
        const std::int64_t window_ppm = ClockDivergencePpm(
            source_now - source_window_, target_now - target_window_);
        source_window_ = source_now;
        target_window_ = target_now;
        ++snapshot_.windows;
        const std::int64_t magnitude =
            window_ppm < 0 ? -window_ppm : window_ppm;
        const std::int64_t worst = snapshot_.worst_window_ppm < 0
            ? -snapshot_.worst_window_ppm : snapshot_.worst_window_ppm;
        if (magnitude > worst)
        {
            snapshot_.worst_window_ppm = window_ppm;
        }
        snapshot_.total_ppm = ClockDivergencePpm(
            source_now - source_origin_, target_now - target_origin_);
        if (magnitude > kLimitPpm)
        {
            ++snapshot_.windows_over_limit;
            return snapshot_.windows_over_limit == 1U;
        }
        return false;
    }

    HostClockDivergenceSnapshot snapshot() const { return snapshot_; }

private:
    bool started_ = false;
    std::uint64_t source_origin_ = 0;
    std::uint64_t target_origin_ = 0;
    std::uint64_t source_window_ = 0;
    std::uint64_t target_window_ = 0;
    HostClockDivergenceSnapshot snapshot_;
};

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_EVENT_CLOCK_H_
