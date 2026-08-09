#ifndef REPIU_SOUND_DECODER_INPUT_FIFO_H_
#define REPIU_SOUND_DECODER_INPUT_FIFO_H_

#include "repiu/sound/spsc_byte_ring.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace repiu::sound
{

class DecoderInputFifo
{
public:
    DecoderInputFifo(std::size_t logical_capacity,
                     std::size_t physical_capacity);

    DecoderInputFifo(const DecoderInputFifo&) = delete;
    DecoderInputFifo& operator=(const DecoderInputFifo&) = delete;

    bool PushByte(std::uint8_t value);
    std::size_t PushBatch(std::span<const std::uint8_t> input);
    std::size_t Pop(std::span<std::uint8_t> output);
    bool Consume(std::size_t count);
    void Reset();

    bool demand() const;
    std::size_t inflight_size() const;
    std::size_t inflight_high_water() const;
    std::size_t ring_size() const;
    std::size_t ring_high_water() const;
    std::size_t logical_capacity() const { return logical_capacity_; }
    std::size_t physical_capacity() const { return physical_capacity_; }

private:
    std::size_t Reserve(std::size_t requested, std::size_t limit);
    void Release(std::size_t count);
    void NoteHighWater(std::size_t size);

    SpscByteRing ring_;
    std::size_t logical_capacity_ = 0;
    std::size_t physical_capacity_ = 0;
    std::atomic<std::size_t> inflight_{0};
    std::atomic<std::size_t> inflight_high_water_{0};
};

}  // namespace repiu::sound

#endif  // REPIU_SOUND_DECODER_INPUT_FIFO_H_
