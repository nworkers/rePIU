#include "repiu/sound/spsc_byte_ring.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace repiu::sound
{

SpscByteRing::SpscByteRing(std::size_t capacity)
    : bytes_(std::make_unique<std::uint8_t[]>(capacity)), capacity_(capacity),
      mask_(capacity - 1U)
{
    if (capacity < 2U || (capacity & (capacity - 1U)) != 0U)
    {
        throw std::invalid_argument("SPSC byte ring capacity must be a power of two");
    }
}

bool SpscByteRing::Push(std::uint8_t value)
{
    const std::uint64_t write =
        write_position_.load(std::memory_order_relaxed);
    const std::uint64_t read = read_position_.load(std::memory_order_acquire);
    if (write - read >= capacity_)
    {
        return false;
    }
    bytes_[static_cast<std::size_t>(write) & mask_] = value;
    write_position_.store(write + 1U, std::memory_order_release);

    const std::size_t used = static_cast<std::size_t>(write + 1U - read);
    std::size_t observed = high_water_.load(std::memory_order_relaxed);
    while (used > observed &&
           !high_water_.compare_exchange_weak(
               observed, used, std::memory_order_relaxed))
    {
    }
    return true;
}

std::size_t SpscByteRing::Push(std::span<const std::uint8_t> input)
{
    if (input.empty())
    {
        return 0U;
    }
    const std::uint64_t write =
        write_position_.load(std::memory_order_relaxed);
    const std::uint64_t read = read_position_.load(std::memory_order_acquire);
    const std::size_t available =
        capacity_ - std::min<std::size_t>(
            static_cast<std::size_t>(write - read), capacity_);
    const std::size_t count = std::min(available, input.size());
    const std::size_t start = static_cast<std::size_t>(write) & mask_;
    const std::size_t first = std::min(count, capacity_ - start);
    std::memcpy(bytes_.get() + start, input.data(), first);
    if (first < count)
    {
        std::memcpy(bytes_.get(), input.data() + first, count - first);
    }
    write_position_.store(write + count, std::memory_order_release);

    const std::size_t used = static_cast<std::size_t>(write + count - read);
    std::size_t observed = high_water_.load(std::memory_order_relaxed);
    while (used > observed &&
           !high_water_.compare_exchange_weak(
               observed, used, std::memory_order_relaxed))
    {
    }
    return count;
}

std::size_t SpscByteRing::Pop(std::span<std::uint8_t> output)
{
    if (output.empty())
    {
        return 0U;
    }
    const std::uint64_t read = read_position_.load(std::memory_order_relaxed);
    const std::uint64_t write =
        write_position_.load(std::memory_order_acquire);
    const std::size_t available = static_cast<std::size_t>(write - read);
    const std::size_t count = std::min(available, output.size());
    const std::size_t start = static_cast<std::size_t>(read) & mask_;
    const std::size_t first = std::min(count, capacity_ - start);
    std::memcpy(output.data(), bytes_.get() + start, first);
    if (first < count)
    {
        std::memcpy(output.data() + first, bytes_.get(), count - first);
    }
    read_position_.store(read + count, std::memory_order_release);
    return count;
}

void SpscByteRing::Reset()
{
    read_position_.store(0U, std::memory_order_relaxed);
    write_position_.store(0U, std::memory_order_relaxed);
    high_water_.store(0U, std::memory_order_relaxed);
}

std::size_t SpscByteRing::size() const
{
    const std::uint64_t read = read_position_.load(std::memory_order_acquire);
    const std::uint64_t write =
        write_position_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(write - read);
}

}  // namespace repiu::sound
