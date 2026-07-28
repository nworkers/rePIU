#ifndef REPIU_HLE_PIT_TIMER_H_
#define REPIU_HLE_PIT_TIMER_H_

#include <atomic>
#include <cstdint>

namespace repiu::hle
{

struct PitChannel0Snapshot
{
    std::uint32_t generation = 0;
    std::uint32_t divisor = 65536;
};

class PitChannel0
{
public:
    static constexpr std::uint32_t kInputClockHz = 1193280;
    static constexpr std::uint32_t kDefaultDivisor = 65536;

    PitChannel0();

    bool WriteControl(std::uint8_t value);
    bool WriteData(std::uint8_t value,
                   PitChannel0Snapshot* published_snapshot = nullptr);
    PitChannel0Snapshot snapshot() const;

private:
    void PublishDivisor(std::uint32_t divisor,
                        PitChannel0Snapshot* published_snapshot);

    std::atomic<std::uint64_t> configuration_;
    std::uint8_t access_mode_ = 3;
    std::uint8_t pending_low_byte_ = 0;
    bool awaiting_low_byte_ = true;
};

class PitIrqSchedule
{
public:
    std::uint64_t Poll(const PitChannel0Snapshot& snapshot,
                       std::uint64_t elapsed_nanoseconds);

private:
    std::uint32_t generation_ = 0;
    std::uint64_t epoch_nanoseconds_ = 0;
    std::uint64_t emitted_tick_count_ = 0;
    bool initialized_ = false;
};

std::uint64_t PitTickCountForElapsed(std::uint64_t elapsed_nanoseconds,
                                     std::uint32_t divisor);

double PitFrequencyHz(std::uint32_t divisor);

}  // namespace repiu::hle

#endif  // REPIU_HLE_PIT_TIMER_H_
