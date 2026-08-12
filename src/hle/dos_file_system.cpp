#include "repiu/hle/dos_file_system.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>

namespace repiu::hle
{
namespace
{

// DOS reserves standard handles 0-4 (stdin/stdout/stderr/stdaux/stdprn), so user file
// handles start at 5. The limit 20 is the DOS default Job File Table size and matches the
// guest clib's 20-entry handle-flags table that is indexed by the handle; handles >= 20
// overflow that table (Task 228).
constexpr std::uint16_t kFirstDosUserHandle = 5;
constexpr std::uint16_t kDosOpenHandleLimit = 20;

// Returns the lowest handle number not currently open, mirroring real DOS. 0 if exhausted.
std::uint16_t AllocateLowestFreeDosHandle(const DosVirtualFileSystemState& state)
{
    for (std::uint16_t candidate = kFirstDosUserHandle;
         candidate < kDosOpenHandleLimit; ++candidate)
    {
        bool in_use = false;
        for (const DosOpenFileHandle& open_file : state.open_files)
        {
            if (open_file.open && open_file.handle == candidate)
            {
                in_use = true;
                break;
            }
        }
        if (!in_use)
        {
            return candidate;
        }
    }
    return 0;
}

std::string ToUpperAscii(std::string value)
{
    for (char& ch : value)
    {
        ch = static_cast<char>(
            std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string BuildDosPath(const std::vector<std::string>& components)
{
    if (components.empty())
    {
        return "\\";
    }

    std::string result;
    for (const std::string& component : components)
    {
        result += "\\";
        result += component;
    }
    return result;
}

bool BuildCurrentComponentsFromHostPath(
    const std::filesystem::path& absolute_root,
    const std::filesystem::path& absolute_current_directory,
    std::vector<std::string>* components,
    std::string* message)
{
    if (components == nullptr || message == nullptr)
    {
        return false;
    }

    components->clear();
    const std::filesystem::path relative =
        absolute_current_directory.lexically_relative(absolute_root);
    if (relative.empty() &&
        absolute_current_directory != absolute_root)
    {
        *message = "DOS initial current directory is outside the host root";
        return true;
    }

    for (const std::filesystem::path& component : relative)
    {
        const std::string part = component.string();
        if (part.empty() || part == ".")
        {
            continue;
        }
        if (part == "..")
        {
            *message =
                "DOS initial current directory escapes the host root";
            components->clear();
            return true;
        }

        components->push_back(ToUpperAscii(part));
    }

    *message = "DOS initial current directory resolved";
    return true;
}

bool ResolveGuestPath(const DosVirtualFileSystemState& state,
                      const std::string& guest_path,
                      DosResolvedPath* resolved,
                      std::vector<std::string>* resolved_components)
{
    if (resolved == nullptr || resolved_components == nullptr)
    {
        return false;
    }

    *resolved = DosResolvedPath{};
    resolved->guest_path = guest_path;
    resolved_components->clear();

    if (!state.valid)
    {
        resolved->message = "DOS virtual filesystem state is not valid";
        return true;
    }

    std::string normalized = guest_path;
    for (char& ch : normalized)
    {
        if (ch == '/')
        {
            ch = '\\';
        }
    }

    if (normalized.empty())
    {
        resolved->message = "DOS path is empty";
        return true;
    }

    std::size_t start = 0;
    bool absolute = false;
    if (normalized.size() >= 2 && normalized[1] == ':')
    {
        start = 2;
        if (start < normalized.size() && normalized[start] == '\\')
        {
            absolute = true;
            ++start;
        }
    }
    else if (!normalized.empty() && normalized[0] == '\\')
    {
        absolute = true;
        start = 1;
    }

    std::vector<std::string> components =
        absolute ? std::vector<std::string>{}
                 : state.current_components;
    std::string part;
    for (std::size_t index = start; index <= normalized.size(); ++index)
    {
        const bool at_end = index == normalized.size();
        const char ch = at_end ? '\\' : normalized[index];
        if (ch != '\\')
        {
            part += ch;
            continue;
        }

        if (part.empty() || part == ".")
        {
            part.clear();
            continue;
        }
        if (part == "..")
        {
            if (components.empty())
            {
                resolved->result = DosPathResult::kAccessDenied;
                resolved->message =
                    "DOS path attempts to escape the target root";
                return true;
            }
            components.pop_back();
            part.clear();
            continue;
        }

        components.push_back(ToUpperAscii(part));
        part.clear();
    }

    std::filesystem::path host_path = state.host_root;
    for (const std::string& component : components)
    {
        host_path /= component;
    }

    *resolved_components = components;
    resolved->result = DosPathResult::kOk;
    resolved->host_path = host_path.lexically_normal();
    resolved->dos_path = BuildDosPath(components);
    resolved->message = "DOS path resolved";
    return true;
}

}  // namespace

bool InitializeDosVirtualFileSystem(
    const std::filesystem::path& host_root,
    DosVirtualFileSystemState* state)
{
    return InitializeDosVirtualFileSystem(host_root, host_root, state);
}

bool InitializeDosVirtualFileSystem(
    const std::filesystem::path& host_root,
    const std::filesystem::path& initial_current_directory,
    DosVirtualFileSystemState* state)
{
    if (state == nullptr)
    {
        return false;
    }

    *state = DosVirtualFileSystemState{};
    std::error_code error;
    const std::filesystem::path absolute_root =
        std::filesystem::absolute(host_root, error).lexically_normal();
    if (error)
    {
        state->message = "failed to resolve DOS host root";
        return true;
    }

    if (!std::filesystem::exists(absolute_root, error) || error ||
        !std::filesystem::is_directory(absolute_root, error) || error)
    {
        state->host_root = absolute_root;
        state->message = "DOS host root is not an existing directory";
        return true;
    }

    const std::filesystem::path absolute_current_directory =
        std::filesystem::absolute(initial_current_directory, error)
            .lexically_normal();
    if (error)
    {
        state->host_root = absolute_root;
        state->message = "failed to resolve DOS initial current directory";
        return true;
    }

    if (!std::filesystem::exists(absolute_current_directory, error) ||
        error ||
        !std::filesystem::is_directory(absolute_current_directory, error) ||
        error)
    {
        state->host_root = absolute_root;
        state->message =
            "DOS initial current directory is not an existing directory";
        return true;
    }

    std::vector<std::string> current_components;
    std::string current_message;
    if (!BuildCurrentComponentsFromHostPath(
            absolute_root,
            absolute_current_directory,
            &current_components,
            &current_message))
    {
        return false;
    }
    if (current_message != "DOS initial current directory resolved")
    {
        state->host_root = absolute_root;
        state->message = current_message;
        return true;
    }

    state->valid = true;
    state->host_root = absolute_root;
    state->current_components = current_components;
    state->message = "DOS virtual filesystem is ready";
    return true;
}

bool ChangeDosCurrentDirectory(DosVirtualFileSystemState* state,
                               const std::string& guest_path,
                               DosResolvedPath* resolved)
{
    if (state == nullptr || resolved == nullptr)
    {
        return false;
    }

    std::vector<std::string> components;
    if (!ResolveGuestPath(*state, guest_path, resolved, &components))
    {
        return false;
    }

    if (resolved->result != DosPathResult::kOk)
    {
        return true;
    }

    std::error_code error;
    if (!std::filesystem::exists(resolved->host_path, error) || error ||
        !std::filesystem::is_directory(resolved->host_path, error) || error)
    {
        resolved->result = DosPathResult::kPathNotFound;
        resolved->message = "DOS target directory was not found";
        return true;
    }

    state->current_components = components;
    state->message = "DOS current directory changed";
    resolved->message = "DOS current directory changed";
    return true;
}

std::string GetDosCurrentDirectory(const DosVirtualFileSystemState& state)
{
    std::string result;
    for (std::size_t index = 0; index < state.current_components.size();
         ++index)
    {
        if (index != 0)
        {
            result += "\\";
        }
        result += state.current_components[index];
    }
    return result;
}

bool OpenDosFile(DosVirtualFileSystemState* state,
                 const std::string& guest_path,
                 std::uint8_t access_mode,
                 DosResolvedPath* resolved,
                 std::uint16_t* handle)
{
    if (state == nullptr || resolved == nullptr || handle == nullptr)
    {
        return false;
    }

    *handle = 0;
    std::vector<std::string> components;
    if (!ResolveGuestPath(*state, guest_path, resolved, &components))
    {
        return false;
    }

    if (resolved->result != DosPathResult::kOk)
    {
        return true;
    }

    std::error_code error;
    if (!std::filesystem::exists(resolved->host_path, error) || error ||
        !std::filesystem::is_regular_file(resolved->host_path, error) ||
        error)
    {
        resolved->result = DosPathResult::kFileNotFound;
        resolved->message = "DOS target file was not found";
        return true;
    }

    // Real DOS returns the lowest free handle and recycles numbers on close; monotonic
    // growth would overflow the guest clib's 20-entry handle table (Task 228).
    const std::uint16_t allocated_handle = AllocateLowestFreeDosHandle(*state);
    if (allocated_handle == 0)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS file handle table is exhausted";
        return true;
    }

    DosOpenFileHandle new_handle{};
    new_handle.open = true;
    new_handle.handle = allocated_handle;
    new_handle.access_mode = access_mode;
    new_handle.file_offset = 0;
    new_handle.guest_path = guest_path;
    new_handle.host_path = resolved->host_path;
    new_handle.dos_path = resolved->dos_path;
    // Task 477: DOS access mode 1 is write-only and 2 read/write. Marking the
    // handle here is what lets a guest reopen a file it created and append.
    new_handle.writable = (access_mode & 0x07U) == 0x01U ||
        (access_mode & 0x07U) == 0x02U;
    // Task 374: the one stat this handle will ever do. Reads and SEEK_END use it
    // instead of re-stating the file every call.
    {
        std::error_code size_error;
        const std::uint64_t size =
            std::filesystem::file_size(resolved->host_path, size_error);
        new_handle.cached_file_size = size_error ? 0U : size;
    }
    // Reuse a closed slot if available to bound the growth of open_files.
    bool reused_slot = false;
    for (DosOpenFileHandle& slot : state->open_files)
    {
        if (!slot.open)
        {
            slot = std::move(new_handle);
            reused_slot = true;
            break;
        }
    }
    if (!reused_slot)
    {
        state->open_files.push_back(std::move(new_handle));
    }
    *handle = allocated_handle;
    resolved->message = "DOS file opened";
    state->message = "DOS file opened";
    return true;
}

bool CreateDosFile(DosVirtualFileSystemState* state,
                   const std::string& guest_path,
                   std::uint16_t attributes,
                   DosResolvedPath* resolved,
                   std::uint16_t* handle)
{
    if (state == nullptr || resolved == nullptr || handle == nullptr)
    {
        return false;
    }

    *handle = 0;
    std::vector<std::string> components;
    if (!ResolveGuestPath(*state, guest_path, resolved, &components))
    {
        return false;
    }

    if (resolved->result != DosPathResult::kOk)
    {
        return true;
    }

    if (components.empty())
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS create path names no file";
        return true;
    }

    // Only the parent must exist. Creating intermediate directories would
    // silently invent a layout the original image never had.
    std::error_code error;
    const std::filesystem::path parent = resolved->host_path.parent_path();
    if (!std::filesystem::is_directory(parent, error) || error)
    {
        resolved->result = DosPathResult::kPathNotFound;
        resolved->message = "DOS create parent directory was not found";
        return true;
    }
    if (std::filesystem::is_directory(resolved->host_path, error) && !error)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS create path names a directory";
        return true;
    }

