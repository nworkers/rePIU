#include "repiu/hle/piu10_isa_board.h"

// CAT702 PIU state transitions and PIU10 register semantics are adapted from
// MAME's BSD-3-Clause xtom3d_piu10.cpp and cat702.cpp implementations.

#include <utility>

namespace repiu::hle
{
namespace
{

constexpr std::array<std::uint8_t, 8> kInitialSbox = {
    0xFFU, 0xFEU, 0xFCU, 0xF8U, 0xF0U, 0xE0U, 0xC0U, 0x7FU};

int Bit(std::uint32_t value, unsigned index)
{
    return static_cast<int>((value >> index) & 1U);
}

}  // namespace

void Piu10IsaBoard::Cat702Piu::Configure(
    const std::array<std::uint8_t, kCat702TransformBytes>& transform)
{
    transform_ = transform;
    Reset();
}

void Piu10IsaBoard::Cat702Piu::Reset()
{
    select_ = 1;
    clock_ = 1;
    data_in_ = 1;
    state_ = 0;
    bit_ = 0;
    data_out_ = 1;
}

std::uint8_t Piu10IsaBoard::Cat702Piu::ComputeSboxCoefficient(
    int select, int bit) const
{
    if (select == 0)
    {
        return transform_[static_cast<std::size_t>(bit)];
    }

    std::uint8_t result = ComputeSboxCoefficient(
        (select - 1) & 7, (bit - 1) & 7);
    result = static_cast<std::uint8_t>(
        (result << 1U) | (Bit(result, 7) ^ Bit(result, 6)));
    if (bit != 7)
    {
        return result;
    }
    return static_cast<std::uint8_t>(
        result ^ ComputeSboxCoefficient(select, 0));
}

void Piu10IsaBoard::Cat702Piu::ApplyBitSbox(int select)
{
    std::uint8_t result = 0;
    for (int index = 0; index < 8; ++index)
    {
        if (Bit(state_, static_cast<unsigned>(index)) != 0)
        {
            result ^= ComputeSboxCoefficient(select, index);
        }
    }
    state_ = result;
}

void Piu10IsaBoard::Cat702Piu::ApplySbox(
    const std::array<std::uint8_t, 8>& sbox)
{
    std::uint8_t result = 0;
    for (int index = 0; index < 8; ++index)
    {
        if (Bit(state_, static_cast<unsigned>(index)) != 0)
        {
            result ^= sbox[static_cast<std::size_t>(index)];
        }
    }
    state_ = result;
}

void Piu10IsaBoard::Cat702Piu::WriteData(int state)
{
    data_in_ = state != 0 ? 1 : 0;
}

void Piu10IsaBoard::Cat702Piu::WriteSelect(int state)
{
    state = state != 0 ? 1 : 0;
    if (select_ == state)
    {
        return;
    }
    if (state == 0)
    {
        state_ = 0xFCU;
        bit_ = 0;
        ApplySbox(kInitialSbox);
    }
    else
    {
        data_out_ = 1;
    }
    select_ = state;
}

void Piu10IsaBoard::Cat702Piu::WriteClock(int state)
{
    state = state != 0 ? 1 : 0;
    if (state != 0 && clock_ == 0 && select_ == 0)
    {
        if (data_in_ == 0)
        {
            ApplyBitSbox(bit_);
        }
        bit_ = static_cast<std::uint8_t>((bit_ + 1U) & 7U);
        if (bit_ == 0)
        {
            ApplySbox(kInitialSbox);
        }
        data_out_ = static_cast<std::uint8_t>(Bit(state_, bit_));
    }
    clock_ = state;
}

bool Piu10IsaBoard::Initialize(
    std::vector<std::uint8_t> flash,
    const std::array<std::uint8_t, kCat702TransformBytes>& cat702_transform,
    std::string* message)
{
    available_ = false;
    flash_.clear();
    if (flash.size() != kFlashBytes)
    {
        if (message != nullptr)
        {
            *message = "PIU10 flash image must contain exactly 2097152 bytes";
        }
        return false;
    }

    flash_ = std::move(flash);
    cat702_.Configure(cat702_transform);
    available_ = true;
    Reset();
    if (message != nullptr)
    {
        *message = "PIU10 ISA board initialized";
    }
    return true;
}

void Piu10IsaBoard::Reset()
{
    address_ = 0;
    destination_ = 0;
    flash_auto_increment_ = false;
    mp3_frame_sync_ = 1;
    mp3_demand_ = 1;
    cat702_.Reset();
    dac3350a_.Reset();
}

void Piu10IsaBoard::SetMp3DataSink(
    std::function<void(std::uint8_t)> sink)
{
    mp3_data_sink_ = std::move(sink);
}

void Piu10IsaBoard::SetMp3StatusSource(
    std::function<std::uint8_t()> source)
{
    mp3_status_source_ = std::move(source);
}

void Piu10IsaBoard::SetDacControlSink(
    std::function<void(const sound::Dac3350aControlEvent&)> sink)
{
    dac_control_sink_ = std::move(sink);
}

namespace
{

void WriteDacControl(
    sound::Dac3350aControl* control,
    const std::function<void(const sound::Dac3350aControlEvent&)>& sink,
    std::uint8_t value)
{
    const std::optional<sound::Dac3350aControlEvent> event =
        control->WriteLines(Bit(value, 1) != 0, Bit(value, 0) != 0);
    if (event && sink)
    {
        sink(*event);
    }
}

}  // namespace

std::uint16_t Piu10IsaBoard::ReadFlashWord(std::uint32_t address) const
{
    const std::size_t byte_offset = static_cast<std::size_t>(address) * 2U;
    if (byte_offset + 1U >= flash_.size())
    {
        return 0xFFFFU;
    }
    return static_cast<std::uint16_t>(
        flash_[byte_offset] |
        (static_cast<std::uint16_t>(flash_[byte_offset + 1U]) << 8U));
}

bool Piu10IsaBoard::Read16(std::uint16_t port, std::uint16_t* value)
{
    if (!available_ || value == nullptr || port != 0x02DAU)
    {
        return false;
    }

    if (destination_ == 0x008U)
    {
        const std::uint8_t mp3_status = mp3_status_source_
            ? static_cast<std::uint8_t>(mp3_status_source_() & 0x05U)
            : static_cast<std::uint8_t>(
                  (mp3_frame_sync_ << 2U) | mp3_demand_);
        *value = static_cast<std::uint16_t>(
            (cat702_.data_out() << 5U) |
            mp3_status | (1U << 1U));
        return true;
    }
    if (destination_ == 0U)
    {
        *value = ReadFlashWord(address_);
        if (flash_auto_increment_)
        {
            ++address_;
        }
        return true;
    }

    *value = 0;
    return true;
}

bool Piu10IsaBoard::Read8(std::uint16_t port, std::uint8_t* value)
{
    if (value == nullptr)
    {
        return false;
    }
    std::uint16_t word = 0;
    if (!Read16(port, &word))
    {
        return false;
    }
    *value = static_cast<std::uint8_t>(word & 0xFFU);
    return true;
}

bool Piu10IsaBoard::Write8(std::uint16_t port, std::uint8_t value)
{
    if (!available_ || port != 0x02DAU)
    {
        return false;
    }
    if (destination_ == 0x008U && mp3_data_sink_)
    {
        mp3_data_sink_(value);
    }
    else if (destination_ == 0x010U)
    {
        cat702_.WriteData(Bit(value, 5));
        cat702_.WriteClock(Bit(value, 4));
        cat702_.WriteSelect(Bit(value, 3));
        WriteDacControl(&dac3350a_, dac_control_sink_, value);
    }
    return true;
}

bool Piu10IsaBoard::Write16(std::uint16_t port, std::uint16_t value)
{
    if (!available_)
    {
        return false;
    }

    switch (port)
    {
        case 0x02D0U:
            address_ = (address_ & 0xFFF00U) | (value & 0x00FFU);
            return true;
        case 0x02D2U:
            address_ = (address_ & 0xF00FFU) |
                (static_cast<std::uint32_t>(value & 0x00FFU) << 8U);
            return true;
        case 0x02D4U:
            address_ = (address_ & 0x0FFFFU) |
                (static_cast<std::uint32_t>(value & 0x000FU) << 16U);
            destination_ = static_cast<std::uint16_t>(
                (destination_ & 0x0FF0U) | ((value >> 4U) & 0x000FU));
            return true;
        case 0x02D6U:
            destination_ = static_cast<std::uint16_t>(
                (destination_ & 0x000FU) | ((value & 0x00FFU) << 4U));
            return true;
        case 0x02DAU:
            if (destination_ == 0x008U && mp3_data_sink_)
            {
                mp3_data_sink_(static_cast<std::uint8_t>(value & 0xFFU));
            }
            else if (destination_ == 0x010U)
            {
                cat702_.WriteData(Bit(value, 5));
                cat702_.WriteClock(Bit(value, 4));
                cat702_.WriteSelect(Bit(value, 3));
                WriteDacControl(
                    &dac3350a_, dac_control_sink_,
                    static_cast<std::uint8_t>(value & 0xFFU));
            }
            return true;
        case 0x02DCU:
            flash_auto_increment_ = Bit(value, 3) != 0;
            return true;
        default:
            return false;
    }
}

}  // namespace repiu::hle
