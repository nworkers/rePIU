#ifndef REPIU_PLATFORM_SAFE_MEMORY_COPY_H_
#define REPIU_PLATFORM_SAFE_MEMORY_COPY_H_

#include <cstddef>
#include <cstdint>

// Task 503d-7. Reading memory that might not be readable, without faulting.
//
// The engine does this in two situations, and `memcpy` is wrong in both.
//
// The first is diagnostics. When a fault is being reported, the crash record
// wants the bytes around EIP, the top of the guest stack, and the string each
// register points at -- from addresses that are suspect by definition, since
// something has already gone wrong. A plain read there turns a report into a
// second crash.
//
// The second is stated by the name of the function that has used it longest,
// `CopyHostMemoryWithoutVehRecursion`: a fault raised inside the vectored
// handler re-enters the vectored handler. Reporting failure instead of raising
// is what keeps that from recursing.
//
// Windows spells this `ReadProcessMemory` against its own process. Linux has
// `process_vm_readv`, which is the same idea and, applied to one's own process,
// needs no privilege. Both stop at the first unreadable page and say how far
// they got, which is why the result carries a count rather than just a flag --
// every caller here uses the partial copy.

namespace repiu::platform
{

struct SafeCopyResult
{
    // True only for a complete copy. A partial copy reports false with a
    // non-zero `bytes_copied`, which several callers use directly.
    bool complete = false;
    std::size_t bytes_copied = 0;
    // GetLastError on Windows, errno on POSIX. Zero when nothing failed.
    std::uint32_t error = 0;
};

// Copies up to `bytes` from `source` into `destination`, stopping at the first
// address that cannot be read rather than faulting.
SafeCopyResult CopyMemoryWithoutFaulting(void* destination,
                                         const void* source,
                                         std::size_t bytes);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_SAFE_MEMORY_COPY_H_
