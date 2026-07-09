#include "repiu/hle/dos_file_system.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace repiu::hle
{
namespace
{

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

    state->valid = true;
    state->host_root = absolute_root;
    state->current_components.clear();
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

    if (state->next_file_handle == 0)
    {
        resolved->result = DosPathResult::kAccessDenied;
        resolved->message = "DOS file handle table is exhausted";
        return true;
    }

    const std::uint16_t allocated_handle = state->next_file_handle++;
    state->open_files.push_back(DosOpenFileHandle{
        true,
        allocated_handle,
        access_mode,
        guest_path,
        resolved->host_path,
        resolved->dos_path,
    });
    *handle = allocated_handle;
    resolved->message = "DOS file opened";
    state->message = "DOS file opened";
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
