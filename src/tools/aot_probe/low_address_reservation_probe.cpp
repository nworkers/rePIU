#include "low_address_reservation_probe.h"

#include "repiu/runtime/low_address_reservation.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

namespace repiu::tools
{
namespace
{

using repiu::runtime::kLowAddressUnhintedAttempt;
using repiu::runtime::LowAddressFits32Bit;
using repiu::runtime::LowAddressReservation;
using repiu::runtime::LowAddressReservationRequest;
using repiu::runtime::ReleaseLowAddressMemory;
using repiu::runtime::ReserveLowAddressMemory;

constexpr std::size_t kCapacity = 64U * 1024U;

// Deliberately not the bases any real consumer uses. A probe that reserved the
// code cache's or the shadow selector block's candidates would take them away
// from whatever ran next in the same process.
constexpr std::uintptr_t kProbeCandidateBases[] = {
    0x14000000U,
    0x16000000U,
    0x18000000U,
};

constexpr std::size_t kProbeCandidateCount =
    sizeof(kProbeCandidateBases) / sizeof(kProbeCandidateBases[0]);

constexpr bool kHostPointerExceeds32Bit = sizeof(void*) > 4U;

LowAddressReservationRequest ProbeRequest(const bool allow_fallback)
{
    LowAddressReservationRequest request;
    request.candidate_bases = kProbeCandidateBases;
    request.candidate_count = kProbeCandidateCount;
    request.capacity = kCapacity;
    request.allow_unhinted_fallback = allow_fallback;
    return request;
}

// The fit test is the reason this unit exists, and it is pure, so it is checked
// without reserving anything.
bool ProbeFitTest()
{
    constexpr std::uintptr_t kLimit = std::numeric_limits<std::uint32_t>::max();
    const auto at = [](const std::uintptr_t address) {
        return reinterpret_cast<const void*>(address);
    };
    if (LowAddressFits32Bit(nullptr, 1U))
    {
        return false;
    }
    if (!LowAddressFits32Bit(at(0x1000U), 0x1000U))
    {
        return false;
    }
    // The last byte exactly reaches the end of the 32-bit space.
    if (!LowAddressFits32Bit(at(kLimit - 3U), 4U))
    {
        return false;
    }
    // One byte past it. A first-byte-only test would accept this, which is the
    // hole Task 708 closed when the three consumers were brought together.
    if (LowAddressFits32Bit(at(kLimit - 3U), 5U))
    {
        return false;
    }
    if (kHostPointerExceeds32Bit &&
        LowAddressFits32Bit(at(kLimit + 1U), 1U))
    {
        return false;
    }
    return true;
}

// A candidate answers, and what it answers can be named in 32 bits.
bool ProbeCandidateAnswers()
{
    const LowAddressReservation reservation =
        ReserveLowAddressMemory(ProbeRequest(false));
    if (!reservation.valid || reservation.base == nullptr ||
        !reservation.fits_32bit ||
        !LowAddressFits32Bit(reservation.base, kCapacity))
    {
        ReleaseLowAddressMemory(reservation);
        return false;
    }
    // On a 64-bit host the ladder is the only way to get a low address, so a
    // candidate must be what answered. On a 32-bit host the ladder is skipped
    // by design and the unhinted request is the whole policy.
    const bool attempt_ok = kHostPointerExceeds32Bit
        ? (reservation.attempt < kProbeCandidateCount &&
           reservation.requested_base ==
               kProbeCandidateBases[reservation.attempt])
        : (reservation.attempt == kLowAddressUnhintedAttempt &&
           reservation.requested_base == 0U);
    // The memory is real: writing the first and last byte would fault if the
    // reservation were short or uncommitted.
    auto* const bytes = static_cast<volatile std::uint8_t*>(reservation.base);
    bytes[0] = 0xA5U;
    bytes[kCapacity - 1U] = 0x5AU;
    const bool written = bytes[0] == 0xA5U && bytes[kCapacity - 1U] == 0x5AU;
    ReleaseLowAddressMemory(reservation);
    return attempt_ok && written;
}

// Every candidate taken. Holding all of them and asking again separates the two
// policies: without a fallback the answer is a refusal, with one it is an
// address the caller has to judge for itself.
bool ProbeExhaustedLadder()
{
    if constexpr (!kHostPointerExceeds32Bit)
    {
        // A 32-bit host never consults the ladder, so there is nothing to
        // exhaust and both policies are the same single request.
        return true;
    }
    else
    {
        LowAddressReservation held[kProbeCandidateCount];
        for (std::size_t index = 0; index < kProbeCandidateCount; ++index)
        {
            held[index] = ReserveLowAddressMemory(ProbeRequest(false));
            if (!held[index].valid)
            {
                for (std::size_t undo = 0; undo < index; ++undo)
                {
                    ReleaseLowAddressMemory(held[undo]);
                }
                return false;
            }
        }

        const LowAddressReservation refused =
            ReserveLowAddressMemory(ProbeRequest(false));
        const bool refusal_ok = !refused.valid && refused.base == nullptr;

        const LowAddressReservation fallback =
            ReserveLowAddressMemory(ProbeRequest(true));
        // The fallback answers, and says whether what it answered fits rather
        // than deciding for the caller. Its `fits_32bit` must agree with the
        // address itself either way.
        const bool fallback_ok = fallback.valid &&
            fallback.attempt == kLowAddressUnhintedAttempt &&
            fallback.requested_base == 0U &&
            fallback.fits_32bit ==
                LowAddressFits32Bit(fallback.base, kCapacity);
        ReleaseLowAddressMemory(fallback);

        for (std::size_t index = 0; index < kProbeCandidateCount; ++index)
        {
            ReleaseLowAddressMemory(held[index]);
        }
        return refusal_ok && fallback_ok;
    }
}

bool ProbeRefusals()
{
    LowAddressReservationRequest empty = ProbeRequest(true);
    empty.capacity = 0U;
    const LowAddressReservation nothing = ReserveLowAddressMemory(empty);
    if (nothing.valid)
    {
        ReleaseLowAddressMemory(nothing);
        return false;
    }
    // Releasing an invalid reservation is a no-op rather than a fault: the
    // failure paths in all three consumers call it on the way out.
    ReleaseLowAddressMemory(LowAddressReservation{});
    return true;
}

}  // namespace

bool RunLowAddressReservationProbe()
{
    const bool fit_ok = ProbeFitTest();
    const bool candidate_ok = ProbeCandidateAnswers();
    const bool exhausted_ok = ProbeExhaustedLadder();
    const bool refusal_ok = ProbeRefusals();
    const bool all = fit_ok && candidate_ok && exhausted_ok && refusal_ok;
    std::cout << "low_address_reservation_fit=" << (fit_ok ? "true" : "false")
              << "\nlow_address_reservation_candidate="
              << (candidate_ok ? "true" : "false")
              << "\nlow_address_reservation_exhausted_ladder="
              << (exhausted_ok ? "true" : "false")
              << "\nlow_address_reservation_refusals="
              << (refusal_ok ? "true" : "false")
              << "\nlow_address_reservation_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