    const std::uint16_t allocated_handle = AllocateLowestFreeDosHandle(*state);
    if (allocated_handle == 0)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS file handle table is exhausted";
        return true;
    }

    DosOpenFileHandle new_handle{};
    new_handle.open = true;
    new_handle.handle = allocated_handle;
    // Read/write: DOS hands back a handle the caller may also read through.
    new_handle.access_mode = 0x02U;
    new_handle.file_offset = 0;
    new_handle.guest_path = guest_path;
    new_handle.host_path = resolved->host_path;
    new_handle.dos_path = resolved->dos_path;
    new_handle.cached_file_size = 0;
    new_handle.writable = true;

    // Truncate now rather than at the first write, because creation is what
    // empties the file even if the guest never writes anything.
    bool opened_now = false;
    if (new_handle.host_write_stream.Acquire(new_handle.host_path, 0, true,
                                             &opened_now) == nullptr)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS create failed to open the host file";
        return true;
    }
    if (opened_now)
    {
        ++state->host_file_open_count;
    }

    // The attribute is guest-visible state, not a host file property. Stamping
    // FILE_ATTRIBUTE_READONLY here would block the writes this handle exists for.
    // The archive bit is what DOS sets on a freshly written file.
    constexpr std::uint16_t kMutableAttributes = 0x0027U;
    const std::uint16_t recorded_attributes =
        static_cast<std::uint16_t>((attributes & kMutableAttributes) | 0x0020U);
    bool attribute_recorded = false;
    for (auto& override_entry : state->attribute_overrides)
    {
        if (override_entry.first == resolved->dos_path)
        {
            override_entry.second = recorded_attributes;
            attribute_recorded = true;
            break;
        }
    }
    if (!attribute_recorded)
    {
        state->attribute_overrides.emplace_back(resolved->dos_path,
                                                recorded_attributes);
    }

    bool reused_slot = false;
    for (DosOpenFileHandle& slot : state->open_files)
    {
        if (!slot.open)
        {
            slot = std::move(new_handle);
            reused_slot = true;
            break;
        }
    }
    if (!reused_slot)
    {
        state->open_files.push_back(std::move(new_handle));
    }
    ++state->file_create_count;
    *handle = allocated_handle;
    resolved->message = "DOS file created";
    state->message = "DOS file created";
    return true;
}

