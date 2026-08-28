#include "piu10_sound_port.h"

namespace repiu::engine
{
namespace
{

// Undecoded lanes float high on the ISA bus, matching every other unmapped
// read in this address window.
constexpr std::uint8_t kUndecodedByte = 0xFFU;

// Chip offset for a decoded lane, or -1 when the lane is not decoded. Only even
// addresses inside the window carry data, because umask16(0x00ff) routes just
// the low byte of each 16-bit word.
int DecodeChipOffset(std::uint32_t address)
{
    if (address < kPiu10SoundPortBase || address > kPiu10SoundPortEnd)
    {
        return -1;
    }
    if ((address & 1U) != 0U)
    {
        return -1;
    }
    return static_cast<int>((address - kPiu10SoundPortBase) / 2U);
}

}  // namespace

bool IsPiu10SoundPort(std::uint16_t port)
{
    return port >= kPiu10SoundPortBase && port <= kPiu10SoundPortEnd;
}

void WritePiu10SoundPort(Ymz280bAudioOut* audio,
                         std::uint16_t port,
                         std::uint32_t width,
                         std::uint32_t value)
{
    if (audio == nullptr)
    {
        return;
    }
    for (std::uint32_t lane = 0; lane < width; ++lane)
    {
        const int offset = DecodeChipOffset(port + lane);
        if (offset < 0)
        {
            continue;
        }
        const auto byte = static_cast<std::uint8_t>((value >> (lane * 8)) & 0xFFU);
        if (offset == 0)
        {
            audio->WriteRegisterSelect(byte);
        }
        else
        {
            audio->WriteRegisterData(byte);
        }
    }
}

std::uint32_t ReadPiu10SoundPort(Ymz280bAudioOut* audio,
                                 std::uint16_t port,
                                 std::uint32_t width)
{
    std::uint32_t value = 0;
    for (std::uint32_t lane = 0; lane < width; ++lane)
    {
        std::uint8_t byte = kUndecodedByte;
        const int offset = DecodeChipOffset(port + lane);
        if (audio != nullptr && offset == 0)
        {
            byte = audio->ReadExternalMemory();
        }
        else if (audio != nullptr && offset == 1)
        {
            byte = audio->ReadStatus();
        }
        value |= static_cast<std::uint32_t>(byte) << (lane * 8);
    }
    return value;
}

}  // namespace repiu::engine
