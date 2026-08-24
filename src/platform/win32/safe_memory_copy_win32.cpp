#include "repiu/platform/safe_memory_copy.h"

#if defined(_WIN32)

#include <windows.h>

namespace repiu::platform
{

SafeCopyResult CopyMemoryWithoutFaulting(void* destination,
                                         const void* source,
                                         const std::size_t bytes)
{
    SafeCopyResult result;
    if (destination == nullptr || source == nullptr || bytes == 0U)
    {
        result.complete = bytes == 0U;
        return result;
    }
    SIZE_T copied = 0;
    // Against this process's own handle, which is what makes the call a guarded
    // read rather than a cross-process one.
    const BOOL ok = ReadProcessMemory(GetCurrentProcess(), source, destination,
                                      static_cast<SIZE_T>(bytes), &copied);
    result.bytes_copied = static_cast<std::size_t>(copied);
    result.complete = ok != 0 && result.bytes_copied == bytes;
    if (!result.complete)
    {
        result.error = static_cast<std::uint32_t>(GetLastError());
    }
    return result;
}

}  // namespace repiu::platform

#endif
