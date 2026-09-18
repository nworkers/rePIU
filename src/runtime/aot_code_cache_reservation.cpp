#include "repiu/runtime/aot_code_cache_reservation.h"

#include "repiu/runtime/low_address_reservation.h"

namespace repiu::runtime
{
namespace
{

constexpr std::size_t kCandidateCount =
    sizeof(kAotCodeCacheCandidateBases) /
    sizeof(kAotCodeCacheCandidateBases[0]);

}  // namespace

AotCodeCacheReservation ReserveAotCodeCacheMemory(const std::size_t capacity)
{
    AotCodeCacheReservation result;
    if (capacity == 0U)
    {
        return result;
    }

    // Task 708 moved the ladder into `low_address_reservation`; the policy
    // stays here. The unhinted request remains the last resort, and a result it
    // answers above 4 GiB is still accepted rather than refused, because the
    // engine's own "AOT code cache is outside the x86 address range" refusal is
    // the guard behind it and says so in words a caller can print.
    LowAddressReservationRequest request;
    request.candidate_bases = kAotCodeCacheCandidateBases;
    request.candidate_count = kCandidateCount;
    request.capacity = capacity;
    request.allow_unhinted_fallback = true;

    const LowAddressReservation reservation =
        ReserveLowAddressMemory(request);
    result.error = reservation.error;
    result.attempt = reservation.attempt;
    result.requested_base = reservation.requested_base;
    if (!reservation.valid)
    {
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
    LowAddressReservation released;
    released.valid = true;
    released.base = reservation.base;
    released.size = reservation.size;
    ReleaseLowAddressMemory(released);
}

}  // namespace repiu::runtime
