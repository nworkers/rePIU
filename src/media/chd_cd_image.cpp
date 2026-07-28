#include "repiu/media/chd_cd_image.h"

#include <libchdr/chd.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace repiu::media
{
namespace
{
constexpr std::uint32_t kRawSectorBytes = 2352U;
constexpr std::uint32_t kTrackPadding = 4U;

std::uint32_t RoundTrackFrames(std::uint32_t frames)
{
    return (frames + kTrackPadding - 1U) & ~(kTrackPadding - 1U);
}
}

struct ChdCdImage::Impl
{
    chd_file* file = nullptr;
    const chd_header* header = nullptr;
    std::vector<std::uint8_t> hunk;
    std::uint32_t frames_per_hunk = 0;
    std::uint32_t cached_hunk = UINT32_MAX;
    std::vector<ChdCdTrack> tracks;
    std::uint32_t lead_out_lba = 0;
    std::string message;
    std::mutex mutex;
};

ChdCdImage::ChdCdImage() : impl_(std::make_unique<Impl>()) {}
ChdCdImage::~ChdCdImage() { Close(); }
ChdCdImage::ChdCdImage(ChdCdImage&&) noexcept = default;
ChdCdImage& ChdCdImage::operator=(ChdCdImage&&) noexcept = default;

bool ChdCdImage::Open(const std::filesystem::path& path)
{
    Close();
    const chd_error error = chd_open(path.string().c_str(), CHD_OPEN_READ,
                                     nullptr, &impl_->file);
    if (error != CHDERR_NONE)
    {
        impl_->message = std::string("chd_open failed: ") +
                         chd_error_string(error);
        return false;
    }
    impl_->header = chd_get_header(impl_->file);
    if (impl_->header == nullptr || impl_->header->unitbytes < kRawSectorBytes ||
        impl_->header->hunkbytes == 0U ||
        impl_->header->hunkbytes % impl_->header->unitbytes != 0U)
    {
        impl_->message = "CHD is not a raw CD image";
        Close();
        return false;
    }
    impl_->frames_per_hunk = impl_->header->hunkbytes / impl_->header->unitbytes;
    impl_->hunk.resize(impl_->header->hunkbytes);

    std::uint32_t physical_lba = 0;
    std::uint32_t logical_lba = 0;
    for (std::uint32_t index = 0; index < 99U; ++index)
    {
        std::array<char, 256> metadata = {};
        chd_error metadata_error = chd_get_metadata(
            impl_->file, CDROM_TRACK_METADATA2_TAG, index, metadata.data(),
            static_cast<std::uint32_t>(metadata.size()), nullptr, nullptr,
            nullptr);
        int track = 0;
        int frames = 0;
        int pregap = 0;
        int postgap = 0;
        std::array<char, 32> type = {};
        std::array<char, 32> subtype = {};
        std::array<char, 32> pgtype = {};
        std::array<char, 32> pgsub = {};
        if (metadata_error == CHDERR_NONE)
        {
            if (std::sscanf(metadata.data(), CDROM_TRACK_METADATA2_FORMAT,
                            &track, type.data(), subtype.data(), &frames,
                            &pregap, pgtype.data(), pgsub.data(), &postgap) != 8)
            {
                impl_->message = "invalid CHT2 track metadata";
                Close();
                return false;
            }
        }
        else
        {
            metadata_error = chd_get_metadata(
                impl_->file, CDROM_TRACK_METADATA_TAG, index, metadata.data(),
                static_cast<std::uint32_t>(metadata.size()), nullptr, nullptr,
                nullptr);
            if (metadata_error != CHDERR_NONE)
            {
                break;
            }
            if (std::sscanf(metadata.data(), CDROM_TRACK_METADATA_FORMAT,
                            &track, type.data(), subtype.data(), &frames) != 4)
            {
                impl_->message = "invalid CHTR track metadata";
                Close();
                return false;
            }
        }
        if (track <= 0 || track > 99 || frames <= 0 || pregap < 0 ||
            pregap >= frames)
        {
            impl_->message = "invalid CHD track extent";
            Close();
            return false;
        }
        ChdCdTrack entry;
        entry.number = static_cast<std::uint8_t>(track);
        entry.audio = std::strcmp(type.data(), "AUDIO") == 0;
        entry.stored_frames = static_cast<std::uint32_t>(frames);
        entry.pregap_frames = static_cast<std::uint32_t>(pregap);
        entry.pregap_in_file = pgtype[0] == 'V';
        entry.metadata = metadata.data();

        entry.physical_lba = physical_lba;
        entry.logical_lba = logical_lba;
        // A stored pregap already occupies the head of FRAMES; an unstored one
        // only widens the logical extent ahead of the stored data.
        entry.data_start_lba = logical_lba +
            (entry.pregap_in_file ? 0U : entry.pregap_frames);
        entry.start_lba = logical_lba + entry.pregap_frames;
        entry.end_lba = entry.data_start_lba + entry.stored_frames;
        impl_->tracks.push_back(entry);

        logical_lba = entry.end_lba;
        physical_lba += RoundTrackFrames(entry.stored_frames);
    }
    if (impl_->tracks.empty())
    {
        impl_->message = "CHD contains no CD track metadata";
        Close();
        return false;
    }
    impl_->lead_out_lba = impl_->tracks.back().end_lba;
    impl_->message = "CHD CD track table ready";
    return true;
}

void ChdCdImage::Close()
{
    if (impl_ && impl_->file != nullptr)
    {
        chd_close(impl_->file);
    }
    if (impl_)
    {
        impl_->file = nullptr;
        impl_->header = nullptr;
        impl_->hunk.clear();
        impl_->tracks.clear();
        impl_->lead_out_lba = 0;
        impl_->cached_hunk = UINT32_MAX;
    }
}

bool ChdCdImage::ReadRawSector(std::uint32_t lba, void* output,
                               std::uint32_t output_bytes)
{
    if (output == nullptr || output_bytes < kRawSectorBytes ||
        impl_->file == nullptr || lba >= impl_->lead_out_lba)
    {
        return false;
    }
    const ChdCdTrack* track = FindTrackByLba(lba);
    if (track == nullptr)
    {
        return false;
    }
    if (lba < track->data_start_lba)
    {
        // Inside a pregap the writer never stored: the disc would play silence.
        std::memset(output, 0, kRawSectorBytes);
        return true;
    }
    const std::uint32_t physical =
        track->physical_lba + (lba - track->data_start_lba);

    std::lock_guard<std::mutex> lock(impl_->mutex);
    const std::uint32_t hunk_index = physical / impl_->frames_per_hunk;
    if (hunk_index != impl_->cached_hunk)
    {
        if (chd_read(impl_->file, hunk_index, impl_->hunk.data()) != CHDERR_NONE)
        {
            impl_->message = "failed to decompress CHD CD hunk";
            return false;
        }
        impl_->cached_hunk = hunk_index;
    }
    const std::uint32_t frame = physical % impl_->frames_per_hunk;
    const std::uint8_t* source = impl_->hunk.data() +
        static_cast<std::size_t>(frame) * impl_->header->unitbytes;
    std::memcpy(output, source, kRawSectorBytes);
    return true;
}

const std::vector<ChdCdTrack>& ChdCdImage::tracks() const { return impl_->tracks; }
const ChdCdTrack* ChdCdImage::FindTrack(std::uint8_t number) const
{
    const auto found = std::find_if(impl_->tracks.begin(), impl_->tracks.end(),
        [number](const ChdCdTrack& track) { return track.number == number; });
    return found == impl_->tracks.end() ? nullptr : &*found;
}
const ChdCdTrack* ChdCdImage::FindTrackByLba(std::uint32_t lba) const
{
    // Matches the full logical extent, so an address inside a track's pregap
    // resolves to that track the way a real drive's Q channel reports it.
    const auto found = std::find_if(impl_->tracks.begin(), impl_->tracks.end(),
        [lba](const ChdCdTrack& track) {
            return lba >= track.logical_lba && lba < track.end_lba;
        });
    return found == impl_->tracks.end() ? nullptr : &*found;
}
std::uint32_t ChdCdImage::lead_out_lba() const { return impl_->lead_out_lba; }
const std::string& ChdCdImage::message() const { return impl_->message; }

}  // namespace repiu::media
