#include "repiu/media/chd_cd_image.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

constexpr std::uint32_t kSectorBytes = 2352U;

// CD-DA sectors in a CHD are stored big-endian, so peak detection has to swap
// each sample pair before interpreting it as signed 16-bit PCM.
std::uint32_t SectorPeak(const std::uint8_t* sector)
{
    std::uint32_t peak = 0;
    for (std::uint32_t byte = 0; byte < kSectorBytes; byte += 2U)
    {
        const std::int16_t sample = static_cast<std::int16_t>(
            (static_cast<std::uint16_t>(sector[byte]) << 8) | sector[byte + 1U]);
        const std::uint32_t magnitude = static_cast<std::uint32_t>(
            sample < 0 ? -static_cast<std::int32_t>(sample) : sample);
        peak = magnitude > peak ? magnitude : peak;
    }
    return peak;
}

int PrintToc(repiu::media::ChdCdImage& image, bool with_metadata)
{
    std::array<std::uint8_t, kSectorBytes> sector = {};
    std::uint32_t audio_tracks = 0;
    for (const repiu::media::ChdCdTrack& track : image.tracks())
    {
        if (track.audio)
        {
            ++audio_tracks;
        }
        if (!image.ReadRawSector(track.start_lba, sector.data(),
                                 static_cast<std::uint32_t>(sector.size())))
        {
            std::cerr << "failed to read track "
                      << static_cast<unsigned>(track.number) << "\n";
            return 1;
        }
        std::cout << "track=" << static_cast<unsigned>(track.number)
                  << " type=" << (track.audio ? "audio" : "data")
                  << " phys=" << track.physical_lba
                  << " frames=" << track.stored_frames
                  << " logical=" << track.logical_lba
                  << " data_start=" << track.data_start_lba
                  << " start=" << track.start_lba
                  << " end=" << track.end_lba
                  << " pregap=" << track.pregap_frames
                  << " pregap_in_file=" << (track.pregap_in_file ? 1 : 0)
                  << "\n";
        if (with_metadata)
        {
            std::cout << "  meta: " << track.metadata << "\n";
        }
    }
    std::cout << "tracks=" << image.tracks().size()
              << " audio_tracks=" << audio_tracks
              << " lead_out=" << image.lead_out_lba() << "\n";
    return 0;
}

// Walks a track's stored extent and reports where audible content actually
// begins, which is what decides whether a reported pregap is really present.
int ScanTrack(repiu::media::ChdCdImage& image, std::uint8_t number,
              std::uint32_t sector_count, std::uint32_t silence_threshold)
{
    const repiu::media::ChdCdTrack* track = image.FindTrack(number);
    if (track == nullptr)
    {
        std::cerr << "track " << static_cast<unsigned>(number)
                  << " is not present\n";
        return 1;
    }
    std::array<std::uint8_t, kSectorBytes> sector = {};
    const std::uint32_t begin = track->logical_lba;
    std::uint32_t limit = begin + sector_count;
    limit = limit > track->end_lba ? track->end_lba : limit;
    std::uint32_t first_audible = UINT32_MAX;
    std::cout << "scan track=" << static_cast<unsigned>(number)
              << " phys=" << track->physical_lba
              << " logical=" << track->logical_lba
              << " data_start=" << track->data_start_lba
              << " start=" << track->start_lba
              << " end=" << track->end_lba
              << " pregap=" << track->pregap_frames
              << " pregap_in_file=" << (track->pregap_in_file ? 1 : 0) << "\n";
    for (std::uint32_t lba = begin; lba < limit; ++lba)
    {
        if (!image.ReadRawSector(lba, sector.data(),
                                 static_cast<std::uint32_t>(sector.size())))
        {
            std::cerr << "failed to read lba " << lba << "\n";
            return 1;
        }
        const std::uint32_t peak = SectorPeak(sector.data());
        if (peak > silence_threshold && first_audible == UINT32_MAX)
        {
            first_audible = lba;
        }
        std::cout << "  lba=" << lba
                  << " offset=" << (lba - begin)
                  << " peak=" << peak << "\n";
    }
    if (first_audible == UINT32_MAX)
    {
        std::cout << "first_audible=none within scanned window\n";
    }
    else
    {
        std::cout << "first_audible_lba=" << first_audible
                  << " offset_from_logical=" << (first_audible - begin)
                  << " offset_from_reported_start="
                  << static_cast<std::int64_t>(first_audible) -
                     static_cast<std::int64_t>(track->start_lba)
                  << "\n";
    }
    return 0;
}

void PrintUsage()
{
    std::cerr << "usage: repiu_chd_cd_probe <cd.chd> [--metadata]\n"
                 "       repiu_chd_cd_probe <cd.chd> --scan <track> "
                 "[sector_count] [silence_threshold]\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return 2;
    }
    repiu::media::ChdCdImage image;
    if (!image.Open(std::filesystem::path(argv[1])))
    {
        std::cerr << image.message() << "\n";
        return 1;
    }
    if (argc >= 3 && std::strcmp(argv[2], "--scan") == 0)
    {
        if (argc < 4)
        {
            PrintUsage();
            return 2;
        }
        const auto number = static_cast<std::uint8_t>(std::atoi(argv[3]));
        const std::uint32_t sector_count = argc >= 5
            ? static_cast<std::uint32_t>(std::atoi(argv[4])) : 300U;
        const std::uint32_t silence_threshold = argc >= 6
            ? static_cast<std::uint32_t>(std::atoi(argv[5])) : 64U;
        return ScanTrack(image, number, sector_count, silence_threshold);
    }
    const bool with_metadata =
        argc >= 3 && std::strcmp(argv[2], "--metadata") == 0;
    return PrintToc(image, with_metadata);
}