bool IsDosFileHandleWritable(const DosVirtualFileSystemState& state,
                             std::uint16_t handle)
{
    for (const DosOpenFileHandle& candidate : state.open_files)
    {
        if (candidate.open && candidate.handle == handle)
        {
            return candidate.writable;
        }
    }
    return false;
}

bool WriteDosFile(DosVirtualFileSystemState* state,
                  std::uint16_t handle,
                  const std::uint8_t* bytes,
                  std::uint32_t byte_count,
                  std::uint32_t* actual_bytes,
                  std::uint16_t* dos_error)
{
    if (state == nullptr || actual_bytes == nullptr || dos_error == nullptr ||
        (bytes == nullptr && byte_count != 0U))
    {
        return false;
    }

    *actual_bytes = 0;
    *dos_error = 0;

    DosOpenFileHandle* open_file = nullptr;
    for (DosOpenFileHandle& candidate : state->open_files)
    {
        if (candidate.open && candidate.handle == handle)
        {
            open_file = &candidate;
            break;
        }
    }

    if (open_file == nullptr)
    {
        *dos_error = 0x0006;
        state->message = "DOS file write failed: invalid handle";
        return true;
    }
    if (!open_file->writable)
    {
        *dos_error = 0x0005;
        state->message = "DOS file write failed: handle is read-only";
        return true;
    }
    // DOS truncates the file at the current offset for a zero-length write. The
    // guests observed here never do it, so decline rather than implement a
    // destructive path from no evidence.
    if (byte_count == 0U)
    {
        state->message = "DOS file write completed with no bytes";
        return true;
    }

    bool opened_now = false;
    std::ofstream* stream = open_file->host_write_stream.Acquire(
        open_file->host_path, open_file->file_offset, false, &opened_now);
    if (opened_now)
    {
        ++state->host_file_open_count;
    }
    if (stream == nullptr)
    {
        *dos_error = open_file->host_write_stream.warm() ? 0x0005 : 0x0002;
        state->message = open_file->host_write_stream.warm()
            ? "DOS file write failed: seek failed"
            : "DOS file write failed: file could not be opened";
        return true;
    }

    stream->write(reinterpret_cast<const char*>(bytes),
                  static_cast<std::streamsize>(byte_count));
    stream->flush();
    if (!*stream)
    {
        *dos_error = 0x0005;
        state->message = "DOS file write failed";
        return true;
    }

    open_file->file_offset += byte_count;
    // Task 374 cached the size at open on the grounds that nothing wrote. This
    // is that invalidation point: a read on the same handle must see the growth.
    open_file->cached_file_size =
        (std::max)(open_file->cached_file_size, open_file->file_offset);
    open_file->host_stream.Reset();
    ++state->file_write_count;
    state->file_written_bytes += byte_count;
    *actual_bytes = byte_count;
    state->message = "DOS file write completed";
    return true;
}

