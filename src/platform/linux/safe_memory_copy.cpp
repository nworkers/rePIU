#include "repiu/platform/safe_memory_copy.h"

#if !defined(_WIN32)

#include <cerrno>

#include <sys/uio.h>
#include <unistd.h>

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

    iovec local{};
    local.iov_base = destination;
    local.iov_len = bytes;
    iovec remote{};
    // const_cast because the structure has no const form; the call only reads.
    remote.iov_base = const_cast<void*>(source);
    remote.iov_len = bytes;

    // Reading one's own process needs no privilege, and unlike a plain copy it
    // returns EFAULT for an unreadable page instead of raising SIGSEGV -- which
    // is the entire point, since the caller may already be inside the handler
    // that a SIGSEGV would re-enter.
    const ssize_t copied =
        ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0)
    {
        result.error = static_cast<std::uint32_t>(errno);
        return result;
    }
    result.bytes_copied = static_cast<std::size_t>(copied);
    result.complete = result.bytes_copied == bytes;
    if (!result.complete)
    {
        // A short read is not an error the kernel reports, so there is no errno
        // to pass on; the count is what tells the caller what happened.
        result.error = 0;
    }
    return result;
}

}  // namespace repiu::platform

#endif
