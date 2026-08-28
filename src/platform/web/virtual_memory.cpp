#include "repiu/platform/virtual_memory.h"

#if defined(__EMSCRIPTEN__)

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "web_unsupported.h"

namespace repiu::platform
{
namespace
{

bool g_reported = false;

void Report()
{
    web::ReportUnsupportedOnce(
        &g_reported,
        "[repiu-web] virtual memory is unavailable: wasm linear memory has no "
        "page protection, so reservation, commit, and RW/RX transitions cannot "
        "be expressed. Task 513 Stage 3 replaces the callers.\n");
}

}  // namespace

// Reservation is refused rather than emulated with malloc.
//
// Emulating it would succeed: an allocation of the right size hands back usable
// memory. But every caller of this API reserves in order to *protect* later --
// the AOT cache flips RW to RX, and self-modifying-code detection stands on
// making a page unwritable. A reservation that cannot be protected satisfies the
// call and breaks the invariant, which is the failure that surfaces far from
// here.
MemoryReservation ReserveMemory(void* /*preferred_base*/,
                                std::size_t /*bytes*/,
                                bool /*commit*/,
                                MemoryProtection /*protection*/)
{
    Report();
    MemoryReservation reservation;
    // ENOSYS, matching what the POSIX backend would report for a call the host
    // does not implement.
    reservation.error = 38U;
    return reservation;
}

bool CommitMemory(void* /*base*/,
                  std::size_t /*bytes*/,
                  MemoryProtection /*protection*/)
{
    Report();
    return false;
}

bool ReleaseMemory(void* /*base*/, std::size_t /*bytes*/)
{
    Report();
    return false;
}

bool ProtectMemory(void* /*address*/,
                   std::size_t /*bytes*/,
                   MemoryProtection /*protection*/,
                   MemoryProtection* /*previous*/)
{
    Report();
    return false;
}

// Deliberately not "unclaimed". An unclaimed region is a positive answer -- the
// Linux backend returns it so a caller can reserve there -- and answering it
// here would invite a reservation that then fails.
MemoryRegion QueryMemory(const void* address)
{
    Report();
    MemoryRegion region;
    region.base = address;
    region.protection = MemoryProtection::kOther;
    return region;
}

// The one call in this header that is honestly satisfiable. wasm has no
// separate instruction cache to flush, and code is never rewritten in linear
// memory, so there is nothing to make coherent and true is the correct answer
// rather than a convenient one.
bool FlushInstructionCacheRange(const void* /*address*/, std::size_t /*bytes*/)
{
    return true;
}

}  // namespace repiu::platform

#endif  // __EMSCRIPTEN__