std::ifstream* DosHostFileCache::Acquire(const std::filesystem::path& path,
                                         std::uint64_t offset,
                                         bool* opened_now)
{
    if (opened_now != nullptr)
    {
        *opened_now = false;
    }
    if (stream_ == nullptr)
    {
        auto stream = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!*stream)
        {
            return nullptr;
        }
        stream_ = std::move(stream);
        if (opened_now != nullptr)
        {
            *opened_now = true;
        }
    }
    // A stream that hit EOF on the previous read keeps its failbit set, which
    // would silently poison every later seek on the same handle.
    stream_->clear();
    stream_->seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!*stream_)
    {
        return nullptr;
    }
    return stream_.get();
}

void DosHostFileCache::Reset()
{
    stream_.reset();
}

std::ofstream* DosHostWriteCache::Acquire(const std::filesystem::path& path,
                                          std::uint64_t offset,
                                          bool truncate,
                                          bool* opened_now)
{
    if (opened_now != nullptr)
    {
        *opened_now = false;
    }
    if (truncate)
    {
        // Creation empties the file, so the cached stream (if any) is reopened
        // with trunc rather than seeked.
        stream_.reset();
    }
    if (stream_ == nullptr)
    {
        const std::ios::openmode mode = truncate
            ? (std::ios::binary | std::ios::out | std::ios::trunc)
            : (std::ios::binary | std::ios::out | std::ios::in);
        auto stream = std::make_unique<std::ofstream>(path, mode);
        if (!*stream)
        {
            return nullptr;
        }
        stream_ = std::move(stream);
        if (opened_now != nullptr)
        {
            *opened_now = true;
        }
    }
    stream_->clear();
    stream_->seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!*stream_)
    {
        return nullptr;
    }
    return stream_.get();
}

