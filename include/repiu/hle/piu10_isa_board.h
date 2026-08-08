#ifndef REPIU_HLE_PIU10_ISA_BOARD_H_
#define REPIU_HLE_PIU10_ISA_BOARD_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace repiu::hle
{

class Piu10IsaBoard
{
public:
    static constexpr std::size_t kFlashBytes = 2U * 1024U * 1024U;
    static constexpr std::size_t kCat702TransformBytes = 8U;

    bool Initialize(std::vector<std::uint8_t> flash,
                    const std::array<std::uint8_t,
                                     kCat702TransformBytes>& cat702_transform,
                    std::string* message = nullptr);
    void Reset();

    bool available() const { return available_; }
    bool Read16(std::uint16_t port, std::uint16_t* value);
    bool Write16(std::uint16_t port, std::uint16_t value);

    std::uint32_t address() const { return address_; }
    std::uint16_t destination() const { return destination_; }

private:
    class Cat702Piu
    {
    public:
        void Configure(const std::array<std::uint8_t,
                                        kCat702TransformBytes>& transform);
        void Reset();
        void WriteData(int state);
        void WriteSelect(int state);
        void WriteClock(int state);
        std::uint8_t data_out() const { return data_out_; }

    private:
        std::uint8_t ComputeSboxCoefficient(int select, int bit) const;
        void ApplyBitSbox(int select);
        void ApplySbox(const std::array<std::uint8_t, 8>& sbox);

        std::array<std::uint8_t, kCat702TransformBytes> transform_ = {};
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
    std::uint32_t address_ = 0;
    std::uint16_t destination_ = 0;
    bool flash_auto_increment_ = false;
    std::uint8_t mp3_frame_sync_ = 1;
    std::uint8_t mp3_demand_ = 1;
    bool available_ = false;
};

}  // namespace repiu::hle

#endif  // REPIU_HLE_PIU10_ISA_BOARD_H_
