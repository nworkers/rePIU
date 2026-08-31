#ifndef REPIU_RUNTIME_AOT_CODE_CACHE_RESERVATION_H_
#define REPIU_RUNTIME_AOT_CODE_CACHE_RESERVATION_H_

#include <cstddef>
#include <cstdint>

namespace repiu::runtime
{

// Task 554. Where a code cache is allowed to live.
//
// A code cache address is a host pointer that the engine keeps in a
// `std::uint32_t` -- `AotCodeCachePlacement::base_address`, and the
// `FindAotGuestAddress` / `FindAotCacheAddress` pair that reads it back. On
// i386 that is the identity. On x86-64 it is a truncation, and the engine
// already refuses rather than truncating: a reservation above 4 GiB is rejected
// with "AOT code cache is outside the x86 address range".
//
// The kernel was then measured handing an unhinted 16 MiB anonymous mapping out
// at 0x00007fdd_f72e8000, so before this unit **an x86-64 host placed no cache
// at all**. The design records the measurement and why the answer is to place
// the cache low rather than widen 121 call sites:
// docs/design/20260901-554-linux-x64-code-cache-placement.md
//
// Separated from the placement code because it is a policy with one question --
// where may these bytes go -- and because a probe that links the engine drags
// in the whole renderer. Here it can be measured on its own on every host.

struct AotCodeCacheReservation
{
    bool valid = false;
    void* base = nullptr;
    std::size_t size = 0;
    // The host's error when nothing could be reserved.
    std::uint32_t error = 0;
    // Which candidate answered, so a run can say whether the first choice was
    // free or something already had it. `kUnhintedAttempt` means the ladder was
    // skipped or exhausted and the host chose the address.
    std::uint32_t attempt = 0;
    // What was asked for, which is zero for the unhinted request.
    std::uintptr_t requested_base = 0;
};

inline constexpr std::uint32_t kUnhintedAttempt = 0xFFFFFFFFU;

// The candidates, in order. They sit between the guest arena's top -- about
// 0x08600000 for the PIU profile, measured in Task 551 -- and the engine image
// at 0x40000000, which Task 503 fixed with `-Wl,-Ttext-segment`. There is more
// than one because nothing guarantees a single placement per process, and the
// mapping is MAP_FIXED_NOREPLACE, so an occupied candidate fails rather than
// overwriting whatever is there.
inline constexpr std::uintptr_t kAotCodeCacheCandidateBases[] = {
    0x20000000U,
    0x28000000U,
    0x30000000U,
    0x38000000U,
};

// Reserves read/write memory for a code cache image of `capacity` bytes.
//
// On a 32-bit host this is one unhinted request, which is what the engine did
// before this unit existed and already yields a 32-bit address by construction.
// A hint there could fail where the host would have succeeded, so there is none.
//
// On a 64-bit host the candidates above are tried in order first, and the
// unhinted request remains as the last resort -- where the engine's existing
// above-4-GiB refusal still stands as the final guard.
[[nodiscard]] AotCodeCacheReservation ReserveAotCodeCacheMemory(
    std::size_t capacity);

// Releases what the call above returned.
void ReleaseAotCodeCacheMemory(const AotCodeCacheReservation& reservation);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_CODE_CACHE_RESERVATION_H_
