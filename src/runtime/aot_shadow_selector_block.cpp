#include "repiu/runtime/aot_shadow_selector_block.h"

#include "repiu/runtime/low_address_reservation.h"

#include <new>

namespace repiu::runtime
{
namespace
{

constexpr std::size_t kShadowSelectorPageBytes = 4096U;

constexpr std::size_t kCandidateCount =
    sizeof(kAotShadowSelectorCandidateBases) /
    sizeof(kAotShadowSelectorCandidateBases[0]);

// Whether this host's pointers can name an address the guard operand cannot.
// A value rather than a preprocessor branch, so both messages below stay
// compiled on every host.
constexpr bool kHostPointerExceeds32Bit = sizeof(void*) > 4U;

AotShadowSelectorReservation AdoptReservation(
    const LowAddressReservation& reservation)
{
    AotShadowSelectorReservation result;
    result.valid = true;
    result.base = reservation.base;
    result.size = reservation.size;
    result.block = static_cast<AotShadowSelectorBlock*>(reservation.base);
    new (result.block) AotShadowSelectorBlock();
    result.message = "shadow selector block reserved";
    return result;
}

}  // namespace

AotShadowSelectorReservation ReserveAotShadowSelectorBlock()
{
    // Task 708 moved the ladder into `low_address_reservation`; the policy
    // stays here. There is deliberately no unhinted last resort on a 64-bit
    // host: Task 586 measured that an unhinted `mmap` there answers above
    // 4 GiB, so it is not a fallback but a failure that also has to release.
    // On a 32-bit host the shared unit skips the ladder and makes that one
    // unhinted request, which is the whole policy there.
    LowAddressReservationRequest request;
    request.candidate_bases = kAotShadowSelectorCandidateBases;
    request.candidate_count = kCandidateCount;
    // A whole page, because the host reserves in pages regardless and asking
    // for the block's size alone would leave the rest of it unaccounted for at
    // release. The fit test then covers the page, which is stricter than the
    // block Task 586 tested and correct for the same reason.
    request.capacity = kShadowSelectorPageBytes;
    request.allow_unhinted_fallback = false;

    const LowAddressReservation reservation =
        ReserveLowAddressMemory(request);
    if (reservation.valid && reservation.fits_32bit)
    {
        return AdoptReservation(reservation);
    }
    if (reservation.valid)
    {
        ReleaseLowAddressMemory(reservation);
    }

    AotShadowSelectorReservation result;
    result.message = kHostPointerExceeds32Bit
        ? "no address below 4GiB was available for the shadow selector block"
        : "shadow selector block reservation failed";
    return result;
}

void ReleaseAotShadowSelectorBlock(
    const AotShadowSelectorReservation& reservation)
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