void DosHostWriteCache::Reset()
{
    stream_.reset();
}

bool ReadDosFile(DosVirtualFileSystemState* state,
                 std::uint16_t handle,
                 std::uint32_t requested_bytes,
                 std::vector<std::uint8_t>* bytes,
                 std::uint32_t* actual_bytes,
                 std::uint16_t* dos_error)
{
    if (state == nullptr || bytes == nullptr || actual_bytes == nullptr ||
        dos_error == nullptr)
    {
        return false;
    }

    bytes->clear();
    *actual_bytes = 0;
    *dos_error = 0;

    DosOpenFileHandle* open_file = nullptr;
    for (DosOpenFileHandle& candidate : state->open_files)
    {
        if (candidate.open && candidate.handle == handle)
        {
            open_file = &candidate;
            break;
        }
    }

    if (open_file == nullptr)
    {
        *dos_error = 0x0006;
        state->message = "DOS file read failed: invalid handle";
        return true;
    }

    if ((open_file->access_mode & 0x07U) == 0x01U)
    {
        *dos_error = 0x0005;
        state->message = "DOS file read failed: handle is write-only";
        return true;
    }

    if (requested_bytes == 0)
    {
        state->message = "DOS file read completed";
        return true;
    }

    // Task 374: the size was stated on every read. It is captured once at open
    // now, which is safe while this API stays read-only.
    const std::uint64_t file_size = open_file->cached_file_size;

    if (open_file->file_offset >= file_size)
    {
        state->message = "DOS file read completed at EOF";
        return true;
    }

    const std::uint64_t remaining = file_size - open_file->file_offset;
    const std::uint32_t clamped_request =
        static_cast<std::uint32_t>(
            std::min<std::uint64_t>(requested_bytes, remaining));

    // Task 374: reuses the stream this handle already opened. The failure codes
    // stay exactly as they were -- 0x0002 when the file cannot be opened, 0x0005
    // when positioning fails -- so guest-visible behaviour is unchanged.
    bool opened_now = false;
    std::ifstream* stream = open_file->host_stream.Acquire(
        open_file->host_path, open_file->file_offset, &opened_now);
    if (opened_now)
    {
        ++state->host_file_open_count;
    }
    if (stream == nullptr)
    {
        // Acquire fails either because the open failed or because the seek did.
        // A cold cache that stayed cold is the open case.
        *dos_error = open_file->host_stream.warm() ? 0x0005 : 0x0002;
        state->message = open_file->host_stream.warm()
            ? "DOS file read failed: seek failed"
            : "DOS file read failed: file could not be opened";
        return true;
    }

    ++state->file_read_count;
    bytes->resize(clamped_request);
    stream->read(reinterpret_cast<char*>(bytes->data()), clamped_request);
    const std::streamsize read_count = stream->gcount();
    if (read_count < 0)
    {
        *dos_error = 0x0005;
        bytes->clear();
        state->message = "DOS file read failed";
        return true;
    }

    bytes->resize(static_cast<std::size_t>(read_count));
    *actual_bytes = static_cast<std::uint32_t>(read_count);
    open_file->file_offset += static_cast<std::uint64_t>(*actual_bytes);
    state->message = "DOS file read completed";
    return true;
}

