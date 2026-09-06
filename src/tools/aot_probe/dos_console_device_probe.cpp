#include "dos_console_device_probe.h"

#include "repiu/hle/dos_file_system.h"

#include <filesystem>
#include <iostream>
#include <system_error>

namespace repiu::tools
{
namespace
{

std::filesystem::path MakeScratchRoot()
{
    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) /
        "repiu_dos_console_device_probe";
    if (error)
    {
        return {};
    }
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return error ? std::filesystem::path{} : root;
}

}  // namespace

bool RunDosConsoleDeviceProbe()
{
    using repiu::hle::CloseDosFile;
    using repiu::hle::DosPathResult;
    using repiu::hle::DosResolvedPath;
    using repiu::hle::DosVirtualFileSystemState;
    using repiu::hle::InitializeDosVirtualFileSystem;
    using repiu::hle::IsDosConsoleFileHandle;
    using repiu::hle::IsDosFileHandleWritable;
    using repiu::hle::OpenDosFile;

    const std::filesystem::path root = MakeScratchRoot();
    DosVirtualFileSystemState state;
    const bool initialized = !root.empty() &&
        InitializeDosVirtualFileSystem(root, &state) && state.valid;

    DosResolvedPath resolved;
    std::uint16_t handle = 0;
    const bool opened = initialized &&
        OpenDosFile(&state, "con", 0x01U, &resolved, &handle) &&
        resolved.result == DosPathResult::kOk && handle == 5U &&
        resolved.host_path.empty() &&
        IsDosConsoleFileHandle(state, handle) &&
        IsDosFileHandleWritable(state, handle) &&
        !std::filesystem::exists(root / "CON") &&
        state.host_file_open_count == 0U;

    std::uint16_t dos_error = 0;
    const bool closed = opened && CloseDosFile(&state, handle, &dos_error) &&
        dos_error == 0U && !IsDosConsoleFileHandle(state, handle);

    DosResolvedPath colon_resolved;
    std::uint16_t colon_handle = 0;
    const bool colon_opened = closed &&
        OpenDosFile(&state, "CON:", 0x02U, &colon_resolved, &colon_handle) &&
        colon_resolved.result == DosPathResult::kOk &&
        IsDosConsoleFileHandle(state, colon_handle) &&
        IsDosFileHandleWritable(state, colon_handle) &&
        colon_resolved.host_path.empty() &&
        !std::filesystem::exists(root / "CON:");
    const bool colon_closed = colon_opened &&
        CloseDosFile(&state, colon_handle, &dos_error) && dos_error == 0U;

    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    const bool cleaned = !cleanup_error && !std::filesystem::exists(root);
    const bool all = initialized && opened && closed && colon_opened &&
        colon_closed && cleaned;

    std::cout << "dos_console_device_initialized="
              << (initialized ? "true" : "false")
              << "\ndos_console_device_opened=" << (opened ? "true" : "false")
              << "\ndos_console_device_closed=" << (closed ? "true" : "false")
              << "\ndos_console_device_colon_opened="
              << (colon_opened ? "true" : "false")
              << "\ndos_console_device_colon_closed="
              << (colon_closed ? "true" : "false")
              << "\ndos_console_device_cleanup=" << (cleaned ? "true" : "false")
              << "\ndos_console_device_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
