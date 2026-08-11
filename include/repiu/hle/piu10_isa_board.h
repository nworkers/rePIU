#ifndef REPIU_HLE_PIU10_ISA_BOARD_H_
#define REPIU_HLE_PIU10_ISA_BOARD_H_

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "repiu/sound/dac3350a_control.h"

namespace repiu::hle
{

class Piu10IsaBoard
{
public:
    static constexpr std::size_t kFlashBytes = 2U * 1024U * 1024U;
    static constexpr std::size_t kCat702TransformBytes = 8U;
    using Cat702Transform =
        std::array<std::uint8_t, kCat702TransformBytes>;

    bool Initialize(std::vector<std::uint8_t> flash,
                    const std::optional<Cat702Transform>& cat702_transform,
                    std::string* message = nullptr);
    void Reset();
    void SetMp3DataSink(std::function<void(std::uint8_t)> sink);
    void SetMp3StatusSource(std::function<std::uint8_t()> source);
    void SetDacControlSink(
        std::function<void(const sound::Dac3350aControlEvent&)> sink);

    bool available() const { return available_; }
    bool cat702_enabled() const { return cat702_enabled_; }
    bool Read8(std::uint16_t port, std::uint8_t* value);
    bool Read16(std::uint16_t port, std::uint16_t* value);
    bool Write8(std::uint16_t port, std::uint8_t value);
    bool Write16(std::uint16_t port, std::uint16_t value);

    std::uint32_t address() const { return address_; }
    std::uint16_t destination() const { return destination_; }

private:
    class Cat702Piu
    {
    public:
        void Configure(const Cat702Transform& transform);
        void Reset();
        void WriteData(int state);
        void WriteSelect(int state);
        void WriteClock(int state);
        std::uint8_t data_out() const { return data_out_; }

    private:
        std::uint8_t ComputeSboxCoefficient(int select, int bit) const;
        void ApplyBitSbox(int select);
        void ApplySbox(const std::array<std::uint8_t, 8>& sbox);

        Cat702Transform transform_ = {};
        int select_ = 1;
        int clock_ = 1;
        int data_in_ = 1;
        std::uint8_t state_ = 0;
        std::uint8_t bit_ = 0;
        std::uint8_t data_out_ = 1;
    };

    std::uint16_t ReadFlashWord(std::uint32_t address) const;

    std::vector<std::uint8_t> flash_;
    Cat702Piu cat702_;
    sound::Dac3350aControl dac3350a_;
    std::uint32_t address_ = 0;
    std::uint16_t destination_ = 0;
    bool flash_auto_increment_ = false;
    std::uint8_t mp3_frame_sync_ = 1;
    std::uint8_t mp3_demand_ = 1;
    bool available_ = false;
    bool cat702_enabled_ = false;
    std::function<void(std::uint8_t)> mp3_data_sink_;
    std::function<std::uint8_t()> mp3_status_source_;
    std::function<void(const sound::Dac3350aControlEvent&)>
        dac_control_sink_;
};

}  // namespace repiu::hle

#endif  // REPIU_HLE_PIU10_ISA_BOARD_H_
