#include "virtual_memory_probe.h"

#include "repiu/platform/virtual_memory.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace repiu::tools
{
namespace
{

using repiu::platform::MemoryProtection;
using repiu::platform::MemoryRegion;
using repiu::platform::MemoryReservation;

// Large enough to span several pages on either host, so a protection change
// over part of a reservation actually has a part to leave alone.
constexpr std::size_t kReserveBytes = 64U * 1024U;

// Everything below is asserted identically on Windows and Linux. That is the
// point of this probe: it is not two implementations checked separately, it is
// one set of promises checked against both.

bool ProbeReserveAndQuery()
{
    const MemoryReservation reservation = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, true, MemoryProtection::kReadWrite);
    if (!reservation.valid || reservation.base == nullptr ||
        reservation.size < kReserveBytes || reservation.error != 0U)
    {
        return false;
    }

    bool ok = true;
    const MemoryRegion region = repiu::platform::QueryMemory(reservation.base);
    ok = ok && region.valid && region.committed && region.readable &&
        region.writable && !region.executable &&
        region.protection == MemoryProtection::kReadWrite &&
        region.base == reservation.base &&
        region.allocation_base == reservation.base &&
        region.size >= kReserveBytes;

    // Committed read-write memory has to actually accept a write, or the query
    // above was describing something other than reality.
    auto* bytes = static_cast<std::uint8_t*>(reservation.base);
    bytes[0] = 0x5AU;
    bytes[kReserveBytes - 1U] = 0xA5U;
    ok = ok && bytes[0] == 0x5AU && bytes[kReserveBytes - 1U] == 0xA5U;

    ok = ok && repiu::platform::IsRangeReadable(reservation.base, kReserveBytes);

    ok = ok && repiu::platform::ReleaseMemory(reservation.base,
                                              reservation.size);
    // After release the range must not read as committed. Windows still
    // describes the address as free space, Linux has nothing to describe, and
    // both agree on the part that matters.
    const MemoryRegion released =
        repiu::platform::QueryMemory(reservation.base);
    ok = ok && !released.committed;
    ok = ok && !repiu::platform::IsRangeReadable(reservation.base,
                                                 kReserveBytes);
    return ok;
}

bool ProbeProtectionRoundTrip()
{
    const MemoryReservation reservation = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, true, MemoryProtection::kReadWrite);
    if (!reservation.valid)
    {
        return false;
    }

    bool ok = true;
    MemoryProtection previous = MemoryProtection::kOther;
    ok = ok && repiu::platform::ProtectMemory(reservation.base, kReserveBytes,
                                              MemoryProtection::kReadOnly,
                                              &previous);
    // The protection that was replaced is what the guest store path restores,
    // so getting it wrong is not a reporting bug but a corruption bug.
    ok = ok && previous == MemoryProtection::kReadWrite;

    MemoryRegion region = repiu::platform::QueryMemory(reservation.base);
    ok = ok && region.readable && !region.writable && !region.executable &&
        region.protection == MemoryProtection::kReadOnly;

    ok = ok && repiu::platform::ProtectMemory(reservation.base, kReserveBytes,
                                              MemoryProtection::kExecuteRead,
                                              &previous);
    ok = ok && previous == MemoryProtection::kReadOnly;
    region = repiu::platform::QueryMemory(reservation.base);
    ok = ok && region.readable && !region.writable && region.executable &&
        region.protection == MemoryProtection::kExecuteRead;

    // Restoring is the whole reason previous is reported; check the value
    // actually round-trips back into ProtectMemory.
    ok = ok && repiu::platform::ProtectMemory(reservation.base, kReserveBytes,
                                              previous, nullptr);
    region = repiu::platform::QueryMemory(reservation.base);
    ok = ok && region.protection == MemoryProtection::kReadOnly;

    ok = ok && repiu::platform::ReleaseMemory(reservation.base,
                                              reservation.size);
    return ok;
}

