#include "event_clock_probe.h"

#include "repiu/engine/event_clock.h"
#include "repiu/hle/pit_timer.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{

bool RunEventClockProbe()
{
    using engine::ClockDivergencePpm;
    using engine::HostClockDivergenceMeter;
    using engine::ResolveEventClockUsesSteady;
    using engine::TranslateEventTimestamp;

    const bool clock_switch =
        ResolveEventClockUsesSteady(nullptr) &&
        ResolveEventClockUsesSteady("") &&
        ResolveEventClockUsesSteady("steady") &&
        !ResolveEventClockUsesSteady("sdl") &&
        !ResolveEventClockUsesSteady("SDL") &&
        !ResolveEventClockUsesSteady("raw") &&
        ResolveEventClockUsesSteady("sdlx");

    // An event 3 ms old on the source clock is 3 ms old on the target clock,
    // whatever the two clocks read.
    const bool translation =
        TranslateEventTimestamp(9997000000ULL, 10000000000ULL,
                                500000000ULL) == 497000000ULL &&
        // A timestamp from the future is the present.
        TranslateEventTimestamp(10000000001ULL, 10000000000ULL,
                                500000000ULL) == 500000000ULL &&
        // An age beyond the target clock's reading does not wrap.
        TranslateEventTimestamp(0U, 10000000000ULL, 500000000ULL) == 0U;

    const bool divergence =
        ClockDivergencePpm(1000000000ULL, 1000000000ULL) == 0 &&
        ClockDivergencePpm(1090000000ULL, 1000000000ULL) == 90000 &&
        ClockDivergencePpm(990000000ULL, 1000000000ULL) == -10000 &&
        ClockDivergencePpm(1000U, 0U) == 0;

    // The measured case: the target clock slewed 4% slow for two seconds of
    // ten. The run's average is under 1%; the windows say 4%.
    HostClockDivergenceMeter meter;
    std::uint64_t source = 1000U;
    std::uint64_t target = 777U;
    bool warned = false;
    std::uint32_t warnings = 0U;
    meter.Sample(source, target);
    for (std::uint32_t second = 0; second < 10U; ++second)
    {
        const bool slewed = second == 4U || second == 5U;
        source += slewed ? 1041666667ULL : 1000000000ULL;
        target += 1000000000ULL;
        if (meter.Sample(source, target))
        {
            warned = true;
            ++warnings;
        }
    }
    const auto snapshot = meter.snapshot();
    const bool metered = warned && warnings == 1U &&
        snapshot.windows == 10U &&
        snapshot.windows_over_limit == 2U &&
        snapshot.worst_window_ppm > 41000 &&
        snapshot.worst_window_ppm < 42000 &&
        snapshot.total_ppm > 8000 && snapshot.total_ppm < 8500;

    // What the divergence did to the guest: 240 Hz ticks counted on a clock
    // 9% fast are 261.6 to a second of the clock the music follows.
    const std::uint64_t ticks_on_fast_clock = hle::PitTickCountForElapsed(
        1090000000ULL, 4971U);
    const std::uint64_t ticks_on_audio_clock = hle::PitTickCountForElapsed(
        1000000000ULL, 4971U);
    const bool tick_rates =
        ticks_on_fast_clock == 261U && ticks_on_audio_clock == 240U;

    const bool valid =
        clock_switch && translation && divergence && metered && tick_rates;
    // The core probe leaves the stream in hex for whoever prints next.
    const std::ios_base::fmtflags flags = std::cout.flags();
    std::cout << std::dec << "event_clock_probe=" << (valid ? "true" : "false")
              << ",switch=" << (clock_switch ? "true" : "false")
              << ",translation=" << (translation ? "true" : "false")
              << ",divergence=" << (divergence ? "true" : "false")
              << ",metered=" << (metered ? "true" : "false")
              << ",windows=" << snapshot.windows
              << ",over_limit=" << snapshot.windows_over_limit
              << ",worst_ppm=" << snapshot.worst_window_ppm
              << ",total_ppm=" << snapshot.total_ppm
              << ",ticks_fast=" << ticks_on_fast_clock
              << ",ticks_audio=" << ticks_on_audio_clock << "\n";
    std::cout.flags(flags);
    return valid;
}

}  // namespace repiu::tools
