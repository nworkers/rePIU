#include "repiu/platform/host_error_stream.h"

// Task 758. The Win32 standard-error write. The POSIX one is in linux/.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace repiu::platform
{

void WriteHostErrorStream(const char* bytes, std::size_t count)
{
    if (bytes == nullptr || count == 0)
    {
        return;
    }
    HANDLE stream = GetStdHandle(STD_ERROR_HANDLE);
    if (stream == nullptr || stream == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(stream, bytes, static_cast<DWORD>(count), &written, nullptr);
}

}  // namespace repiu::platform
