#include "repiu/platform/win32/live_telemetry.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

long ReadInterlocked(volatile long* value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

void PrintSnapshot(
    repiu::platform::win32::Win32SharedLiveTelemetry& telemetry,
    std::uint32_t elapsed_milliseconds)
{
    std::cout << "[repiu-supervisor] elapsed_ms=" << elapsed_milliseconds
              << " phase="
              << ReadInterlocked(&telemetry.host_phase)
              << " heartbeat="
              << ReadInterlocked(&telemetry.heartbeat)
              << " dispatch_entry="
              << ReadInterlocked(&telemetry.dispatch_entry_count)
              << " dispatch_exit="
              << ReadInterlocked(&telemetry.dispatch_exit_count)
              << " last_eip=0x" << std::hex
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_eip))
              << " last_guest_eip=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_eip))
              << " exception=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_exception_code))
              << " guest_eax=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_eax))
              << " guest_ebx/ecx/edx=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_ebx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_ecx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_edx))
              << " guest_esi/edi=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_esi)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_edi))
              << " guest_esp=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.last_guest_esp))
              << " recovery_fs/ds/es/gs=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_fs)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_ds)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_es)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.recovery_host_gs))
              << " dpmi_frame_eax/ebx/ecx=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.dpmi_frame_eax)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.dpmi_frame_ebx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.dpmi_frame_ecx))
              << " handler_phase="
              << ReadInterlocked(&telemetry.guest_handler_phase)
              << " glide_ordinal="
              << ReadInterlocked(&telemetry.glide_gate_ordinal)
              << " glide_esp=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_esp))
              << " glide_ebx/ecx/edx=0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_ebx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_ecx)) << "/0x"
              << static_cast<std::uint32_t>(
                     ReadInterlocked(&telemetry.glide_gate_edx))
              << " glide_stack="
              << ReadInterlocked(&telemetry.glide_gate_stack[0]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[1]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[2]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[3]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[4]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[5]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[6]) << ","
              << ReadInterlocked(&telemetry.glide_gate_stack[7])
              << std::dec << "\n";
    std::cout.flush();
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string target = argc >= 2 ? argv[1] : "piu_1st";
    const std::uint32_t timeout_milliseconds =
        argc >= 3 ? static_cast<std::uint32_t>(std::stoul(argv[2]))
                  : 10000U;
    const std::uint32_t child_timeout_milliseconds =
        timeout_milliseconds >= 2000U
            ? timeout_milliseconds - 1000U
            : 1000U;

    char module_path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, module_path, MAX_PATH) == 0)
    {
        return 1;
    }
    const std::filesystem::path loader_path =
        std::filesystem::path(module_path).parent_path() /
        "repiu_loader_win32.exe";

    const std::string mapping_name =
        "Local\\repiu-live-" + std::to_string(GetCurrentProcessId()) +
        "-" + std::to_string(GetTickCount());
    HANDLE mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(repiu::platform::win32::Win32SharedLiveTelemetry),
        mapping_name.c_str());
    if (mapping == nullptr)
    {
        return 2;
    }
    auto* telemetry = static_cast<
        repiu::platform::win32::Win32SharedLiveTelemetry*>(
            MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (telemetry == nullptr)
    {
        CloseHandle(mapping);
        return 3;
    }
    *telemetry = repiu::platform::win32::Win32SharedLiveTelemetry{};

    SetEnvironmentVariableA(
        repiu::platform::win32::kWin32LiveTelemetryEnvironment,
        mapping_name.c_str());
    const std::string child_timeout =
        std::to_string(child_timeout_milliseconds);
    SetEnvironmentVariableA(
        repiu::platform::win32::kWin32ExecutionTimeoutEnvironment,
        child_timeout.c_str());
    std::string command =
        "\"" + loader_path.string() + "\" " + target;
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL created = CreateProcessA(
        nullptr,
        command.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startup,
        &process);
    SetEnvironmentVariableA(
        repiu::platform::win32::kWin32LiveTelemetryEnvironment,
        nullptr);
    SetEnvironmentVariableA(
        repiu::platform::win32::kWin32ExecutionTimeoutEnvironment,
        nullptr);
    if (!created)
    {
        UnmapViewOfFile(telemetry);
        CloseHandle(mapping);
        return 4;
    }

    const DWORD start = GetTickCount();
    DWORD last_snapshot = start - 1000U;
    bool terminated = false;
    for (;;)
    {
        const DWORD wait = WaitForSingleObject(process.hProcess, 50U);
        const DWORD elapsed = GetTickCount() - start;
        if (GetTickCount() - last_snapshot >= 1000U)
        {
            PrintSnapshot(*telemetry, elapsed);
            last_snapshot = GetTickCount();
        }
        if (wait == WAIT_OBJECT_0)
        {
            break;
        }
        if (elapsed >= timeout_milliseconds)
        {
            TerminateProcess(process.hProcess, 124U);
            WaitForSingleObject(process.hProcess, 5000U);
            terminated = true;
            break;
        }
    }
    PrintSnapshot(*telemetry, GetTickCount() - start);

    DWORD exit_code = 0;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    UnmapViewOfFile(telemetry);
    CloseHandle(mapping);
    std::cout << "[repiu-supervisor] child_exit=" << exit_code
              << " terminated=" << (terminated ? "true" : "false")
              << "\n";
    return 0;
}