// A protection change over part of a reservation must leave the rest alone.
// This is what the self-modifying-code detector does: it protects individual
// guest pages inside one large arena and restores them one at a time.
bool ProbePartialProtection()
{
    const MemoryReservation reservation = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, true, MemoryProtection::kReadWrite);
    if (!reservation.valid)
    {
        return false;
    }
    auto* base = static_cast<std::uint8_t*>(reservation.base);

    bool ok = true;
    // 4 KiB is the guest page size the engine works in, and both hosts use it
    // as their protection granularity.
    constexpr std::size_t kPageBytes = 4096U;
    MemoryProtection previous = MemoryProtection::kOther;
    ok = ok && repiu::platform::ProtectMemory(base, kPageBytes,
                                              MemoryProtection::kExecuteRead,
                                              &previous);
    ok = ok && previous == MemoryProtection::kReadWrite;

    const MemoryRegion protected_region = repiu::platform::QueryMemory(base);
    ok = ok && protected_region.protection == MemoryProtection::kExecuteRead;
    ok = ok && protected_region.size == kPageBytes;
    // The reservation it belongs to is still reported whole, which is what the
    // guest stack limit is read from.
    ok = ok && protected_region.allocation_base == reservation.base;

    const MemoryRegion untouched =
        repiu::platform::QueryMemory(base + kPageBytes);
    ok = ok && untouched.valid && untouched.committed && untouched.writable &&
        untouched.protection == MemoryProtection::kReadWrite;
    ok = ok && untouched.allocation_base == reservation.base;
    // The neighbour is still writable in fact, not just on paper.
    base[kPageBytes] = 0x3CU;
    ok = ok && base[kPageBytes] == 0x3CU;

    ok = ok && repiu::platform::ProtectMemory(base, kPageBytes, previous,
                                              nullptr);
    ok = ok && repiu::platform::QueryMemory(base).protection ==
            MemoryProtection::kReadWrite;

    ok = ok && repiu::platform::ReleaseMemory(reservation.base,
                                              reservation.size);
    return ok;
}

bool ProbeUncommittedReservation()
{
    const MemoryReservation reservation = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, false, MemoryProtection::kReadWrite);
    if (!reservation.valid)
    {
        return false;
    }
    bool ok = true;
    const MemoryRegion region = repiu::platform::QueryMemory(reservation.base);
    ok = ok && region.valid && !region.committed && !region.readable &&
        !region.writable;
    ok = ok && !repiu::platform::IsRangeReadable(reservation.base, 1U);

    // Committing afterwards makes the range usable for real. This must go
    // through CommitMemory: on Windows ProtectMemory fails here, and writing
    // anyway faults.
    ok = ok && repiu::platform::CommitMemory(reservation.base, kReserveBytes,
                                             MemoryProtection::kReadWrite);
    const MemoryRegion committed =
        repiu::platform::QueryMemory(reservation.base);
    ok = ok && committed.committed && committed.writable;
    if (committed.committed && committed.writable)
    {
        // Guarded, because a probe that faults takes every later probe with it
        // and reports nothing at all.
        static_cast<std::uint8_t*>(reservation.base)[0] = 0x7EU;
        ok = ok && static_cast<std::uint8_t*>(reservation.base)[0] == 0x7EU;
    }

    ok = ok && repiu::platform::ReleaseMemory(reservation.base,
                                              reservation.size);
    return ok;
}

bool ProbeRefusals()
{
    bool ok = true;
    // kOther describes what a host reported; asking for it is a caller error,
    // and silently substituting a real protection would be worse than failing.
    const MemoryReservation other = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, true, MemoryProtection::kOther);
    ok = ok && !other.valid;

    const MemoryReservation empty = repiu::platform::ReserveMemory(
        nullptr, 0U, true, MemoryProtection::kReadWrite);
    ok = ok && !empty.valid;

    ok = ok && !repiu::platform::ReleaseMemory(nullptr, kReserveBytes);
    ok = ok && !repiu::platform::ProtectMemory(nullptr, kReserveBytes,
                                               MemoryProtection::kReadWrite,
                                               nullptr);
    ok = ok && !repiu::platform::IsRangeReadable(nullptr, 1U);

    const MemoryReservation reservation = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, true, MemoryProtection::kReadWrite);
    if (!reservation.valid)
    {
        return false;
    }
    ok = ok && !repiu::platform::IsRangeReadable(reservation.base, 0U);
    ok = ok && !repiu::platform::ProtectMemory(reservation.base, 0U,
                                               MemoryProtection::kReadWrite,
                                               nullptr);
    ok = ok && !repiu::platform::ProtectMemory(reservation.base, kReserveBytes,
                                               MemoryProtection::kOther,
                                               nullptr);

    // Asking for an address that is already taken must not quietly hand back
    // the occupied one. Windows relocates, Linux refuses; either is fine, and
    // returning the occupied address is not.
    const MemoryReservation collision = repiu::platform::ReserveMemory(
        reservation.base, kReserveBytes, true, MemoryProtection::kReadWrite);
    ok = ok && (!collision.valid || collision.base != reservation.base);
    if (collision.valid)
    {
        ok = ok && repiu::platform::ReleaseMemory(collision.base,
                                                  collision.size);
    }

    ok = ok && repiu::platform::ReleaseMemory(reservation.base,
                                              reservation.size);
    return ok;
}

