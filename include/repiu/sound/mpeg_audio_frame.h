#ifndef REPIU_SOUND_MPEG_AUDIO_FRAME_H_
#define REPIU_SOUND_MPEG_AUDIO_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <span>

namespace repiu::sound
{

struct MpegAudioFrameInfo
{
    std::size_t frame_bytes = 0;
    std::uint32_t sample_rate_hz = 0;
    std::uint32_t bitrate_kbps = 0;
    std::uint16_t samples_per_frame = 0;
    std::uint8_t channels = 0;
};

bool ParseMpegAudioFrameHeader(std::span<const std::uint8_t> bytes,
                               MpegAudioFrameInfo* info);

}  // namespace repiu::sound

#endif  // REPIU_SOUND_MPEG_AUDIO_FRAME_H_
