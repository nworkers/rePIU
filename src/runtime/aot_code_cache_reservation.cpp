#include "repiu/runtime/aot_code_cache_reservation.h"

#include "repiu/platform/virtual_memory.h"

#include <limits>

namespace repiu::runtime
{
namespace
{

// Whether this host's pointers can name an address the engine's 32-bit cache
// fields cannot hold. Written as a value rather than a preprocessor branch so
// both halves are compiled everywhere and neither can rot unnoticed.
constexpr bool kHostPointerExceedsCacheAddress = sizeof(void*) > 4U;

repiu::platform::MemoryReservation Reserve(void* preferred_base,
                                           const std::size_t capacity)
{
    return repiu::platform::ReserveMemory(
        preferred_base, capacity, true,
        repiu::platform::MemoryProtection::kReadWrite);
}

bool FitsCacheAddress(const void* const base)
{
    return reinterpret_cast<std::uintptr_t>(base) <=
        std::numeric_limits<std::uint32_t>::max();
}

}  // namespace

AotCodeCacheReservation ReserveAotCodeCacheMemory(const std::size_t capacity)
{
    AotCodeCacheReservation result;
    if (capacity == 0U)
    {
        return result;
    }

    if constexpr (kHostPointerExceedsCacheAddress)
    {
        std::uint32_t attempt = 0U;
        for (const std::uintptr_t candidate : kAotCodeCacheCandidateBases)
        {
            const repiu::platform::MemoryReservation reservation =
                Reserve(reinterpret_cast<void*>(candidate), capacity);
            // The mapping is MAP_FIXED_NOREPLACE, so an occupied candidate
            // fails rather than displacing whatever holds it. A host that
            // ignores the hint instead of refusing would hand back some other
            // address, which is why the result is checked rather than assumed.
            if (reservation.base != nullptr && FitsCacheAddress(reservation.base))
            {
                result.valid = true;
                result.base = reservation.base;
                result.size = reservation.size;
                result.attempt = attempt;
                result.requested_base = candidate;
                return result;
            }
            if (reservation.base != nullptr)
            {
                repiu::platform::ReleaseMemory(reservation.base,
                                               reservation.size);
            }
            result.error = reservation.error;
            ++attempt;
        }
    }

    // The unhinted request. On a 32-bit host it is the whole policy and yields
    // a 32-bit address by construction; on a 64-bit host it is what is left
    // after every candidate was taken, and the caller's own above-4-GiB refusal
    // is the guard behind it.
    const repiu::platform::MemoryReservation reservation =
        Reserve(nullptr, capacity);
    result.attempt = kUnhintedAttempt;
    result.requested_base = 0U;
    if (reservation.base == nullptr)
    {
        result.error = reservation.error;
        return result;
    }
    result.valid = true;
    result.base = reservation.base;
    result.size = reservation.size;
    return result;
}

void ReleaseAotCodeCacheMemory(const AotCodeCacheReservation& reservation)
{
    if (!reservation.valid || reservation.base == nullptr)
    {
        return;
    }
    repiu::platform::ReleaseMemory(reservation.base, reservation.size);
}

}  // namespace repiu::runtime
