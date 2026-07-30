#include "repiu/platform/win32/aot_boundary_opcode_census.h"

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint8_t kTwoByteEscape = 0x0FU;

bool IsSegmentPrefix(std::uint8_t value)
{
    return value == 0x26U || value == 0x2EU || value == 0x36U ||
        value == 0x3EU || value == 0x64U || value == 0x65U;
}

bool IsRepeatPrefix(std::uint8_t value)
{
    return value == 0xF2U || value == 0xF3U;
}

}  // namespace

bool IsX86LegacyPrefix(std::uint8_t value)
{
    return IsSegmentPrefix(value) || IsRepeatPrefix(value) ||
        value == 0x66U || value == 0x67U || value == 0xF0U;
}

void RecordAotBoundaryOpcodeSample(Win32AotBoundaryOpcodeCensus* census,
                                   const std::uint8_t* bytes,
                                   std::size_t length)
{
    if (census == nullptr)
    {
        return;
    }
    if (bytes == nullptr || length == 0U)
    {
        ++census->empty_sample_count;
        return;
    }

    // Every sample is counted exactly once, which is what lets this histogram be
    // checked against the existing `bytes[0]` one.
    ++census->sample_count;

    std::size_t index = 0;
    std::uint32_t prefix_count = 0;
    bool saw_segment = false;
    bool saw_operand_size = false;
    bool saw_address_size = false;
    bool saw_repeat = false;
    bool saw_lock = false;
    while (index < length && IsX86LegacyPrefix(bytes[index]))
    {
        if (prefix_count >= kWin32AotMaxLegacyPrefixes)
        {
            ++census->prefix_overflow_count;
            break;
        }
        const std::uint8_t prefix = bytes[index];
        saw_segment = saw_segment || IsSegmentPrefix(prefix);
        saw_repeat = saw_repeat || IsRepeatPrefix(prefix);
        saw_operand_size = saw_operand_size || prefix == 0x66U;
        saw_address_size = saw_address_size || prefix == 0x67U;
        saw_lock = saw_lock || prefix == 0xF0U;
        ++prefix_count;
        ++index;
    }

    if (prefix_count != 0U)
    {
        ++census->prefixed_count;
        if (saw_segment)
        {
            ++census->segment_prefixed_count;
        }
        if (saw_operand_size)
        {
            ++census->operand_size_prefixed_count;
        }
        if (saw_address_size)
        {
            ++census->address_size_prefixed_count;
        }
        if (saw_repeat)
        {
            ++census->repeat_prefixed_count;
        }
        if (saw_lock)
        {
            ++census->lock_prefixed_count;
        }
    }

    if (index >= length)
    {
        // Prefixes filled the captured bytes, so no opcode is visible.
        ++census->escape_truncated_count;
        return;
    }

    const std::uint8_t opcode = bytes[index];
    ++census->effective_opcode_counts[opcode];
    if (opcode != kTwoByteEscape)
    {
        return;
    }

    ++census->escape_count;
    if (index + 1U >= length)
    {
        ++census->escape_truncated_count;
        return;
    }
    ++census->escape_opcode_counts[bytes[index + 1U]];
}

void RankAotOpcodeHistogram(const std::uint32_t* counts,
                            Win32AotOpcodeRank* ranks,
                            std::size_t capacity)
{
    if (ranks == nullptr || capacity == 0U)
    {
        return;
    }
    for (std::size_t slot = 0; slot < capacity; ++slot)
    {
        ranks[slot] = Win32AotOpcodeRank{};
    }
    if (counts == nullptr)
    {
        return;
    }
    // Selection into a small fixed array: the histogram is 256 wide and this runs
    // once at exit, so a simple insertion keeps the code obvious.
    for (std::size_t opcode = 0; opcode < kWin32AotOpcodeHistogramSize;
         ++opcode)
    {
        const std::uint32_t count = counts[opcode];
        if (count == 0U)
        {
            continue;
        }
        for (std::size_t slot = 0; slot < capacity; ++slot)
        {
            if (count <= ranks[slot].count)
            {
                continue;
            }
            for (std::size_t shift = capacity - 1U; shift > slot; --shift)
            {
                ranks[shift] = ranks[shift - 1U];
            }
            ranks[slot].opcode = static_cast<std::uint8_t>(opcode);
            ranks[slot].count = count;
            break;
        }
    }
}

}  // namespace repiu::platform::win32
