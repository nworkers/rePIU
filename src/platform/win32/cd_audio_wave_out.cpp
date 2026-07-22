#include "repiu/platform/win32/cd_audio_wave_out.h"

#include "repiu/media/chd_cd_image.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace repiu::platform::win32
{
namespace
{
constexpr std::uint32_t kSectorBytes = 2352U;
constexpr std::uint32_t kSectorsPerBuffer = 8U;
constexpr std::uint32_t kQueuedBufferCount = 4U;
}

struct CdAudioWaveOut::Impl
{
    media::ChdCdImage image;
    SDL_AudioStream* stream = nullptr;
    std::thread worker;
    std::atomic<bool> shutdown{false};
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    std::atomic<std::uint32_t> current_lba{0};
    std::atomic<std::uint32_t> end_lba{0};
    std::string message;
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
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        impl_->message = std::string("SDL audio initialization failed: ") +
            SDL_GetError();
        impl_->image.Close();
        return false;
    }
    const SDL_AudioSpec format{SDL_AUDIO_S16LE, 2, 44100};
    impl_->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
    if (impl_->stream == nullptr)
    {
        impl_->message = std::string("SDL CD-DA stream creation failed: ") +
            SDL_GetError();
        impl_->image.Close();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    impl_->shutdown = false;
    impl_->worker = std::thread([this]() {
        std::vector<std::uint8_t> data(kSectorBytes * kSectorsPerBuffer);
        while (!impl_->shutdown.load())
        {
            if (!impl_->playing.load() || impl_->paused.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (SDL_GetAudioStreamQueued(impl_->stream) >=
                static_cast<int>(kSectorBytes * kSectorsPerBuffer *
                                 kQueuedBufferCount))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
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
                std::uint8_t* pcm = data.data() +
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
            if (!SDL_PutAudioStreamData(impl_->stream, data.data(),
                                        sectors * kSectorBytes))
            {
                impl_->message = std::string("SDL CD-DA queue failed: ") +
                    SDL_GetError();
                impl_->playing = false;
                continue;
            }
            impl_->current_lba = begin + sectors;
        }
    });
    impl_->message = "SDL3 CD-DA audio stream ready";
    return true;
}

bool CdAudioWaveOut::Play(std::uint32_t start_lba, std::uint32_t frame_count)
{
    const media::ChdCdTrack* track = impl_->image.FindTrackByLba(start_lba);
    if (impl_->stream == nullptr || track == nullptr || !track->audio ||
        frame_count == 0U)
    {
        impl_->message = "CD-DA play range is not an audio track";
        return false;
    }
    SDL_ClearAudioStream(impl_->stream);
    impl_->current_lba = start_lba;
    impl_->end_lba = std::min(start_lba + frame_count, track->end_lba);
    impl_->paused = false;
    impl_->playing = true;
    SDL_ResumeAudioStreamDevice(impl_->stream);
    impl_->message = "CD-DA playback started through SDL3";
    return true;
}

void CdAudioWaveOut::Stop()
{
    impl_->playing = false;
    impl_->paused = false;
    if (impl_->stream != nullptr)
    {
        SDL_ClearAudioStream(impl_->stream);
        SDL_PauseAudioStreamDevice(impl_->stream);
    }
}

bool CdAudioWaveOut::Resume()
{
    if (impl_->stream == nullptr ||
        impl_->current_lba.load() >= impl_->end_lba.load())
    {
        return false;
    }
    impl_->paused = false;
    impl_->playing = true;
    SDL_ResumeAudioStreamDevice(impl_->stream);
    return true;
}

void CdAudioWaveOut::Close()
{
    impl_->shutdown = true;
    impl_->playing = false;
    if (impl_->stream != nullptr)
    {
        SDL_ClearAudioStream(impl_->stream);
        SDL_PauseAudioStreamDevice(impl_->stream);
    }
    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }
    if (impl_->stream != nullptr)
    {
        SDL_DestroyAudioStream(impl_->stream);
        impl_->stream = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
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
