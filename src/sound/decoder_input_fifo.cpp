#include "repiu/sound/decoder_input_fifo.h"

#include <algorithm>
#include <stdexcept>

namespace repiu::sound
{

DecoderInputFifo::DecoderInputFifo(
    std::size_t logical_capacity, std::size_t physical_capacity)
    : ring_(physical_capacity), logical_capacity_(logical_capacity),
      physical_capacity_(physical_capacity)
{
    if (logical_capacity == 0U || logical_capacity > physical_capacity)
    {
        throw std::invalid_argument(
            "decoder FIFO logical capacity must fit its physical capacity");
    }
}

std::size_t DecoderInputFifo::Reserve(
    std::size_t requested, std::size_t limit)
{
    std::size_t current = inflight_.load(std::memory_order_acquire);
    while (current < limit)
    {
        const std::size_t accepted =
            std::min(requested, limit - current);
        if (inflight_.compare_exchange_weak(
                current, current + accepted,
                std::memory_order_acq_rel, std::memory_order_acquire))
        {
            NoteHighWater(current + accepted);
            return accepted;
        }
    }
    return 0U;
}

void DecoderInputFifo::Release(std::size_t count)
{
    inflight_.fetch_sub(count, std::memory_order_acq_rel);
}

void DecoderInputFifo::NoteHighWater(std::size_t size)
{
    std::size_t observed =
        inflight_high_water_.load(std::memory_order_relaxed);
    while (size > observed &&
           !inflight_high_water_.compare_exchange_weak(
               observed, size, std::memory_order_relaxed))
    {
    }
}

bool DecoderInputFifo::PushByte(std::uint8_t value)
{
    if (Reserve(1U, physical_capacity_) != 1U)
    {
        return false;
    }
    if (!ring_.Push(value))
    {
        Release(1U);
        return false;
    }
    return true;
}

std::size_t DecoderInputFifo::PushBatch(
    std::span<const std::uint8_t> input)
{
    if (input.empty())
    {
        return 0U;
    }
    const std::size_t reserved = Reserve(input.size(), logical_capacity_);
    if (reserved == 0U)
    {
        return 0U;
    }
    const std::size_t accepted = ring_.Push(input.first(reserved));
    if (accepted < reserved)
    {
        Release(reserved - accepted);
    }
    return accepted;
}

std::size_t DecoderInputFifo::Pop(std::span<std::uint8_t> output)
{
    return ring_.Pop(output);
}

bool DecoderInputFifo::Consume(std::size_t count)
{
    std::size_t current = inflight_.load(std::memory_order_acquire);
    while (count <= current)
    {
        if (inflight_.compare_exchange_weak(
                current, current - count,
                std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return true;
        }
    }
    return false;
}

void DecoderInputFifo::Reset()
{
    ring_.Reset();
    inflight_.store(0U, std::memory_order_relaxed);
    inflight_high_water_.store(0U, std::memory_order_relaxed);
}

bool DecoderInputFifo::demand() const
{
    return inflight_.load(std::memory_order_acquire) < logical_capacity_;
}

std::size_t DecoderInputFifo::inflight_size() const
{
    return inflight_.load(std::memory_order_acquire);
}

std::size_t DecoderInputFifo::inflight_high_water() const
{
    return inflight_high_water_.load(std::memory_order_relaxed);
}

std::size_t DecoderInputFifo::ring_size() const
{
    return ring_.size();
}

std::size_t DecoderInputFifo::ring_high_water() const
{
    return ring_.high_water();
}

}  // namespace repiu::sound