bool SeekDosFile(DosVirtualFileSystemState* state,
                 std::uint16_t handle,
                 std::uint8_t origin,
                 std::int32_t offset,
                 std::uint32_t* new_position,
                 std::uint16_t* dos_error)
{
    if (state == nullptr || new_position == nullptr || dos_error == nullptr)
    {
        return false;
    }

    *new_position = 0;
    *dos_error = 0;

    DosOpenFileHandle* open_file = nullptr;
    for (DosOpenFileHandle& candidate : state->open_files)
    {
        if (candidate.open && candidate.handle == handle)
        {
            open_file = &candidate;
            break;
        }
    }

    if (open_file == nullptr)
    {
        *dos_error = 0x0006;
        state->message = "DOS file seek failed: invalid handle";
        return true;
    }

    std::uint64_t base = 0;
    if (origin == 0)
    {
        base = 0;
    }
    else if (origin == 1)
    {
        base = open_file->file_offset;
    }
    else if (origin == 2)
    {
        // Task 374: the size captured at open, rather than a stat per seek.
        base = open_file->cached_file_size;
    }
    else
    {
        *dos_error = 0x0001;
        state->message = "DOS file seek failed: invalid origin";
        return true;
    }

    const std::int64_t signed_base =
        static_cast<std::int64_t>(base);
    const std::int64_t signed_position =
        signed_base + static_cast<std::int64_t>(offset);
    if (signed_position < 0)
    {
        *dos_error = 0x0001;
        state->message = "DOS file seek failed: negative position";
        return true;
    }

    const std::uint64_t position =
        static_cast<std::uint64_t>(signed_position);
    if (position > std::numeric_limits<std::uint32_t>::max())
    {
        *dos_error = 0x0001;
        state->message = "DOS file seek failed: position is too large";
        return true;
    }

    open_file->file_offset = position;
    *new_position = static_cast<std::uint32_t>(position);
    state->message = "DOS file seek completed";
    return true;
}

