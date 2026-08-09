#ifndef REPIU_SOUND_SPSC_BYTE_RING_H_
#define REPIU_SOUND_SPSC_BYTE_RING_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace repiu::sound
{

class SpscByteRing
{
public:
    explicit SpscByteRing(std::size_t capacity);

    SpscByteRing(const SpscByteRing&) = delete;
    SpscByteRing& operator=(const SpscByteRing&) = delete;

    bool Push(std::uint8_t value);
    std::size_t Push(std::span<const std::uint8_t> input);
    std::size_t Pop(std::span<std::uint8_t> output);
    void Reset();

    std::size_t size() const;
    std::size_t capacity() const { return capacity_; }
    std::size_t high_water() const { return high_water_.load(); }

private:
    std::unique_ptr<std::uint8_t[]> bytes_;
    std::size_t capacity_ = 0;
    std::size_t mask_ = 0;
    alignas(64) std::atomic<std::uint64_t> read_position_{0};
    alignas(64) std::atomic<std::uint64_t> write_position_{0};
    std::atomic<std::size_t> high_water_{0};
};

}  // namespace repiu::sound

#endif  // REPIU_SOUND_SPSC_BYTE_RING_H_
