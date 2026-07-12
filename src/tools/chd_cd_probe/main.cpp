#include "repiu/media/chd_cd_image.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: repiu_chd_cd_probe <cd.chd>\n";
        return 2;
    }
    repiu::media::ChdCdImage image;
    if (!image.Open(std::filesystem::path(argv[1])))
    {
        std::cerr << image.message() << "\n";
        return 1;
    }
    std::array<std::uint8_t, 2352> sector = {};
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
                  << " start=" << track.start_lba
                  << " end=" << track.end_lba
                  << " pregap=" << track.pregap_frames << "\n";
    }
    std::cout << "tracks=" << image.tracks().size()
              << " audio_tracks=" << audio_tracks
              << " lead_out=" << image.lead_out_lba() << "\n";
    return 0;
}
