#ifndef REPIU_PLATFORM_WIN32_CD_AUDIO_WAVE_OUT_H_
#define REPIU_PLATFORM_WIN32_CD_AUDIO_WAVE_OUT_H_

#include "repiu/platform/win32/cd_audio_position_census.h"

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
    // start_lba is a logical (Red Book) LBA, matching what the TOC reports.
    bool Play(std::uint32_t start_lba, std::uint32_t frame_count);
    // MSCDEX 85h: stops playback but remembers where it stopped so that a
    // following Resume continues from the same frame.
    void Stop();
    bool Resume();
    // MSCDEX 83h: park the head at a logical LBA without producing audio.
    bool Seek(std::uint32_t lba);
    void Close();
    bool playing() const;
    bool paused() const;
    // Position of the audio actually reaching the device, not the decode
    // cursor: queued-but-unplayed frames are subtracted.
    std::uint32_t current_lba() const;
    std::uint32_t last_play_start_lba() const;
    std::uint32_t last_play_end_lba() const;
    // Task 421: everything `current_lba()` is derived from, read together so a
    // wrong position can be attributed instead of guessed at. Safe to call from
    // another thread; every field is an atomic load.
    void FillPositionSample(Win32CdAudioPositionEntry* entry) const;
    const std::string& message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_CD_AUDIO_WAVE_OUT_H_
