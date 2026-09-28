#include "repiu/platform/host_error_stream.h"

// Task 758. The POSIX standard-error write, used by the Linux and web builds. The
// Win32 one is in win32/.

#include <unistd.h>

namespace repiu::platform
{

void WriteHostErrorStream(const char* bytes, std::size_t count)
{
    if (bytes == nullptr || count == 0)
    {
        return;
    }
    // The result is deliberately ignored, as it is on Windows. A short write or
    // an EINTR loses part of a diagnostic line, which is the same outcome the
    // Windows side already accepts, and retrying here would let a full pipe
    // block the sampler against the thread it samples.
    const ssize_t ignored = ::write(STDERR_FILENO, bytes, count);
    (void)ignored;
}

}  // namespace repiu::platform
