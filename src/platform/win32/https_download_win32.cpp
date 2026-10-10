#include "repiu/platform/https_download.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace repiu::platform
{
namespace
{

std::wstring Widen(const char* text)
{
    if (text == nullptr || text[0] == '\0')
    {
        return std::wstring();
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (length <= 0)
    {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), length);
    wide.resize(static_cast<std::size_t>(length - 1));
    return wide;
}

std::string LastErrorText(const char* what)
{
    return std::string(what) + " failed (error " +
        std::to_string(GetLastError()) + ")";
}

// Closes a WinHTTP handle when it goes out of scope.
class InternetHandle
{
public:
    explicit InternetHandle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~InternetHandle()
    {
        if (handle_ != nullptr)
        {
            WinHttpCloseHandle(handle_);
        }
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    HINTERNET get() const { return handle_; }

private:
    HINTERNET handle_;
};

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
    std::wstring url = Widen(request.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS)
    {
        *error = "malformed https URL";
        return HttpsDownloadResult::kFailed;
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo != nullptr)
    {
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }

    const std::wstring agent = Widen(
        request.user_agent != nullptr ? request.user_agent : "rePIU");
    InternetHandle session(WinHttpOpen(agent.c_str(),
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (session.get() == nullptr)
    {
        *error = LastErrorText("WinHttpOpen");
        return HttpsDownloadResult::kFailed;
    }
    // Per-call limits short enough that a cancel is seen between reads; the
    // whole transfer is held to the request's limit below.
    WinHttpSetTimeouts(session.get(), 15000, 15000, 15000, 15000);
    // https to https redirects only: GitHub's asset links redirect to its
    // download host, and nothing should ever lead to plain http.
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(session.get(), WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirect_policy, sizeof(redirect_policy));
    InternetHandle connection(
        WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (connection.get() == nullptr)
    {
        *error = LastErrorText("WinHttpConnect");
        return HttpsDownloadResult::kFailed;
    }
    InternetHandle http(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (http.get() == nullptr)
    {
        *error = LastErrorText("WinHttpOpenRequest");
        return HttpsDownloadResult::kFailed;
    }
    if (request.accept != nullptr)
    {
        const std::wstring header =
            L"Accept: " + Widen(request.accept);
        WinHttpAddRequestHeaders(http.get(), header.c_str(),
                                 static_cast<DWORD>(-1L),
                                 WINHTTP_ADDREQ_FLAG_ADD |
                                     WINHTTP_ADDREQ_FLAG_REPLACE);
    }
    if (!WinHttpSendRequest(http.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(http.get(), nullptr))
    {
        *error = LastErrorText("HTTPS request");
        return HttpsDownloadResult::kFailed;
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(http.get(),
                             WINHTTP_QUERY_STATUS_CODE |
                                 WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status,
                             &status_size, WINHTTP_NO_HEADER_INDEX) ||
        status != 200U)
    {
        *error = "HTTP status " + std::to_string(status);
        return HttpsDownloadResult::kFailed;
    }

    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        *error = "cannot write " + destination.string();
        return HttpsDownloadResult::kFailed;
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(request.timeout_seconds);
    std::vector<char> buffer(1U << 16);
    HttpsDownloadResult result = HttpsDownloadResult::kOk;
    for (;;)
    {
        if (request.cancel != nullptr &&
            request.cancel->load(std::memory_order_relaxed))
        {
            *error = "cancelled";
            result = HttpsDownloadResult::kCancelled;
            break;
        }
        if (std::chrono::steady_clock::now() > deadline)
        {
            *error = "timed out";
            result = HttpsDownloadResult::kFailed;
            break;
        }
        DWORD read = 0;
        if (!WinHttpReadData(http.get(), buffer.data(),
                             static_cast<DWORD>(buffer.size()), &read))
        {
            *error = LastErrorText("WinHttpReadData");
            result = HttpsDownloadResult::kFailed;
            break;
        }
        if (read == 0U)
        {
            break;
        }
        stream.write(buffer.data(), static_cast<std::streamsize>(read));
        stream.flush();
        if (!stream)
        {
            *error = "cannot write " + destination.string();
            result = HttpsDownloadResult::kFailed;
            break;
        }
    }
    stream.close();
    if (result != HttpsDownloadResult::kOk)
    {
        std::error_code remove_error;
        std::filesystem::remove(destination, remove_error);
    }
    return result;
}

}  // namespace repiu::platform

#endif  // _WIN32
