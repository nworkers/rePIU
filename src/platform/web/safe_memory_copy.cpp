#include "repiu/platform/safe_memory_copy.h"

#if defined(__EMSCRIPTEN__)

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace repiu::platform
{
namespace
{

// wasm has no counterpart to ReadProcessMemory or process_vm_readv, and it does
// not need one for the same reason: every address inside linear memory is
// readable, so an in-bounds read cannot fault.
//
// What it has instead is a harder failure at the edge. An out-of-bounds access
// is a wasm trap, and a trap is not a signal -- there is no handler to install
// and no resumption point. The process is gone. So where the other two hosts
// recover from a bad read, this one has to refuse it beforehand, and the bounds
// check below is the entire safety net rather than a first line of one.
//
// __builtin_wasm_memory_size is the current size in 64 KiB pages. It is read on
// every call rather than cached because memory.grow moves the limit upward
// while the module runs.
std::uintptr_t LinearMemoryLimit()
{
    constexpr std::uintptr_t kWasmPageBytes = 65536U;
    return static_cast<std::uintptr_t>(__builtin_wasm_memory_size(0)) *
           kWasmPageBytes;
}

}  // namespace

SafeCopyResult CopyMemoryWithoutFaulting(void* destination,
                                         const void* source,
                                         std::size_t bytes)
{
    SafeCopyResult result;
    if (destination == nullptr || source == nullptr || bytes == 0)
    {
        result.complete = bytes == 0 && destination != nullptr &&
                          source != nullptr;
        return result;
    }

    const std::uintptr_t limit = LinearMemoryLimit();
    const auto source_start = reinterpret_cast<std::uintptr_t>(source);
    const auto destination_start =
        reinterpret_cast<std::uintptr_t>(destination);

    // Overflow first: a length that wraps would otherwise pass the comparison
    // it is supposed to fail.
    if (source_start > limit || destination_start > limit ||
        bytes > limit - source_start || bytes > limit - destination_start)
    {
        // EFAULT, so the number a caller logs means the same thing it does on
        // the POSIX backend.
        result.error = 14U;
        return result;
    }

    std::memcpy(destination, source, bytes);
    result.complete = true;
    result.bytes_copied = bytes;
    return result;
}

}  // namespace repiu::platform

#endif  // __EMSCRIPTEN__
