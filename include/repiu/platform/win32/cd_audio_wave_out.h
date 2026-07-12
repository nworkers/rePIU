#ifndef REPIU_PLATFORM_WIN32_CD_AUDIO_WAVE_OUT_H_
#define REPIU_PLATFORM_WIN32_CD_AUDIO_WAVE_OUT_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace repiu::platform::win32
{

class CdAudioWaveOut
{
public:
    CdAudioWaveOut();
    ~CdAudioWaveOut();
    CdAudioWaveOut(const CdAudioWaveOut&) = delete;
    CdAudioWaveOut& operator=(const CdAudioWaveOut&) = delete;

    bool Open(const std::filesystem::path& chd_path);
    bool Play(std::uint32_t start_lba, std::uint32_t frame_count);
    void Stop();
    bool Resume();
    void Close();
    bool playing() const;
    bool paused() const;
    std::uint32_t current_lba() const;
    const std::string& message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_CD_AUDIO_WAVE_OUT_H_
