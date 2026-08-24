#include "repiu/platform/host_error_stream.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace repiu::platform
{

void WriteHostErrorStream(const char* bytes, std::size_t count)
{
    if (bytes == nullptr || count == 0)
    {
        return;
    }
#if defined(_WIN32)
    HANDLE stream = GetStdHandle(STD_ERROR_HANDLE);
    if (stream == nullptr || stream == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(stream, bytes, static_cast<DWORD>(count), &written, nullptr);
#else
    // The result is deliberately ignored, as it is on Windows. A short write or
    // an EINTR loses part of a diagnostic line, which is the same outcome the
    // Windows side already accepts, and retrying here would let a full pipe
    // block the sampler against the thread it samples.
    const ssize_t ignored = ::write(STDERR_FILENO, bytes, count);
    (void)ignored;
#endif
}

}  // namespace repiu::platform
