#ifndef REPIU_HLE_DOS_FILE_SYSTEM_H_
#define REPIU_HLE_DOS_FILE_SYSTEM_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace repiu::hle
{

enum class DosPathResult
{
    kOk,
    kFileNotFound,
    kPathNotFound,
    kAccessDenied,
};

struct DosOpenFileHandle
{
    bool open = false;
    std::uint16_t handle = 0;
    std::uint8_t access_mode = 0;
    std::string guest_path;
    std::filesystem::path host_path;
    std::string dos_path;
};

struct DosVirtualFileSystemState
{
    bool valid = false;
    std::filesystem::path host_root;
    std::vector<std::string> current_components;
    std::uint16_t next_file_handle = 5;
    std::vector<DosOpenFileHandle> open_files;
    std::string message;
};

struct DosResolvedPath
{
    DosPathResult result = DosPathResult::kPathNotFound;
    std::string guest_path;
    std::filesystem::path host_path;
    std::string dos_path;
    std::string message;
};

bool InitializeDosVirtualFileSystem(
    const std::filesystem::path& host_root,
    DosVirtualFileSystemState* state);

bool ChangeDosCurrentDirectory(DosVirtualFileSystemState* state,
                               const std::string& guest_path,
                               DosResolvedPath* resolved);

std::string GetDosCurrentDirectory(const DosVirtualFileSystemState& state);

bool OpenDosFile(DosVirtualFileSystemState* state,
                 const std::string& guest_path,
                 std::uint8_t access_mode,
                 DosResolvedPath* resolved,
                 std::uint16_t* handle);

bool IsDosFileHandleOpen(const DosVirtualFileSystemState& state,
                         std::uint16_t handle);

std::uint16_t DosPathResultToErrorCode(DosPathResult result);

}  // namespace repiu::hle

#endif  // REPIU_HLE_DOS_FILE_SYSTEM_H_
