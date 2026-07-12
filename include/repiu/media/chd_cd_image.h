#ifndef REPIU_MEDIA_CHD_CD_IMAGE_H_
#define REPIU_MEDIA_CHD_CD_IMAGE_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace repiu::media
{

struct ChdCdTrack
{
    std::uint8_t number = 0;
    bool audio = false;
    std::uint32_t storage_lba = 0;
    std::uint32_t start_lba = 0;
    std::uint32_t end_lba = 0;
    std::uint32_t pregap_frames = 0;
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
    bool ReadRawSector(std::uint32_t lba, void* output,
                       std::uint32_t output_bytes);
    const std::vector<ChdCdTrack>& tracks() const;
    const ChdCdTrack* FindTrack(std::uint8_t number) const;
    const ChdCdTrack* FindTrackByLba(std::uint32_t lba) const;
    std::uint32_t lead_out_lba() const;
    const std::string& message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace repiu::media

#endif  // REPIU_MEDIA_CHD_CD_IMAGE_H_
