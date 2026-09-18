#ifndef REPIU_RUNTIME_LOW_ADDRESS_RESERVATION_H_
#define REPIU_RUNTIME_LOW_ADDRESS_RESERVATION_H_

#include <cstddef>
#include <cstdint>

// Task 708. Host memory whose address has to fit a 32-bit field.
//
// Three places in this engine hand a host allocation's address to something
// that can only hold 32 bits of it, and on x86-64 the natural allocation is
// above 4 GiB in all three:
//
//   * the AOT code cache, through `AotCodeCachePlacement::base_address`
//     (Task 554)
//   * the shadow selector block, through a guard slot's
//     `cmp word ptr [disp32]` (Tasks 585 and 586)
//   * the Glide LFB staging surface, through `GrLfbInfo_t::lfbPtr`, which the
//     guest then dereferences (Task 708)
//
// The first two each grew their own copy of the same candidate ladder. This is
// that ladder, once, so the third did not become a third copy.
//
// What stays with the callers is the policy, because it genuinely differs.
// `allow_unhinted_fallback` is the one axis: the code cache keeps an unhinted
// last resort and its own above-4-GiB refusal behind it, while the shadow
// selector block has none, for the reason Task 586 recorded -- an unhinted
// `mmap` on x86-64 reliably answers above 4 GiB, so a "fallback" there is a
// failure that also has to release. Messages stay with the callers too: each
// names its own region, and a shared unit that formatted them would have to
// own strings it cannot word as well.

namespace repiu::runtime
{

// `kUnhintedAttempt` in `attempt` means no candidate answered -- either the
// ladder was skipped, as it is on a 32-bit host, or every candidate was taken.
inline constexpr std::uint32_t kLowAddressUnhintedAttempt = 0xFFFFFFFFU;

struct LowAddressReservationRequest
{
    // Tried in order. Ignored on a 32-bit host, where any address already fits.
    const std::uintptr_t* candidate_bases = nullptr;
    std::size_t candidate_count = 0;
    std::size_t capacity = 0;
    // Whether a 64-bit host may fall back to letting the allocator choose when
    // every candidate is taken. The result then reports `fits_32bit` rather
    // than being refused here, because only the caller knows whether an address
    // it cannot name is still worth having.
    bool allow_unhinted_fallback = false;
};

struct LowAddressReservation
{
    bool valid = false;
    void* base = nullptr;
    std::size_t size = 0;
    // Whether a 32-bit field can name every byte, the last one included. Always
    // true when a candidate answered and on a 32-bit host; only an unhinted
    // fallback on a 64-bit host can make it false.
    bool fits_32bit = false;
    // GetLastError on Windows, errno on POSIX. Zero when nothing failed.
    std::uint32_t error = 0;
    // Which candidate answered, so a run can say whether its first choice was
    // free or something already held it.
    std::uint32_t attempt = kLowAddressUnhintedAttempt;
    // What was asked for; zero for the unhinted request.
    std::uintptr_t requested_base = 0;
};

// Reserves committed read/write memory for `capacity` bytes.
//
// On a 64-bit host the candidates are tried in order first. The mapping is
// MAP_FIXED_NOREPLACE, so an occupied candidate fails rather than displacing
// whatever holds it -- but a host is also allowed to ignore a hint and answer
// elsewhere, so each answer is measured rather than assumed, and one that
// cannot be named in 32 bits is released rather than kept.
//
// On a 32-bit host there is one unhinted request and no ladder: a hint there
// could fail where the allocator would have succeeded, and every address it can
// return already fits.
[[nodiscard]] LowAddressReservation ReserveLowAddressMemory(
    const LowAddressReservationRequest& request);

// Releases what the call above returned. Safe on an invalid reservation.
void ReleaseLowAddressMemory(const LowAddressReservation& reservation);

// Whether a 32-bit field can name every byte of `capacity` bytes at `base`.
//
// Exposed because callers assert it too: the last byte is what has to fit, not
// the first, and a caller checking only `base <= UINT32_MAX` would accept a
// region running off the end of the 32-bit space.
[[nodiscard]] bool LowAddressFits32Bit(const void* base, std::size_t capacity);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_LOW_ADDRESS_RESERVATION_H_
