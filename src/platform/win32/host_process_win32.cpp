#include "repiu/platform/host_process.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace repiu::platform
{

// `CreateProcessA` writes into the command-line buffer it is given, so it is
// copied rather than cast away: the caller's string is not ours to modify.
int RunChildProcessAndWait(const ChildProcessLaunch& launch,
                           std::uint32_t* host_error)
{
    if (host_error != nullptr)
    {
        *host_error = 0;
    }
    if (launch.executable_path == nullptr || launch.command_line == nullptr)
    {
        return kChildProcessDidNotStart;
    }

    std::string command_line(launch.command_line);
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessA(launch.executable_path, command_line.data(), nullptr,
                       nullptr, TRUE, 0, nullptr, nullptr, &startup,
                       &process) == 0)
    {
        if (host_error != nullptr)
        {
            *host_error = static_cast<std::uint32_t>(GetLastError());
        }
        return kChildProcessDidNotStart;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exit_code);
}

}  // namespace repiu::platform

#endif  // _WIN32
