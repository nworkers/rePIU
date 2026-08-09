#include "repiu/sound/mpeg_audio_frame.h"

#include <array>

namespace repiu::sound
{
namespace
{

constexpr std::array<std::uint16_t, 14> kMpeg1Layer1Bitrates = {
    32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448};
constexpr std::array<std::uint16_t, 14> kMpeg1Layer2Bitrates = {
    32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
constexpr std::array<std::uint16_t, 14> kMpeg1Layer3Bitrates = {
    32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
constexpr std::array<std::uint16_t, 14> kMpeg2Layer1Bitrates = {
    32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
constexpr std::array<std::uint16_t, 14> kMpeg2Layer23Bitrates = {
    8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
constexpr std::array<std::uint32_t, 3> kMpeg1SampleRates = {
    44100, 48000, 32000};

std::uint32_t BitrateKbps(bool mpeg1, std::uint8_t layer,
                          std::uint8_t index)
{
    const std::size_t table_index = static_cast<std::size_t>(index - 1U);
    if (mpeg1)
    {
        if (layer == 3U)
        {
            return kMpeg1Layer1Bitrates[table_index];
        }
        if (layer == 2U)
        {
            return kMpeg1Layer2Bitrates[table_index];
        }
        return kMpeg1Layer3Bitrates[table_index];
    }
    return layer == 3U ? kMpeg2Layer1Bitrates[table_index]
                       : kMpeg2Layer23Bitrates[table_index];
}

}  // namespace

bool ParseMpegAudioFrameHeader(std::span<const std::uint8_t> bytes,
                               MpegAudioFrameInfo* info)
{
    if (bytes.size() < 4U || info == nullptr)
    {
        return false;
    }
    const std::uint32_t header =
        (static_cast<std::uint32_t>(bytes[0]) << 24U) |
        (static_cast<std::uint32_t>(bytes[1]) << 16U) |
        (static_cast<std::uint32_t>(bytes[2]) << 8U) |
        static_cast<std::uint32_t>(bytes[3]);
    if ((header & 0xFFE00000U) != 0xFFE00000U)
    {
        return false;
    }

    const std::uint8_t version = static_cast<std::uint8_t>((header >> 19U) & 3U);
    const std::uint8_t layer = static_cast<std::uint8_t>((header >> 17U) & 3U);
    const std::uint8_t bitrate_index =
        static_cast<std::uint8_t>((header >> 12U) & 0x0FU);
    const std::uint8_t sample_rate_index =
        static_cast<std::uint8_t>((header >> 10U) & 3U);
    if (version == 1U || layer == 0U || bitrate_index == 0U ||
        bitrate_index == 0x0FU || sample_rate_index == 3U)
    {
        return false;
    }

    const bool mpeg1 = version == 3U;
    std::uint32_t sample_rate = kMpeg1SampleRates[sample_rate_index];
    if (version == 2U)
    {
        sample_rate /= 2U;
    }
    else if (version == 0U)
    {
        sample_rate /= 4U;
    }
    const std::uint32_t bitrate = BitrateKbps(mpeg1, layer, bitrate_index);
    const std::uint32_t padding = (header >> 9U) & 1U;

    std::size_t frame_bytes = 0;
    if (layer == 3U)
    {
        frame_bytes = static_cast<std::size_t>(
            ((12U * bitrate * 1000U) / sample_rate + padding) * 4U);
    }
    else
    {
        const std::uint32_t coefficient = (!mpeg1 && layer == 1U) ? 72U : 144U;
        frame_bytes = static_cast<std::size_t>(
            (coefficient * bitrate * 1000U) / sample_rate + padding);
    }
    if (frame_bytes < 4U)
    {
        return false;
    }

    info->frame_bytes = frame_bytes;
    info->sample_rate_hz = sample_rate;
    info->bitrate_kbps = bitrate;
    info->samples_per_frame = layer == 3U ? 384U
        : ((layer == 1U && !mpeg1) ? 576U : 1152U);
    info->channels = ((header >> 6U) & 3U) == 3U ? 1U : 2U;
    return true;
}

}  // namespace repiu::sound
