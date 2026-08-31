#include "code_cache_placement_probe.h"

#include "repiu/runtime/aot_code_cache_reservation.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::runtime::AotCodeCacheReservation;
using repiu::runtime::kAotCodeCacheCandidateBases;
using repiu::runtime::kUnhintedAttempt;
using repiu::runtime::ReleaseAotCodeCacheMemory;
using repiu::runtime::ReserveAotCodeCacheMemory;

// Task 554. Whether this host can put a code cache where the cache's own
// address fields can name it.
//
// A cache address is a host pointer the engine keeps in a `std::uint32_t`, and
// the engine refuses rather than truncating -- so on x86-64, where an unhinted
// 16 MiB mapping was measured landing at 0x00007fdd_f72e8000, no cache was
// placed at all. This is the reservation half of that, measured on its own so
// the check does not have to link the renderer to ask the question.
//
// Reported as addresses rather than as verdicts. A number can be compared
// across hosts and between runs; a boolean cannot say that i386 got what it
// always got while x64 now gets a candidate.

// The engine's own dynamic cache capacity, repeated rather than shared because
// it is a local constant of the placement path. If the two ever diverge this
// probe measures a smaller reservation than the engine asks for, which
// under-reports a refusal rather than inventing one.
constexpr std::size_t kDynamicCacheCapacity = 16U * 1024U * 1024U;

constexpr std::uint64_t kCacheAddressLimit = 0xFFFFFFFFULL;

bool ProbeOnce(const char* label, const std::size_t capacity)
{
    const AotCodeCacheReservation reservation =
        ReserveAotCodeCacheMemory(capacity);
    if (!reservation.valid)
    {
        std::cout << "  " << label << " reserved=0 error=" << reservation.error
                  << "\n";
        return false;
    }
    const auto base = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(reservation.base));
    const bool addressable = base <= kCacheAddressLimit;
    std::cout << "  " << label << " base=0x" << std::hex << base << std::dec
              << " size=" << reservation.size << " attempt=";
    if (reservation.attempt == kUnhintedAttempt)
    {
        std::cout << "unhinted";
    }
    else
    {
        std::cout << reservation.attempt << " requested=0x" << std::hex
                  << static_cast<std::uint64_t>(reservation.requested_base)
                  << std::dec;
    }
    std::cout << " addressable=" << (addressable ? 1 : 0) << "\n";
    ReleaseAotCodeCacheMemory(reservation);
    return addressable;
}

// Two at once, which is the case a single candidate would not survive. Nothing
// promises one placement per process -- a regenerated image is placed while the
// previous one is still mapped -- and the mapping refuses rather than
// displacing, so a ladder with one rung would fail the second time.
bool ProbeConcurrent()
{
    std::vector<AotCodeCacheReservation> held;
    bool ok = true;
    for (int index = 0; index < 2; ++index)
    {
        const AotCodeCacheReservation reservation =
            ReserveAotCodeCacheMemory(kDynamicCacheCapacity);
        const auto base = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(reservation.base));
        ok = ok && reservation.valid && base <= kCacheAddressLimit;
        std::cout << "  code_cache_concurrent_" << index << " base=0x"
                  << std::hex << base << std::dec << "\n";
        held.push_back(reservation);
    }
    // Distinct, which is the property that matters: two live caches must not be
    // handed the same pages.
    if (held.size() == 2U && held[0].base == held[1].base)
    {
        ok = false;
    }
    for (const AotCodeCacheReservation& reservation : held)
    {
        ReleaseAotCodeCacheMemory(reservation);
    }
    std::cout << "code_cache_placement_concurrent=" << (ok ? "true" : "false")
              << "\n";
    return ok;
}

}  // namespace

bool RunCodeCachePlacementProbe()
{
    std::cout << "  host_pointer_bytes=" << sizeof(void*)
              << " candidates=" << (sizeof(kAotCodeCacheCandidateBases) /
                                    sizeof(kAotCodeCacheCandidateBases[0]))
              << "\n";
    const bool dynamic_ok = ProbeOnce("code_cache_dynamic",
                                      kDynamicCacheCapacity);
    // A small image takes the same path; the capacity the engine asks for is
    // the maximum of the image size and the dynamic capacity, so this is the
    // same request in practice and is here to show the policy does not depend
    // on the size it is given.
    const bool small_ok = ProbeOnce("code_cache_small", 4096U);
    const bool concurrent_ok = ProbeConcurrent();

    const bool all = dynamic_ok && small_ok && concurrent_ok;
    std::cout << "code_cache_placement_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
