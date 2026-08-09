#ifndef REPIU_SOUND_DAC3350A_CONTROL_H_
#define REPIU_SOUND_DAC3350A_CONTROL_H_

#include <cstddef>
#include <cstdint>
#include <optional>

namespace repiu::sound
{

struct Dac3350aControlEvent
{
    std::uint8_t subaddress = 0U;
    std::uint16_t data = 0U;
    std::size_t data_bytes = 0U;
    std::uint8_t left_volume = 0U;
    std::uint8_t right_volume = 0U;
    float left_gain = 0.0F;
    float right_gain = 0.0F;
    bool analog_volume = false;
    bool stereo_muted = false;
};

class Dac3350aControl
{
public:
    static float CalculateAnalogGain(std::uint8_t volume);

    void Reset();
    std::optional<Dac3350aControlEvent> WriteLines(bool data, bool clock);

private:
    void StartTransaction();
    std::optional<Dac3350aControlEvent> FinishTransaction();
    void ReceiveBit(bool value);

    bool data_ = true;
    bool clock_ = true;
    bool active_ = false;
    bool ack_clock_ = false;
    std::uint8_t current_byte_ = 0U;
    unsigned current_bits_ = 0U;
    std::uint8_t bytes_[4] = {};
    std::size_t byte_count_ = 0U;
};

}  // namespace repiu::sound

#endif  // REPIU_SOUND_DAC3350A_CONTROL_H_
