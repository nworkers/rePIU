#include "pit_timer_probe.h"

#include "repiu/hle/pit_timer.h"

#include <cmath>
#include <cstdint>
#include <iostream>

namespace repiu::tools
{

bool RunPitTimerProbe()
{
    hle::PitChannel0 channel;
    const hle::PitChannel0Snapshot initial = channel.snapshot();
    if (initial.divisor != hle::PitChannel0::kDefaultDivisor ||
        !channel.WriteControl(0x36U))
    {
        std::cout << "pit_timer_probe=false\n";
        return false;
    }

    hle::PitChannel0Snapshot published;
    if (!channel.WriteData(0x6CU, &published) ||
        channel.snapshot().generation != initial.generation ||
        !channel.WriteData(0x13U, &published))
    {
        std::cout << "pit_timer_probe=false\n";
        return false;
    }

    const hle::PitChannel0Snapshot programmed = channel.snapshot();
    const double frequency_hz = hle::PitFrequencyHz(programmed.divisor);
    hle::PitIrqSchedule schedule;
    const std::uint64_t epoch = 1000000ULL;
    hle::PitIrqDueRange due_range;
    const bool cadence_valid =
        schedule.Poll(programmed, epoch) == 0U &&
        schedule.Poll(programmed, epoch + 4166000ULL) == 0U &&
        schedule.Poll(programmed, epoch + 4167000ULL) == 1U &&
        schedule.Poll(programmed, epoch + 8334000ULL) == 1U &&
        schedule.Poll(programmed, epoch + 25000000ULL, &due_range) == 4U;
    const std::uint64_t first_due = hle::PitElapsedNanosecondsForTick(
        due_range.first_tick_ordinal, due_range.divisor);
    const std::uint64_t last_due = hle::PitElapsedNanosecondsForTick(
        due_range.first_tick_ordinal + due_range.tick_count - 1U,
        due_range.divisor);
    const bool due_range_valid = due_range.epoch_nanoseconds == epoch &&
        due_range.first_tick_ordinal == 3U && due_range.tick_count == 4U &&
        due_range.divisor == 4972U && first_due < last_due &&
        last_due <= 25000000ULL;

    const bool valid =
        programmed.generation == initial.generation + 1U &&
        programmed.divisor == 4972U &&
        std::abs(frequency_hz - 240.0) < 0.000001 &&
        cadence_valid && due_range_valid &&
        !channel.WriteControl(0x76U);
    std::cout << "pit_timer_probe=" << (valid ? "true" : "false")
              << ",divisor=" << programmed.divisor
              << ",frequency_hz=" << frequency_hz << "\n";
    return valid;
}

}  // namespace repiu::tools
