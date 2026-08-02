#ifndef REPIU_ASSETS_PIU_CHD_MOUNT_H_
#define REPIU_ASSETS_PIU_CHD_MOUNT_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace repiu::assets
{

struct PiuChdMountResult
{
    bool valid = false;
    bool mounted = false;
    bool cache_reused = false;
    std::string rom_set_id;
    std::filesystem::path rom_zip_path;
    std::filesystem::path chd_path;
    std::filesystem::path mount_root;
    std::filesystem::path executable_path;
    std::uint32_t data_track_lba = 0;
    std::int64_t iso_extent_lba_bias = 0;
    std::uint32_t extracted_file_count = 0;
    std::uint32_t skipped_external_extent_file_count = 0;
    std::uint64_t extracted_byte_count = 0;
    std::string message;
};

bool PreparePiuChdMount(std::string_view rom_set_id,
                        const std::filesystem::path& roms_root,
                        const std::filesystem::path& cache_root,
                        PiuChdMountResult* result);

}  // namespace repiu::assets

#endif  // REPIU_ASSETS_PIU_CHD_MOUNT_H_
