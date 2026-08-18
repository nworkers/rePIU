#include "repiu/hle/pit_timer.h"

namespace repiu::hle
{
namespace
{

constexpr std::uint64_t kNanosecondsPerSecond = 1000000000ULL;

std::uint64_t PackConfiguration(std::uint32_t generation,
                                std::uint32_t divisor)
{
    return (static_cast<std::uint64_t>(generation) << 32U) | divisor;
}

}  // namespace

PitChannel0::PitChannel0()
    : configuration_(PackConfiguration(1U, kDefaultDivisor))
{
}

bool PitChannel0::WriteControl(std::uint8_t value)
{
    const std::uint8_t channel = static_cast<std::uint8_t>(
        (value >> 6U) & 0x03U);
    const std::uint8_t access_mode = static_cast<std::uint8_t>(
        (value >> 4U) & 0x03U);
    if (channel != 0U || access_mode == 0U)
    {
        return false;
    }

    access_mode_ = access_mode;
    awaiting_low_byte_ = true;
    return true;
}

bool PitChannel0::WriteData(
    std::uint8_t value,
    PitChannel0Snapshot* published_snapshot)
{
    const PitChannel0Snapshot current = snapshot();
    std::uint32_t reload_value = current.divisor == kDefaultDivisor
        ? 0U
        : current.divisor;

    if (access_mode_ == 1U)
    {
        reload_value = (reload_value & 0xFF00U) | value;
        PublishDivisor(reload_value, published_snapshot);
        return true;
    }
    if (access_mode_ == 2U)
    {
        reload_value = (reload_value & 0x00FFU) |
            (static_cast<std::uint32_t>(value) << 8U);
        PublishDivisor(reload_value, published_snapshot);
        return true;
    }
    if (access_mode_ != 3U)
    {
        return false;
    }

    if (awaiting_low_byte_)
    {
        pending_low_byte_ = value;
        awaiting_low_byte_ = false;
        return true;
    }

    reload_value = pending_low_byte_ |
        (static_cast<std::uint32_t>(value) << 8U);
    awaiting_low_byte_ = true;
    PublishDivisor(reload_value, published_snapshot);
    return true;
}

PitChannel0Snapshot PitChannel0::snapshot() const
{
    const std::uint64_t configuration =
        configuration_.load(std::memory_order_acquire);
    PitChannel0Snapshot result;
    result.generation = static_cast<std::uint32_t>(configuration >> 32U);
    result.divisor = static_cast<std::uint32_t>(configuration);
    return result;
}

void PitChannel0::PublishDivisor(
    std::uint32_t divisor,
    PitChannel0Snapshot* published_snapshot)
{
    if (divisor == 0U)
    {
        divisor = kDefaultDivisor;
    }

    const PitChannel0Snapshot current = snapshot();
    PitChannel0Snapshot next;
    next.generation = current.generation + 1U;
    next.divisor = divisor;
    configuration_.store(
        PackConfiguration(next.generation, next.divisor),
        std::memory_order_release);
    if (published_snapshot != nullptr)
    {
        *published_snapshot = next;
    }
}

std::uint64_t PitIrqSchedule::Poll(
    const PitChannel0Snapshot& snapshot,
    std::uint64_t elapsed_nanoseconds,
    PitIrqDueRange* due_range)
{
    if (due_range != nullptr)
    {
        *due_range = {};
    }
    if (!initialized_ || snapshot.generation != generation_)
    {
        initialized_ = true;
        generation_ = snapshot.generation;
        epoch_nanoseconds_ = elapsed_nanoseconds;
        emitted_tick_count_ = 0;
        return 0;
    }
    if (elapsed_nanoseconds < epoch_nanoseconds_)
    {
        epoch_nanoseconds_ = elapsed_nanoseconds;
        emitted_tick_count_ = 0;
        return 0;
    }

    const std::uint64_t tick_count = PitTickCountForElapsed(
        elapsed_nanoseconds - epoch_nanoseconds_, snapshot.divisor);
    if (tick_count <= emitted_tick_count_)
    {
        return 0;
    }

    const std::uint64_t due = tick_count - emitted_tick_count_;
    if (due_range != nullptr)
    {
        due_range->epoch_nanoseconds = epoch_nanoseconds_;
        due_range->first_tick_ordinal = emitted_tick_count_ + 1U;
        due_range->tick_count = due;
        due_range->divisor = snapshot.divisor;
    }
    emitted_tick_count_ = tick_count;
    return due;
}

std::uint64_t PitTickCountForElapsed(
    std::uint64_t elapsed_nanoseconds,
    std::uint32_t divisor)
{
    if (divisor == 0U)
    {
        divisor = PitChannel0::kDefaultDivisor;
    }

    const std::uint64_t whole_seconds =
        elapsed_nanoseconds / kNanosecondsPerSecond;
    const std::uint64_t remaining_nanoseconds =
        elapsed_nanoseconds % kNanosecondsPerSecond;
    const std::uint64_t input_clocks =
        whole_seconds * PitChannel0::kInputClockHz +
        remaining_nanoseconds * PitChannel0::kInputClockHz /
            kNanosecondsPerSecond;
    return input_clocks / divisor;
}

std::uint64_t PitElapsedNanosecondsForTick(
    std::uint64_t tick_ordinal,
    std::uint32_t divisor)
{
    if (divisor == 0U)
    {
        divisor = PitChannel0::kDefaultDivisor;
    }
    if (tick_ordinal == 0U)
    {
        return 0U;
    }

    const std::uint64_t input_clocks = tick_ordinal * divisor;
    const std::uint64_t whole_seconds =
        input_clocks / PitChannel0::kInputClockHz;
    const std::uint64_t remaining_clocks =
        input_clocks % PitChannel0::kInputClockHz;
    const std::uint64_t numerator =
        remaining_clocks * kNanosecondsPerSecond;
    return whole_seconds * kNanosecondsPerSecond +
        (numerator + PitChannel0::kInputClockHz - 1U) /
            PitChannel0::kInputClockHz;
}

double PitFrequencyHz(std::uint32_t divisor)
{
    if (divisor == 0U)
    {
        divisor = PitChannel0::kDefaultDivisor;
    }
    return static_cast<double>(PitChannel0::kInputClockHz) /
        static_cast<double>(divisor);
}

}  // namespace repiu::hle
