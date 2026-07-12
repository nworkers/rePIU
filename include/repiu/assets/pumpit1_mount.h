#ifndef REPIU_ASSETS_PUMPIT1_MOUNT_H_
#define REPIU_ASSETS_PUMPIT1_MOUNT_H_

#include <cstdint>
#include <filesystem>
#include <string>

namespace repiu::assets
{

struct PumpIt1MountResult
{
    bool valid = false;
    bool mounted = false;
    bool cache_reused = false;
    std::filesystem::path rom_zip_path;
    std::filesystem::path chd_path;
    std::filesystem::path mount_root;
    std::filesystem::path executable_path;
    std::uint32_t extracted_file_count = 0;
    std::uint64_t extracted_byte_count = 0;
    std::string message;
};

bool PreparePumpIt1Mount(const std::filesystem::path& roms_root,
                         const std::filesystem::path& cache_root,
                         PumpIt1MountResult* result);

}  // namespace repiu::assets

#endif  // REPIU_ASSETS_PUMPIT1_MOUNT_H_