// A range that starts inside mapped memory and runs past its end must be
// rejected. Every caller of this uses it to decide whether dereferencing a
// pointer is safe, so a single-region check would be the bug it is meant to
// prevent.
bool ProbeRangeSpansRegions()
{
    const MemoryReservation reservation = repiu::platform::ReserveMemory(
        nullptr, kReserveBytes, true, MemoryProtection::kReadWrite);
    if (!reservation.valid)
    {
        return false;
    }
    auto* base = static_cast<std::uint8_t*>(reservation.base);
    bool ok = true;

    ok = ok && repiu::platform::IsRangeReadable(base, reservation.size);
    // Deliberately not asserted: that a range running past the end of the
    // reservation is unreadable. Whatever follows belongs to someone else and
    // may well be mapped and readable, on either host. The hole punched below
    // is the portable way to make a range span into unreadable memory.

    // A range wholly inside a protected sub-page stays readable, because
    // execute-read is still readable.
    constexpr std::size_t kPageBytes = 4096U;
    ok = ok && repiu::platform::ProtectMemory(base, kPageBytes,
                                              MemoryProtection::kExecuteRead,
                                              nullptr);
    ok = ok && repiu::platform::IsRangeReadable(base, reservation.size);

    // Made unreadable, the same range must now be refused -- including a range
    // that merely starts there and continues into readable memory.
    ok = ok && repiu::platform::ProtectMemory(base, kPageBytes,
                                              MemoryProtection::kNoAccess,
                                              nullptr);
    ok = ok && !repiu::platform::IsRangeReadable(base, kPageBytes);
    ok = ok && !repiu::platform::IsRangeReadable(base, reservation.size);
    // Starting after the hole is readable again.
    ok = ok && repiu::platform::IsRangeReadable(base + kPageBytes,
                                                reservation.size - kPageBytes);

    ok = ok && repiu::platform::ProtectMemory(base, kPageBytes,
                                              MemoryProtection::kReadWrite,
                                              nullptr);
    ok = ok && repiu::platform::ReleaseMemory(reservation.base,
                                              reservation.size);
    return ok;
}

// The host's own stack is memory this project never mapped. It exercises the
// path that has no shortcut on Linux -- reading /proc/self/maps -- and it must
// answer the same way as Windows.
bool ProbeForeignMemory()
{
    volatile std::uint8_t on_stack[64] = {};
    on_stack[0] = 1U;
    const void* address = const_cast<const std::uint8_t*>(&on_stack[0]);

    bool ok = true;
    const MemoryRegion region = repiu::platform::QueryMemory(address);
    ok = ok && region.valid && region.committed && region.readable &&
        region.writable;
    ok = ok && repiu::platform::IsRangeReadable(address, sizeof(on_stack));

    // An address no process maps. Both hosts must describe it as not committed
    // and refuse to call it readable.
    const auto* unmapped = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(0x10U));
    const MemoryRegion missing = repiu::platform::QueryMemory(unmapped);
    ok = ok && !missing.committed;
    ok = ok && !repiu::platform::IsRangeReadable(unmapped, 4U);
    return ok;
}

}  // namespace

bool RunVirtualMemoryProbe()
{
    const bool reserve_ok = ProbeReserveAndQuery();
    const bool protection_ok = ProbeProtectionRoundTrip();
    const bool partial_ok = ProbePartialProtection();
    const bool uncommitted_ok = ProbeUncommittedReservation();
    const bool refusal_ok = ProbeRefusals();
    const bool span_ok = ProbeRangeSpansRegions();
    const bool foreign_ok = ProbeForeignMemory();
    const bool all = reserve_ok && protection_ok && partial_ok &&
        uncommitted_ok && refusal_ok && span_ok && foreign_ok;

    std::cout << "virtual_memory_reserve_query=" << (reserve_ok ? "true" : "false")
              << "\nvirtual_memory_protection_round_trip="
              << (protection_ok ? "true" : "false")
              << "\nvirtual_memory_partial_protection="
              << (partial_ok ? "true" : "false")
              << "\nvirtual_memory_uncommitted_reservation="
              << (uncommitted_ok ? "true" : "false")
              << "\nvirtual_memory_refusals=" << (refusal_ok ? "true" : "false")
              << "\nvirtual_memory_range_spans_regions="
              << (span_ok ? "true" : "false")
              << "\nvirtual_memory_foreign_memory="
              << (foreign_ok ? "true" : "false")
              << "\nvirtual_memory_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
