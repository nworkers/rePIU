#ifndef REPIU_MEDIA_CHD_CD_IMAGE_H_
#define REPIU_MEDIA_CHD_CD_IMAGE_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace repiu::media
{

// A CHD stores each track padded to a 4-frame boundary, so the frame index
// inside the file is not the LBA the disc would report. Every address a guest
// sees is a logical (Red Book) LBA; only ReadRawSector translates to physical.
struct ChdCdTrack
{
    std::uint8_t number = 0;
    bool audio = false;

    // Physical: first frame of this track's extent inside the CHD.
    std::uint32_t physical_lba = 0;
    std::uint32_t stored_frames = 0;

    // Logical: what the guest TOC reports.
    std::uint32_t logical_lba = 0;   // track start, INDEX 00
    std::uint32_t data_start_lba = 0;  // first logical frame backed by the file
    std::uint32_t start_lba = 0;     // INDEX 01, the audible start
    std::uint32_t end_lba = 0;       // one past the track's last logical frame

    std::uint32_t pregap_frames = 0;
    // CHT2 PGTYPE starting with 'V' means the pregap frames are physically
    // stored inside this track's FRAMES extent. Otherwise the pregap is only
    // a logical gap that the writer never stored.
    bool pregap_in_file = false;
    // Raw CHT2/CHTR metadata string, retained for diagnostics.
    std::string metadata;
};

class ChdCdImage
{
public:
    ChdCdImage();
    ~ChdCdImage();
    ChdCdImage(ChdCdImage&&) noexcept;
    ChdCdImage& operator=(ChdCdImage&&) noexcept;
    ChdCdImage(const ChdCdImage&) = delete;
    ChdCdImage& operator=(const ChdCdImage&) = delete;

    bool Open(const std::filesystem::path& path);
    void Close();
    // Takes a logical (Red Book) LBA. Frames inside a pregap the writer never
    // stored have no backing data and are returned as digital silence.
    bool ReadRawSector(std::uint32_t lba, void* output,
                       std::uint32_t output_bytes);
    const std::vector<ChdCdTrack>& tracks() const;
    const ChdCdTrack* FindTrack(std::uint8_t number) const;
    const ChdCdTrack* FindTrackByLba(std::uint32_t lba) const;
    std::uint32_t lead_out_lba() const;
    const std::string& identity() const;
    const std::string& message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace repiu::media

#endif  // REPIU_MEDIA_CHD_CD_IMAGE_H_
