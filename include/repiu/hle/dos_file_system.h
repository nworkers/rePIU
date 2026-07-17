#ifndef REPIU_HLE_DOS_FILE_SYSTEM_H_
#define REPIU_HLE_DOS_FILE_SYSTEM_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
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
    std::uint64_t file_offset = 0;
    std::string guest_path;
    std::filesystem::path host_path;
    std::string dos_path;
};

struct DosVirtualFileSystemState
{
    bool valid = false;
    std::filesystem::path host_root;
    std::vector<std::string> current_components;
    std::vector<DosOpenFileHandle> open_files;
    std::vector<std::pair<std::string, std::uint16_t>> attribute_overrides;
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

bool InitializeDosVirtualFileSystem(
    const std::filesystem::path& host_root,
    const std::filesystem::path& initial_current_directory,
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

bool ReadDosFile(DosVirtualFileSystemState* state,
                 std::uint16_t handle,
                 std::uint32_t requested_bytes,
                 std::vector<std::uint8_t>* bytes,
                 std::uint32_t* actual_bytes,
                 std::uint16_t* dos_error);

bool SeekDosFile(DosVirtualFileSystemState* state,
                 std::uint16_t handle,
                 std::uint8_t origin,
                 std::int32_t offset,
                 std::uint32_t* new_position,
                 std::uint16_t* dos_error);

bool CloseDosFile(DosVirtualFileSystemState* state,
                  std::uint16_t handle,
                  std::uint16_t* dos_error);

bool IsDosFileHandleOpen(const DosVirtualFileSystemState& state,
                         std::uint16_t handle);

bool QueryDosFileAttributes(DosVirtualFileSystemState* state,
                            const std::string& guest_path,
                            DosResolvedPath* resolved,
                            std::uint16_t* attributes);

bool SetDosFileAttributes(DosVirtualFileSystemState* state,
                          const std::string& guest_path,
                          std::uint16_t attributes,
                          DosResolvedPath* resolved);

std::uint16_t DosPathResultToErrorCode(DosPathResult result);

}  // namespace repiu::hle

#endif  // REPIU_HLE_DOS_FILE_SYSTEM_H_
