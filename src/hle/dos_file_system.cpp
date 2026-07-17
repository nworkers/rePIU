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

// DOS 표준 핸들 0~4(stdin/stdout/stderr/stdaux/stdprn)를 예약하므로 사용자 파일
// 핸들은 5부터 시작한다. 상한 20은 DOS 기본 Job File Table 크기이자, 게스트 clib이
// 인덱싱하는 핸들 플래그 테이블(20칸)의 크기와 일치한다 — 핸들 20 이상은 그 테이블을
// 오버플로우한다(Task 228).
// DOS reserves standard handles 0-4 (stdin/stdout/stderr/stdaux/stdprn), so user file
// handles start at 5. The limit 20 is the DOS default Job File Table size and matches the
// guest clib's 20-entry handle-flags table that is indexed by the handle; handles >= 20
// overflow that table (Task 228).
constexpr std::uint16_t kFirstDosUserHandle = 5;
constexpr std::uint16_t kDosOpenHandleLimit = 20;

// 실제 DOS처럼 현재 열려 있지 않은 가장 낮은 핸들 번호를 반환한다. 모두 사용 중이면 0.
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

    // 실제 DOS는 가장 낮은 free 핸들을 반환하고 close 시 번호를 회수한다. 단조 증가로
    // 번호를 소진하면 게스트 clib의 20칸 핸들 테이블을 오버플로우한다(Task 228).
    // Real DOS returns the lowest free handle and recycles numbers on close; monotonic
    // growth would overflow the guest clib's 20-entry handle table (Task 228).
    const std::uint16_t allocated_handle = AllocateLowestFreeDosHandle(*state);
    if (allocated_handle == 0)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS file handle table is exhausted";
        return true;
    }

    DosOpenFileHandle new_handle{
        true,
        allocated_handle,
        access_mode,
        0,
        guest_path,
        resolved->host_path,
        resolved->dos_path,
    };
    // 닫힌 슬롯이 있으면 재사용해 open_files 벡터의 무한 증가를 막는다.
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

    std::error_code error;
    const std::uint64_t file_size =
        std::filesystem::file_size(open_file->host_path, error);
    if (error)
    {
        *dos_error = 0x0002;
        state->message = "DOS file read failed: file size unavailable";
        return true;
    }

    if (open_file->file_offset >= file_size)
    {
        state->message = "DOS file read completed at EOF";
        return true;
    }

    const std::uint64_t remaining = file_size - open_file->file_offset;
    const std::uint32_t clamped_request =
        static_cast<std::uint32_t>(
            std::min<std::uint64_t>(requested_bytes, remaining));

    std::ifstream stream(open_file->host_path, std::ios::binary);
    if (!stream)
    {
        *dos_error = 0x0002;
        state->message = "DOS file read failed: file could not be opened";
        return true;
    }

    stream.seekg(static_cast<std::streamoff>(open_file->file_offset),
                 std::ios::beg);
    if (!stream)
    {
        *dos_error = 0x0005;
        state->message = "DOS file read failed: seek failed";
        return true;
    }

    bytes->resize(clamped_request);
    stream.read(reinterpret_cast<char*>(bytes->data()), clamped_request);
    const std::streamsize read_count = stream.gcount();
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
        std::error_code error;
        base = std::filesystem::file_size(open_file->host_path, error);
        if (error)
        {
            *dos_error = 0x0002;
            state->message =
                "DOS file seek failed: file size unavailable";
            return true;
        }
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
