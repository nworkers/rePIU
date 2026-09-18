#include "repiu/runtime/low_address_reservation.h"

#include "repiu/platform/virtual_memory.h"

#include <limits>

namespace repiu::runtime
{
namespace
{

// Whether this host's pointers can name an address a 32-bit field cannot hold.
// Written as a value rather than a preprocessor branch so both halves compile
// everywhere and neither can rot unnoticed.
constexpr bool kHostPointerExceeds32Bit = sizeof(void*) > 4U;

repiu::platform::MemoryReservation Reserve(void* const preferred_base,
                                           const std::size_t capacity)
{
    return repiu::platform::ReserveMemory(
        preferred_base, capacity, true,
        repiu::platform::MemoryProtection::kReadWrite);
}

}  // namespace

bool LowAddressFits32Bit(const void* const base, const std::size_t capacity)
{
    if (base == nullptr)
    {
        return false;
    }
    constexpr std::uintptr_t kLimit = std::numeric_limits<std::uint32_t>::max();
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
    if (address > kLimit)
    {
        return false;
    }
    // An empty region has no last byte to test; anything else is judged by its
    // last byte rather than its first.
    if (capacity == 0U)
    {
        return true;
    }
    return static_cast<std::uintptr_t>(capacity - 1U) <= kLimit - address;
}

LowAddressReservation ReserveLowAddressMemory(
    const LowAddressReservationRequest& request)
{
    LowAddressReservation result;
    if (request.capacity == 0U)
    {
        return result;
    }

    if constexpr (kHostPointerExceeds32Bit)
    {
        std::uint32_t attempt = 0U;
        for (std::size_t index = 0; index < request.candidate_count; ++index)
        {
            const std::uintptr_t candidate = request.candidate_bases[index];
            const repiu::platform::MemoryReservation reservation =
                Reserve(reinterpret_cast<void*>(candidate), request.capacity);
            if (reservation.base != nullptr &&
                LowAddressFits32Bit(reservation.base, request.capacity))
            {
                result.valid = true;
                result.fits_32bit = true;
                result.base = reservation.base;
                result.size = reservation.size;
                result.attempt = attempt;
                result.requested_base = candidate;
                return result;
            }
            // Either the candidate was taken, or the host ignored the hint and
            // answered somewhere a 32-bit field cannot name. Keeping the second
            // would be worse than having none: whatever reads the truncated
            // address would reach someone else's memory.
            if (reservation.base != nullptr)
            {
                repiu::platform::ReleaseMemory(reservation.base,
                                               reservation.size);
            }
            result.error = reservation.error;
            ++attempt;
        }

        if (!request.allow_unhinted_fallback)
        {
            return result;
        }
    }

    const repiu::platform::MemoryReservation reservation =
        Reserve(nullptr, request.capacity);
    result.attempt = kLowAddressUnhintedAttempt;
    result.requested_base = 0U;
    if (reservation.base == nullptr)
    {
        result.error = reservation.error;
        return result;
    }
    result.valid = true;
    result.base = reservation.base;
    result.size = reservation.size;
    result.fits_32bit =
        LowAddressFits32Bit(reservation.base, request.capacity);
    return result;
}

void ReleaseLowAddressMemory(const LowAddressReservation& reservation)
{
    if (!reservation.valid || reservation.base == nullptr)
    {
        return;
    }
    repiu::platform::ReleaseMemory(reservation.base, reservation.size);
}

}  // namespace repiu::runtime