bool CloseDosFile(DosVirtualFileSystemState* state,
                  std::uint16_t handle,
                  std::uint16_t* dos_error)
{
    if (state == nullptr || dos_error == nullptr)
    {
        return false;
    }

    *dos_error = 0;

    for (DosOpenFileHandle& open_file : state->open_files)
    {
        if (open_file.open && open_file.handle == handle)
        {
            open_file.open = false;
            // Task 374: release the host stream with the handle. A closed slot is
            // reused by the next open, and carrying a stale stream into it would
            // read the previous file.
            open_file.host_stream.Reset();
            open_file.cached_file_size = 0;
            state->message = "DOS file closed";
            return true;
        }
    }

    *dos_error = 0x0006;
    state->message = "DOS file close failed: invalid handle";
    return true;
}

bool IsDosFileHandleOpen(const DosVirtualFileSystemState& state,
                         std::uint16_t handle)
{
    for (const DosOpenFileHandle& open_file : state.open_files)
    {
        if (open_file.open && open_file.handle == handle)
        {
            return true;
        }
    }

    return false;
}

bool QueryDosFileAttributes(DosVirtualFileSystemState* state,
                            const std::string& guest_path,
                            DosResolvedPath* resolved,
                            std::uint16_t* attributes)
{
    if (state == nullptr || resolved == nullptr || attributes == nullptr ||
        !state->valid)
    {
        return false;
    }
    std::vector<std::string> components;
    if (!ResolveGuestPath(*state, guest_path, resolved, &components))
    {
        return false;
    }
    std::error_code status_error;
    const std::filesystem::file_status status =
        std::filesystem::status(resolved->host_path, status_error);
    if (status_error || !std::filesystem::exists(status))
    {
        resolved->result = DosPathResult::kFileNotFound;
        resolved->message = "DOS attribute path does not exist";
        return false;
    }
    for (const auto& override_entry : state->attribute_overrides)
    {
        if (override_entry.first == resolved->dos_path)
        {
            *attributes = override_entry.second;
            resolved->result = DosPathResult::kOk;
            return true;
        }
    }
    *attributes = std::filesystem::is_directory(status) ? 0x0010U : 0x0020U;
    resolved->result = DosPathResult::kOk;
    resolved->message = "DOS attributes resolved";
    return true;
}

bool SetDosFileAttributes(DosVirtualFileSystemState* state,
                          const std::string& guest_path,
                          std::uint16_t attributes,
                          DosResolvedPath* resolved)
{
    std::uint16_t current = 0;
    if (!QueryDosFileAttributes(state, guest_path, resolved, &current))
    {
        return false;
    }
    constexpr std::uint16_t kMutableAttributes = 0x0027U;
    if ((attributes & ~kMutableAttributes) != 0 ||
        (current & 0x0010U) != 0)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS directory or unsupported attributes are immutable";
        return false;
    }
    for (auto& override_entry : state->attribute_overrides)
    {
        if (override_entry.first == resolved->dos_path)
        {
            override_entry.second = attributes & kMutableAttributes;
            return true;
        }
    }
    state->attribute_overrides.emplace_back(
        resolved->dos_path, attributes & kMutableAttributes);
    return true;
}

std::uint16_t DosPathResultToErrorCode(DosPathResult result)
{
    switch (result)
    {
        case DosPathResult::kOk:
            return 0x0000;
        case DosPathResult::kFileNotFound:
            return 0x0002;
        case DosPathResult::kAccessDenied:
            return 0x0005;
        case DosPathResult::kPathNotFound:
            return 0x0003;
    }

    return 0x0003;
}

}  // namespace repiu::hle
