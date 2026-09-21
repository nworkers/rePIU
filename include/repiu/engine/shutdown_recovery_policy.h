#pragma once

#include <cstdint>

namespace repiu::engine
{

// Task 730. Whether the shutdown path may redirect the guest thread into the
// recovery exit from where a signal found it.
//
// The recovery exit unwinds a guest or AOT-cache frame, so it is only valid
// from guest code or the cache. The check used to read `GuestCpuContext::Eip`,
// which on an x64 host is the low 32 bits of RIP. A thread inside a host
// library at 0x7f..... whose low half happened to fall in the guest image or
// the cache was therefore "recovered" from a host frame, and the exit
// trampoline unwound into garbage -- the teardown segfault seen at budget
// expiry since before Task 728.
//
// Guest code and the cache are placed below 4 GiB by design, so an address with
// nonzero upper bits is never guest code. The decision takes the full native
// address where the host has one wider than 32 bits, and refuses without it.
//
// Called from a signal handler: no allocation, no locking.
enum class ShutdownRecoveryDecision : std::uint8_t
{
    kRecover = 0,
    // The low half is not in guest code or the cache.
    kOutsideGuestCode,
    // The low half is in guest code or the cache, but the full address is a
    // host address above 4 GiB. The case the 32-bit check recovered wrongly.
    kAliasedHostAddress,
    // The host needs the full address and could not read it.
    kHostAddressUnavailable,
};

struct ShutdownRecoveryPosition
{
    // True where the native instruction pointer is wider than the guest ABI's
    // 32-bit Eip, so the low half alone cannot place the thread.
    bool host_address_required = false;
    bool host_address_known = false;
    std::uintptr_t host_address = 0;
    // The existing range check on the low half: guest image or AOT cache.
    bool low_half_in_guest_code = false;
};

// Whether a native address can name guest code at all.
bool HostAddressFitsGuestSpace(std::uintptr_t address);

ShutdownRecoveryDecision DecideShutdownRecovery(
    const ShutdownRecoveryPosition& position);

const char* ShutdownRecoveryDecisionName(ShutdownRecoveryDecision decision);

}  // namespace repiu::engine
