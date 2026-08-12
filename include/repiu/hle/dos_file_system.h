#ifndef REPIU_HLE_DOS_FILE_SYSTEM_H_
#define REPIU_HLE_DOS_FILE_SYSTEM_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
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

// Task 374: a host stream kept open for as long as the DOS handle lives. Before
// this, every read reopened the file -- a stat, an ifstream construction, a seek,
// the read, and a close -- which measured at 7.0 ms per 4 KB read and 83.5 ms at
// worst, because a Windows CreateFile traverses the filter driver stack including
// real-time scanning. Real DOS keeps the handle open across reads.
//
// The stream is a cache, not state. Copying starts cold rather than sharing,
// because two handles sharing one stream would share a file position; and the
// state is copy-assigned into the guest thread context, so copyability is
// mandatory rather than optional.
class DosHostFileCache
{
public:
    DosHostFileCache() = default;
    ~DosHostFileCache() = default;
    DosHostFileCache(const DosHostFileCache&) noexcept {}
    DosHostFileCache& operator=(const DosHostFileCache&) noexcept
    {
        Reset();
        return *this;
    }
    DosHostFileCache(DosHostFileCache&&) noexcept = default;
    DosHostFileCache& operator=(DosHostFileCache&&) noexcept = default;

    // Returns a stream positioned at `offset`, opening the file only when the
    // cache is cold. Null when the file cannot be opened, which the caller maps
    // to the same DOS error it reported before.
    std::ifstream* Acquire(const std::filesystem::path& path,
                           std::uint64_t offset,
                           bool* opened_now);

    void Reset();
    bool warm() const { return stream_ != nullptr; }

private:
    std::unique_ptr<std::ifstream> stream_;
};

// Task 477: the write-side counterpart of DosHostFileCache, kept for the same
// reason -- reopening the host file per write costs what Task 374 measured for
// reads. Copy semantics match: a copy starts cold rather than sharing a stream,
// because two handles sharing one would share a file position.
class DosHostWriteCache
{
public:
    DosHostWriteCache() = default;
    ~DosHostWriteCache() = default;
    DosHostWriteCache(const DosHostWriteCache&) noexcept {}
    DosHostWriteCache& operator=(const DosHostWriteCache&) noexcept
    {
        Reset();
        return *this;
    }
    DosHostWriteCache(DosHostWriteCache&&) noexcept = default;
    DosHostWriteCache& operator=(DosHostWriteCache&&) noexcept = default;

    // Returns a stream positioned at `offset`, opening the file only when the
    // cache is cold. `truncate` empties the file, which is what creation means.
    // Null when the file cannot be opened.
    std::ofstream* Acquire(const std::filesystem::path& path,
                           std::uint64_t offset,
                           bool truncate,
                           bool* opened_now);

    void Reset();
    bool warm() const { return stream_ != nullptr; }

private:
    std::unique_ptr<std::ofstream> stream_;
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
    // Task 374: read once at open. Task 477 added the write path this note
    // anticipated, so WriteDosFile updates the value instead of leaving it stale.
    std::uint64_t cached_file_size = 0;
    // Task 477: only handles created by AH=3Ch, or opened with a write access
    // mode, accept writes. AH=40h routes on this.
    bool writable = false;
    DosHostFileCache host_stream;
    DosHostWriteCache host_write_stream;
};

struct DosVirtualFileSystemState
{
    bool valid = false;
    std::filesystem::path host_root;
    std::vector<std::string> current_components;
    std::vector<DosOpenFileHandle> open_files;
    std::vector<std::pair<std::string, std::uint16_t>> attribute_overrides;
    std::string message;
    // Task 374: the pair that makes the fix verifiable. These were equal before
    // the change -- one host open per read -- and afterwards opens should fall to
    // roughly one per file.
    std::uint32_t file_read_count = 0;
    std::uint32_t host_file_open_count = 0;
    std::uint32_t file_create_count = 0;
    std::uint32_t file_write_count = 0;
    std::uint64_t file_written_bytes = 0;
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

// INT 21h AH=3Ch. Unlike OpenDosFile the file need not exist -- only its parent
// directory must -- and an existing file is truncated to zero. `attributes` is
// recorded in the state's attribute overrides rather than applied to the host
// file: DOS keeps writing through the handle it just returned, while a host
// read-only attribute would block the next write.
bool CreateDosFile(DosVirtualFileSystemState* state,
                   const std::string& guest_path,
                   std::uint16_t attributes,
                   DosResolvedPath* resolved,
                   std::uint16_t* handle);

// INT 21h AH=40h against a writable handle. Advances the file offset and keeps
// `cached_file_size` in step.
bool WriteDosFile(DosVirtualFileSystemState* state,
                  std::uint16_t handle,
                  const std::uint8_t* bytes,
                  std::uint32_t byte_count,
                  std::uint32_t* actual_bytes,
                  std::uint16_t* dos_error);

// True when the handle is open and accepts writes, which is what AH=40h routes
// on. Standard handles 0-4 are never VFS handles, so they keep the console path.
bool IsDosFileHandleWritable(const DosVirtualFileSystemState& state,
                             std::uint16_t handle);

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
