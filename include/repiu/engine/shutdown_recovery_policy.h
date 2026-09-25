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

// Task 733. What to do with an exception that reaches the guest thread after
// the shutdown path redirected it.
//
// On Windows the redirect is a SetThreadContext on a suspended thread. The
// legacy backend single-steps the guest, so the thread is often suspended
// while the kernel is already delivering a single-step exception, and the
// rewrite does not cancel it. After resume that exception arrives anyway --
// either carrying the redirected context, when the engine's handler would then
// run on the small host stack and overflow its guard page, or carrying the old
// guest context, when the redirect is silently lost.
enum class ShutdownRedirectGuardAction : std::uint8_t
{
    kPass = 0,
    // The in-flight single step arrived at the recovery entry: resume there
    // without running the engine's handler on the host stack.
    kReapplyAtEntry,
    // The exception arrived at guest code: the redirect was lost, apply it again.
    kReapplyFromGuest,
};

// How many times an exception reported at the recovery entry is sent back to
// it. The entry's first instruction reads a global through CS and cannot fault,
// so an exception reported there is always one that was already in flight --
// but a bound keeps a case nobody foresaw from looping instead of failing.
inline constexpr std::uint32_t kShutdownRedirectGuardEntryLimit = 8U;

struct ShutdownRedirectGuardInput
{
    bool redirect_applied = false;
    bool on_guest_thread = false;
    bool at_recovery_entry = false;
    // Guest image or AOT cache.
    bool in_guest_code = false;
    // Reapplications at the entry already made in this shutdown.
    std::uint32_t entry_reapplies = 0;
};

// Only the two shapes above are taken, whatever the exception code: the legacy
// backend's in-flight exception is usually a single step, but emulated
// instructions raise access violations and privileged-instruction faults too.
ShutdownRedirectGuardAction DecideShutdownRedirectGuard(
    const ShutdownRedirectGuardInput& input);

const char* ShutdownRedirectGuardActionName(ShutdownRedirectGuardAction action);

}  // namespace repiu::engine
