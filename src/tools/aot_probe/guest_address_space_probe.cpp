#include "guest_address_space_probe.h"

#include "repiu/platform/virtual_memory.h"
#include "repiu/target/target_profile.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#if !defined(_WIN32)
#include <cstdio>
#endif

namespace repiu::tools
{
namespace
{

using repiu::platform::MemoryProtection;
using repiu::platform::MemoryReservation;

// Task 551. Whether this host can give the guest the addresses the guest's own
// pointers name.
//
// The guest is relocated to a 32-bit base and its relocations are then written
// against that base, so the arena is not somewhere the host may choose: the
// guest's own pointers already name it. On a 32-bit host that is automatic. On
// x86-64 it is a request the kernel can refuse, and refusing it is the end of
// the port rather than a slow path -- which makes it worth measuring rather
// than assuming.
//
// This settles the half of Task 546's decision 4 that is about guest memory. It
// says nothing about host pointers, which are measured separately below and
// cannot be placed low at all.

// The slack the Win32 host adds to the profile's hint when it builds the arena
// plan (`kRuntimeArenaExpansionSlack`). Repeated rather than shared because it
// is a local constant of that host's main; if the two ever disagree this probe
// measures a smaller range than the host asks for, which is the direction that
// fails safely -- it would under-report a refusal, never invent one.
constexpr std::uint64_t kArenaExpansionSlack = 0x08000000ULL;

struct Request
{
    std::uint32_t base = 0;
    std::uint64_t size = 0;
};

// One entry per distinct base/size the built-in profiles ask for. Taken from
// the profiles themselves rather than copied, so the numbers cannot drift away
// from the ones the host would use.
std::vector<Request> CollectRequests()
{
    std::vector<Request> requests;
    for (const target::TargetProfile& profile :
         target::GetBuiltInTargetProfiles())
    {
        const target::TargetRuntimeReservationHint& hint =
            profile.runtime_reservation_hint;
        if (!hint.valid || hint.reserve_size == 0U)
        {
            continue;
        }
        const Request candidate{
            hint.base_address,
            static_cast<std::uint64_t>(hint.reserve_size) +
                kArenaExpansionSlack};
        bool already = false;
        for (const Request& seen : requests)
        {
            already = already ||
                (seen.base == candidate.base && seen.size == candidate.size);
        }
        if (!already)
        {
            requests.push_back(candidate);
        }
    }
    return requests;
}

// The contract, which is the part that must hold on every host: a reservation
// at the guest's base either lands exactly there or does not count. A host that
// quietly returned a different address would hand the guest an arena its own
// relocated pointers do not name, and every guest pointer would be wrong by a
// constant nobody recorded.
//
// Whether the exact placement *succeeds* is the measurement, reported either
// way. On a 32-bit host it is not in doubt; on x86-64 it is the question.
bool ProbeGuestArenaPlacement(bool* placed_all)
{
    bool contract_ok = true;
    bool placed_every = true;
    for (const Request& request : CollectRequests())
    {
        if (request.size > std::numeric_limits<std::size_t>::max())
        {
            continue;
        }
        void* const requested = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(request.base));
        const MemoryReservation reserved = platform::ReserveMemory(
            requested, static_cast<std::size_t>(request.size), false,
            MemoryProtection::kReadWrite);

        const bool exact = reserved.valid && reserved.base == requested;
        if (reserved.valid && reserved.base != nullptr && !exact)
        {
            // Refused by being answered somewhere else. Released rather than
            // kept, exactly as the host's own placement path does.
            platform::ReleaseMemory(reserved.base,
                                    static_cast<std::size_t>(request.size));
            contract_ok = false;
        }
        if (exact)
        {
            platform::ReleaseMemory(reserved.base,
                                    static_cast<std::size_t>(request.size));
        }
        placed_every = placed_every && exact;

        // Why a refusal happened, when the reason is one this binary carries
        // itself. A range the guest wants is unavailable if this executable is
        // loaded inside it, and that is a property of the *binary asking*
        // rather than of the host.
        //
        // It is the ordinary case for a probe: the Linux binaries are linked at
        // 0x40000000 on purpose, clear of the guest's range, but a plain MSVC
        // executable is based at 0x400000, which is inside it. The Win32 loader
        // host sets /BASE:0x10000000 for exactly this reason and does not
        // collide. Without this line, `placed=false` here reads as "Windows
        // cannot place the guest", which is not what it means.
        const auto own_code = reinterpret_cast<std::uintptr_t>(
            &ProbeGuestArenaPlacement);
        const bool overlaps_own_image =
            own_code >= static_cast<std::uintptr_t>(request.base) &&
            own_code < static_cast<std::uintptr_t>(request.base) +
                static_cast<std::uintptr_t>(request.size);

        std::cout << "  guest_arena base=0x" << std::hex << request.base
                  << " size=0x" << request.size << std::dec
                  << " placed=" << (exact ? "true" : "false")
                  << " host_error=" << reserved.error
                  << " overlaps_own_image=" << (overlaps_own_image ? 1 : 0)
                  << "\n";
    }
    if (placed_all != nullptr)
    {
        *placed_all = placed_every;
    }
    std::cout << "guest_address_space_exact_or_refused="
              << (contract_ok ? "true" : "false")
              << "\nguest_address_space_arena_placed="
              << (placed_every ? "true" : "false") << "\n";
    return contract_ok;
}

// How much room the lowest guest base has above the kernel's floor. The PIU
// profiles ask for 0x00010000, and the default `vm.mmap_min_addr` is 65536 --
// the same number. There is no margin there at all, so a host configured even
// slightly stricter cannot place the guest, and this reports the headroom
// rather than leaving it to be rediscovered from a failed reservation.
void ReportLowAddressFloor()
{
#if !defined(_WIN32)
    unsigned long long floor_value = 0;
    std::FILE* const file = std::fopen("/proc/sys/vm/mmap_min_addr", "r");
    if (file == nullptr)
    {
        std::cout << "guest_address_space_mmap_min_addr=unknown\n";
        return;
    }
    const bool read = std::fscanf(file, "%llu", &floor_value) == 1;
    std::fclose(file);
    if (!read)
    {
        std::cout << "guest_address_space_mmap_min_addr=unknown\n";
        return;
    }

    std::uint32_t lowest = 0xFFFFFFFFU;
    for (const Request& request : CollectRequests())
    {
        lowest = request.base < lowest ? request.base : lowest;
    }
    std::cout << "guest_address_space_mmap_min_addr=" << floor_value
              << ",lowest_guest_base=" << lowest << ",headroom="
              << (static_cast<unsigned long long>(lowest) >= floor_value
                      ? static_cast<unsigned long long>(lowest) - floor_value
                      : 0ULL)
              << (static_cast<unsigned long long>(lowest) >= floor_value
                      ? ""
                      : " (BELOW THE FLOOR)")
              << "\n";
#else
    std::cout << "guest_address_space_mmap_min_addr=n/a\n";
#endif
}

// Whether any code this process can execute lives above 4 GiB.
//
// The engine's own image does not: Task 503 links the Linux binaries
// `-no-pie -Wl,-Ttext-segment=0x40000000`, deliberately, so the loader's text
// sits below 4 GiB and outside the guest's relocation range. That much is
// arranged rather than accidental, and it carries to x86-64 unchanged.
//
// Shared libraries are the part nobody arranges. `ld.so` maps libc and
// libstdc++ where it likes, and a driver the runtime opens for itself later --
// Mesa, libGL -- is mapped after the build has no say at all. Scanning the
// process's own map is the honest way to ask, because it counts what is really
// there rather than what a symbol's address happens to resolve to through a
// PLT stub in the executable.
bool AnyExecutableMappingAboveFourGiB(std::uint64_t* highest)
{
#if !defined(_WIN32)
    constexpr std::uint64_t kFourGiB = 0x100000000ULL;
    std::FILE* const file = std::fopen("/proc/self/maps", "r");
    if (file == nullptr)
    {
        return false;
    }
    bool found = false;
    char line[512];
    while (std::fgets(line, sizeof(line), file) != nullptr)
    {
        unsigned long long start = 0;
        unsigned long long end = 0;
        char permissions[8] = {0};
        if (std::sscanf(line, "%llx-%llx %7s", &start, &end, permissions) != 3)
        {
            continue;
        }
        if (permissions[2] != 'x' || start < kFourGiB)
        {
            continue;
        }
        found = true;
        if (highest != nullptr && start > *highest)
        {
            *highest = start;
        }
    }
    std::fclose(file);
    return found;
#else
    (void)highest;
    return false;
#endif
}

// The other half of decision 4: where the host's own pointers live.
//
// This reports rather than asserts, because the answer differs by host and
// neither answer is a failure. On a 32-bit host everything is below 4 GiB by
// construction. What it prints on x64 is the evidence for which host pointers
// may be patched as 32-bit immediates and which may not.
void ReportHostPointerPlacement()
{
    constexpr std::uint64_t kFourGiB = 0x100000000ULL;
    const auto above = [](const void* pointer) {
        return static_cast<std::uint64_t>(
                   reinterpret_cast<std::uintptr_t>(pointer)) >= kFourGiB;
    };

    const void* const code =
        reinterpret_cast<const void*>(&ReportHostPointerPlacement);
    int stack_marker = 0;
    void* const heap = std::malloc(64U);

    std::cout << "  host_pointer engine_code_above_4gib="
              << (above(code) ? 1 : 0)
              << " stack_above_4gib=" << (above(&stack_marker) ? 1 : 0)
              << " heap_above_4gib="
              << (heap != nullptr && above(heap) ? 1 : 0) << "\n";
    std::free(heap);

    std::uint64_t highest = 0;
    const bool library_high = AnyExecutableMappingAboveFourGiB(&highest);
    std::cout << "  host_pointer executable_mapping_above_4gib="
              << (library_high ? 1 : 0) << " highest=0x" << std::hex << highest
              << std::dec << "\n";

    std::cout << "guest_address_space_host_pointer_bits="
              << (sizeof(void*) * 8U) << "\n";
}

}  // namespace

bool RunGuestAddressSpaceProbe()
{
    bool placed = false;
    const bool contract_ok = ProbeGuestArenaPlacement(&placed);
    ReportLowAddressFloor();
    ReportHostPointerPlacement();

    // The contract is the assertion; the placement is the measurement. A host
    // that cannot place the guest has said something true about itself, and
    // saying it should not turn into a failing probe everywhere else.
    std::cout << "guest_address_space_all=" << (contract_ok ? "true" : "false")
              << "\n";
    return contract_ok;
}

}  // namespace repiu::tools
