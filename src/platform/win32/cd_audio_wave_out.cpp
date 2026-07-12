#include "repiu/platform/win32/cd_audio_wave_out.h"

#include "repiu/media/chd_cd_image.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace repiu::platform::win32
{
namespace
{
constexpr std::uint32_t kSectorBytes = 2352U;
constexpr std::uint32_t kSectorsPerBuffer = 8U;
constexpr std::size_t kBufferCount = 4U;
}

struct CdAudioWaveOut::Impl
{
    media::ChdCdImage image;
    HWAVEOUT wave = nullptr;
    std::thread worker;
    std::atomic<bool> shutdown{false};
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    std::atomic<std::uint32_t> current_lba{0};
    std::atomic<std::uint32_t> end_lba{0};
    std::array<std::vector<std::uint8_t>, kBufferCount> data;
    std::array<WAVEHDR, kBufferCount> headers = {};
    std::string message;
    std::mutex state_mutex;
};

CdAudioWaveOut::CdAudioWaveOut() : impl_(std::make_unique<Impl>()) {}
CdAudioWaveOut::~CdAudioWaveOut() { Close(); }

bool CdAudioWaveOut::Open(const std::filesystem::path& chd_path)
{
    Close();
    if (!impl_->image.Open(chd_path))
    {
        impl_->message = impl_->image.message();
        return false;
    }
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 44100;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    if (waveOutOpen(&impl_->wave, WAVE_MAPPER, &format, 0, 0,
                    CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        impl_->message = "waveOutOpen failed";
        impl_->image.Close();
        return false;
    }
    for (std::size_t index = 0; index < kBufferCount; ++index)
    {
        impl_->data[index].resize(kSectorBytes * kSectorsPerBuffer);
        impl_->headers[index].lpData = reinterpret_cast<LPSTR>(
            impl_->data[index].data());
        impl_->headers[index].dwBufferLength = 0;
        if (waveOutPrepareHeader(impl_->wave, &impl_->headers[index],
                                 sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            impl_->message = "waveOutPrepareHeader failed";
            Close();
            return false;
        }
    }
    impl_->shutdown = false;
    impl_->worker = std::thread([this]() {
        std::size_t buffer_index = 0;
        while (!impl_->shutdown.load())
        {
            if (!impl_->playing.load() || impl_->paused.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            WAVEHDR& header = impl_->headers[buffer_index];
            if ((header.dwFlags & WHDR_INQUEUE) != 0U)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                buffer_index = (buffer_index + 1U) % kBufferCount;
                continue;
            }
            const std::uint32_t begin = impl_->current_lba.load();
            if (begin >= impl_->end_lba.load())
            {
                impl_->playing = false;
                continue;
            }
            const std::uint32_t sectors = std::min(
                kSectorsPerBuffer, impl_->end_lba.load() - begin);
            bool read = true;
            for (std::uint32_t sector = 0; sector < sectors; ++sector)
            {
                std::uint8_t* pcm = impl_->data[buffer_index].data() +
                    static_cast<std::size_t>(sector) * kSectorBytes;
                if (!impl_->image.ReadRawSector(begin + sector, pcm,
                                                kSectorBytes))
                {
                    read = false;
                    break;
                }
                for (std::uint32_t byte = 0; byte < kSectorBytes; byte += 2U)
                {
                    std::swap(pcm[byte], pcm[byte + 1U]);
                }
            }
            if (!read)
            {
                impl_->message = impl_->image.message();
                impl_->playing = false;
                continue;
            }
            header.dwBufferLength = sectors * kSectorBytes;
            header.dwFlags &= ~WHDR_DONE;
            if (waveOutWrite(impl_->wave, &header,
                             sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            {
                impl_->message = "waveOutWrite failed";
                impl_->playing = false;
                continue;
            }
            impl_->current_lba = begin + sectors;
            buffer_index = (buffer_index + 1U) % kBufferCount;
        }
    });
    impl_->message = "Win32 CD-DA waveOut ready";
    return true;
}

bool CdAudioWaveOut::Play(std::uint32_t start_lba,
                          std::uint32_t frame_count)
{
    const media::ChdCdTrack* track = impl_->image.FindTrackByLba(start_lba);
    if (impl_->wave == nullptr || track == nullptr || !track->audio ||
        frame_count == 0U)
    {
        impl_->message = "CD-DA play range is not an audio track";
        return false;
    }
    waveOutReset(impl_->wave);
    impl_->current_lba = start_lba;
    impl_->end_lba = std::min(start_lba + frame_count, track->end_lba);
    impl_->paused = false;
    impl_->playing = true;
    impl_->message = "CD-DA playback started";
    return true;
}

void CdAudioWaveOut::Stop()
{
    impl_->playing = false;
    impl_->paused = false;
    if (impl_->wave != nullptr)
    {
        waveOutReset(impl_->wave);
    }
}

bool CdAudioWaveOut::Resume()
{
    if (impl_->wave == nullptr ||
        impl_->current_lba.load() >= impl_->end_lba.load())
    {
        return false;
    }
    impl_->paused = false;
    impl_->playing = true;
    return true;
}

void CdAudioWaveOut::Close()
{
    impl_->shutdown = true;
    impl_->playing = false;
    if (impl_->wave != nullptr)
    {
        waveOutReset(impl_->wave);
    }
    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }
    if (impl_->wave != nullptr)
    {
        for (WAVEHDR& header : impl_->headers)
        {
            if ((header.dwFlags & WHDR_PREPARED) != 0U)
            {
                waveOutUnprepareHeader(impl_->wave, &header, sizeof(WAVEHDR));
            }
        }
        waveOutClose(impl_->wave);
        impl_->wave = nullptr;
    }
    impl_->image.Close();
}

bool CdAudioWaveOut::playing() const { return impl_->playing.load(); }
bool CdAudioWaveOut::paused() const { return impl_->paused.load(); }
std::uint32_t CdAudioWaveOut::current_lba() const
{
    return impl_->current_lba.load();
}
const std::string& CdAudioWaveOut::message() const { return impl_->message; }

}  // namespace repiu::platform::win32
