#include "repiu/platform/host_process.h"

#if !defined(_WIN32)

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <vector>

namespace repiu::platform
{

// `posix_spawn` rather than a hand-written fork and exec. What this needs is
// exactly what it provides -- start a program, inherit the environment the
// launcher just published, wait -- and a fork in a process that has already
// loaded a GPU driver is the shape with the failure modes.
int RunChildProcessAndWait(const ChildProcessLaunch& launch,
                           std::uint32_t* host_error)
{
    if (host_error != nullptr)
    {
        *host_error = 0;
    }
    if (launch.executable_path == nullptr)
    {
        return kChildProcessDidNotStart;
    }

    // argv[0] is the program itself, then the caller's arguments, then the
    // terminator. The strings are not copied: `posix_spawn` reads them before
    // it returns, and everything here outlives that call.
    std::vector<char*> argv;
    argv.reserve(launch.argument_count + 2);
    argv.push_back(const_cast<char*>(launch.executable_path));
    for (std::size_t index = 0; index < launch.argument_count; ++index)
    {
        if (launch.arguments == nullptr || launch.arguments[index] == nullptr)
        {
            continue;
        }
        argv.push_back(const_cast<char*>(launch.arguments[index]));
    }
    argv.push_back(nullptr);

    pid_t child = 0;
    const int spawned = posix_spawn(&child, launch.executable_path, nullptr,
                                    nullptr, argv.data(), environ);
    if (spawned != 0)
    {
        if (host_error != nullptr)
        {
            *host_error = static_cast<std::uint32_t>(spawned);
        }
        return kChildProcessDidNotStart;
    }

    int status = 0;
    // A signal delivered to this process must not end the wait early, or the
    // launcher would return while the game is still running.
    while (waitpid(child, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            if (host_error != nullptr)
            {
                *host_error = static_cast<std::uint32_t>(errno);
            }
            return kChildProcessDidNotStart;
        }
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    // Killed by a signal. Windows has no counterpart to report, and the caller
    // only distinguishes "started" from "did not", so this reads as a failed
    // run rather than a failed launch.
    return 1;
}

}  // namespace repiu::platform

#endif  // !_WIN32
