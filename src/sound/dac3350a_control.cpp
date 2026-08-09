#include "repiu/sound/dac3350a_control.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace repiu::sound
{
namespace
{

constexpr std::uint8_t kWriteAddress = 0x9AU;
constexpr std::uint8_t kAnalogVolumeRegister = 0x02U;

}  // namespace

float Dac3350aControl::CalculateAnalogGain(std::uint8_t volume)
{
    const int value = static_cast<int>(volume & 0x3FU);
    if (value == 0)
    {
        return 0.0F;
    }
    double decibels = value <= 7
        ? -54.0 - static_cast<double>(8 - value) * 3.0
        : -54.0 + static_cast<double>(value - 8) * 1.5;
    decibels = std::clamp(decibels, -75.0, 18.0);
    return static_cast<float>(std::pow(10.0, decibels / 20.0));
}

void Dac3350aControl::Reset()
{
    data_ = true;
    clock_ = true;
    active_ = false;
    ack_clock_ = false;
    current_byte_ = 0U;
    current_bits_ = 0U;
    std::fill(std::begin(bytes_), std::end(bytes_), 0U);
    byte_count_ = 0U;
}

void Dac3350aControl::StartTransaction()
{
    active_ = true;
    ack_clock_ = false;
    current_byte_ = 0U;
    current_bits_ = 0U;
    std::fill(std::begin(bytes_), std::end(bytes_), 0U);
    byte_count_ = 0U;
}

std::optional<Dac3350aControlEvent>
Dac3350aControl::FinishTransaction()
{
    active_ = false;
    ack_clock_ = false;
    current_byte_ = 0U;
    current_bits_ = 0U;
    if (byte_count_ < 3U || bytes_[0] != kWriteAddress)
    {
        return std::nullopt;
    }

    Dac3350aControlEvent event;
    event.subaddress = bytes_[1];
    event.data_bytes = byte_count_ - 2U;
    event.data = bytes_[2];
    if (event.data_bytes >= 2U)
    {
        event.data = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes_[2]) << 8U) | bytes_[3]);
    }
    event.analog_volume =
        (event.subaddress & 0x03U) == kAnalogVolumeRegister &&
        event.data_bytes >= 2U;
    if (event.analog_volume)
    {
        event.left_volume = static_cast<std::uint8_t>(
            (event.data >> 8U) & 0x3FU);
        event.right_volume = static_cast<std::uint8_t>(event.data & 0x3FU);
        event.left_gain = CalculateAnalogGain(event.left_volume);
        event.right_gain = CalculateAnalogGain(event.right_volume);
        event.stereo_muted =
            event.left_volume == 0U && event.right_volume == 0U;
    }
    return event;
}

void Dac3350aControl::ReceiveBit(bool value)
{
    current_byte_ = static_cast<std::uint8_t>(
        (current_byte_ << 1U) | (value ? 1U : 0U));
    ++current_bits_;
    if (current_bits_ != 8U)
    {
        return;
    }
    if (byte_count_ < std::size(bytes_))
    {
        bytes_[byte_count_++] = current_byte_;
    }
    current_byte_ = 0U;
    current_bits_ = 0U;
    ack_clock_ = true;
}

std::optional<Dac3350aControlEvent> Dac3350aControl::WriteLines(
    bool data, bool clock)
{
    std::optional<Dac3350aControlEvent> event;
    if (clock_ && data_ && !data)
    {
        StartTransaction();
    }
    else if (clock_ && !data_ && data && active_)
    {
        event = FinishTransaction();
    }

    if (!clock_ && clock && active_)
    {
        if (ack_clock_)
        {
            ack_clock_ = false;
        }
        else
        {
            ReceiveBit(data);
        }
    }
    data_ = data;
    clock_ = clock;
    return event;
}

}  // namespace repiu::sound
