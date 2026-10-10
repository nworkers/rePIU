#include "repiu/platform/https_download.h"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

extern char** environ;

namespace repiu::platform
{
namespace
{

// The environment curl runs with: ours, minus what a game launcher such as
// Steam injects for its own process -- an overlay preload and a runtime
// library path -- which have nothing to do with a system tool.
std::vector<char*> CurlEnvironment()
{
    std::vector<char*> environment;
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry)
    {
        if (std::strncmp(*entry, "LD_PRELOAD=", 11) == 0 ||
            std::strncmp(*entry, "LD_LIBRARY_PATH=", 16) == 0)
        {
            continue;
        }
        environment.push_back(*entry);
    }
    environment.push_back(nullptr);
    return environment;
}

std::string ReadAndRemove(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    unlink(path.c_str());
    std::string message = text.str();
    while (!message.empty() &&
           (message.back() == '\n' || message.back() == '\r'))
    {
        message.pop_back();
    }
    return message;
}

}  // namespace

HttpsDownloadResult HttpsDownloadToFile(
    const HttpsDownloadRequest& request,
    const std::filesystem::path& destination, std::string* error)
{
    if (request.url == nullptr || std::strncmp(request.url, "https://", 8) != 0)
    {
        *error = "only https URLs are fetched";
        return HttpsDownloadResult::kFailed;
    }
    const std::string output = destination.string();
    const std::string error_path = output + ".curl-error";
    const std::string timeout = std::to_string(request.timeout_seconds);
    const std::string accept =
        request.accept != nullptr ? std::string("Accept: ") + request.accept
                                  : std::string();
    // An argument array, never a shell. -f turns an HTTP error into a failed
    // exit; -sS keeps progress quiet but errors on stderr, which goes to a
    // file read back below.
    std::vector<const char*> arguments = {
        "curl", "-fsSL", "--proto", "=https", "--proto-redir", "=https",
        "--connect-timeout", "15", "--max-time", timeout.c_str(),
        "-o", output.c_str()};
    if (request.user_agent != nullptr)
    {
        arguments.push_back("-A");
        arguments.push_back(request.user_agent);
    }
    if (!accept.empty())
    {
        arguments.push_back("-H");
        arguments.push_back(accept.c_str());
    }
    arguments.push_back("--");
    arguments.push_back(request.url);
    arguments.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                     O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                     O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                     error_path.c_str(),
                                     O_WRONLY | O_CREAT | O_TRUNC, 0600);
    std::vector<char*> environment = CurlEnvironment();
    pid_t child = 0;
    const int spawned = posix_spawnp(
        &child, "curl", &actions, nullptr,
        const_cast<char* const*>(arguments.data()), environment.data());
    posix_spawn_file_actions_destroy(&actions);
    if (spawned != 0)
    {
        unlink(error_path.c_str());
        *error = std::string("cannot run curl: ") + std::strerror(spawned);
        return spawned == ENOENT ? HttpsDownloadResult::kUnavailable
                                 : HttpsDownloadResult::kFailed;
    }

    int status = 0;
    bool cancelled = false;
    for (;;)
    {
        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
        {
            break;
        }
        if (waited < 0 && errno != EINTR)
        {
            *error = std::string("waiting for curl: ") + std::strerror(errno);
            unlink(error_path.c_str());
            unlink(output.c_str());
            return HttpsDownloadResult::kFailed;
        }
        if (!cancelled && request.cancel != nullptr &&
            request.cancel->load(std::memory_order_relaxed))
        {
            cancelled = true;
            kill(child, SIGTERM);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    const std::string message = ReadAndRemove(error_path);
    if (cancelled)
    {
        unlink(output.c_str());
        *error = "cancelled";
        return HttpsDownloadResult::kCancelled;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        unlink(output.c_str());
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        *error = "curl exit " + std::to_string(code) +
            (message.empty() ? std::string() : ": " + message);
        return HttpsDownloadResult::kFailed;
    }
    return HttpsDownloadResult::kOk;
}

}  // namespace repiu::platform

#endif  // !_WIN32 && !__EMSCRIPTEN__
