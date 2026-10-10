#ifndef REPIU_PLATFORM_HTTPS_DOWNLOAD_H_
#define REPIU_PLATFORM_HTTPS_DOWNLOAD_H_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

// Issue #48. Fetching one https URL into a file, for the launcher's update
// check. Each host uses what it already has rather than a TLS library the
// build would have to carry: WinHTTP on Win32, and the system `curl` on Linux,
// which keeps the release build's static runtime and glibc floor unchanged.
// The web build has neither and reports `kUnavailable`.
//
// Only https is followed, redirects included. The destination is written as
// the bytes arrive, so a caller can show progress from its size; on any
// failure it is removed.

namespace repiu::platform
{

struct HttpsDownloadRequest
{
    const char* url = nullptr;
    const char* user_agent = nullptr;
    // The Accept header, or null for none.
    const char* accept = nullptr;
    // The whole transfer's limit.
    std::uint32_t timeout_seconds = 60;
    // Polled while the transfer runs; true stops it as `kCancelled`.
    const std::atomic<bool>* cancel = nullptr;
};

enum class HttpsDownloadResult
{
    kOk,
    kFailed,
    kCancelled,
    // The host has no way to fetch (no curl on Linux, the web build).
    kUnavailable,
};

HttpsDownloadResult HttpsDownloadToFile(
    const HttpsDownloadRequest& request,
    const std::filesystem::path& destination, std::string* error);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HTTPS_DOWNLOAD_H_
