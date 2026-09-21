#include "shutdown_recovery_policy_probe.h"

#include "repiu/engine/shutdown_recovery_policy.h"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace repiu::tools
{
namespace
{

using engine::DecideShutdownRecovery;
using engine::ShutdownRecoveryDecision;
using engine::ShutdownRecoveryPosition;

ShutdownRecoveryPosition X64(const std::uint64_t address,
                             const bool low_half_in_guest_code)
{
    ShutdownRecoveryPosition position;
    position.host_address_required = true;
    position.host_address_known = true;
    position.host_address = static_cast<std::uintptr_t>(address);
    position.low_half_in_guest_code = low_half_in_guest_code;
    return position;
}

}  // namespace

bool RunShutdownRecoveryPolicyProbe()
{
    const bool wide = sizeof(std::uintptr_t) > 4U;

    // A thread really in the AOT cache or the guest image: recovered.
    const bool recovers_guest =
        DecideShutdownRecovery(X64(0x20068B6AULL, true)) ==
            ShutdownRecoveryDecision::kRecover &&
        DecideShutdownRecovery(X64(0x0103F139ULL, true)) ==
            ShutdownRecoveryDecision::kRecover;

    // The defect: a host library address whose low half lands in the guest
    // image. The 32-bit check saw 0x01234567 and recovered from a host frame.
    // Only meaningful where the pointer is wider than 32 bits.
    const bool refuses_alias = !wide ||
        DecideShutdownRecovery(X64(0x00007F0001234567ULL, true)) ==
            ShutdownRecoveryDecision::kAliasedHostAddress;

    // A host address whose low half is outside guest code is refused as before,
    // and is not counted as an alias.
    const bool refuses_host = !wide ||
        DecideShutdownRecovery(X64(0x00007F82A5E43E4FULL, false)) ==
            ShutdownRecoveryDecision::kOutsideGuestCode;

    // The 4 GiB boundary. The last byte below it can still be guest code; the
    // first byte at it cannot.
    const bool boundary =
        engine::HostAddressFitsGuestSpace(
            static_cast<std::uintptr_t>(0xFFFFFFFFULL)) &&
        (!wide ||
         (!engine::HostAddressFitsGuestSpace(
              static_cast<std::uintptr_t>(0x100000000ULL)) &&
          DecideShutdownRecovery(X64(0xFFFFFFFFULL, true)) ==
              ShutdownRecoveryDecision::kRecover &&
          DecideShutdownRecovery(X64(0x100000000ULL, true)) ==
              ShutdownRecoveryDecision::kAliasedHostAddress));

    // Without the full address a host that needs one refuses, whatever the low
    // half says, and zero counts as not having one.
    ShutdownRecoveryPosition unknown = X64(0x20000000ULL, true);
    unknown.host_address_known = false;
    const bool refuses_unknown =
        DecideShutdownRecovery(unknown) ==
            ShutdownRecoveryDecision::kHostAddressUnavailable &&
        DecideShutdownRecovery(X64(0ULL, true)) ==
            ShutdownRecoveryDecision::kHostAddressUnavailable;

    // A 32-bit host keeps the existing decision: the low half is the address.
    ShutdownRecoveryPosition narrow;
    narrow.host_address_required = false;
    narrow.host_address_known = true;
    narrow.host_address = static_cast<std::uintptr_t>(0x04012345U);
    narrow.low_half_in_guest_code = true;
    ShutdownRecoveryPosition narrow_outside = narrow;
    narrow_outside.low_half_in_guest_code = false;
    ShutdownRecoveryPosition narrow_unknown = narrow;
    narrow_unknown.host_address_known = false;
    const bool narrow_host =
        DecideShutdownRecovery(narrow) == ShutdownRecoveryDecision::kRecover &&
        DecideShutdownRecovery(narrow_outside) ==
            ShutdownRecoveryDecision::kOutsideGuestCode &&
        DecideShutdownRecovery(narrow_unknown) ==
            ShutdownRecoveryDecision::kRecover;

    bool names = true;
    for (const ShutdownRecoveryDecision decision :
         {ShutdownRecoveryDecision::kRecover,
          ShutdownRecoveryDecision::kOutsideGuestCode,
          ShutdownRecoveryDecision::kAliasedHostAddress,
          ShutdownRecoveryDecision::kHostAddressUnavailable})
    {
        names = names &&
            std::string_view(engine::ShutdownRecoveryDecisionName(decision)) !=
                "unknown";
    }

    const bool all = recovers_guest && refuses_alias && refuses_host &&
        boundary && refuses_unknown && narrow_host && names;
    std::cout << "shutdown_recovery_policy_wide_pointer="
              << (wide ? "true" : "false")
              << "\nshutdown_recovery_policy_recovers_guest="
              << (recovers_guest ? "true" : "false")
              << "\nshutdown_recovery_policy_refuses_alias="
              << (refuses_alias ? "true" : "false")
              << "\nshutdown_recovery_policy_refuses_host="
              << (refuses_host ? "true" : "false")
              << "\nshutdown_recovery_policy_boundary="
              << (boundary ? "true" : "false")
              << "\nshutdown_recovery_policy_refuses_unknown="
              << (refuses_unknown ? "true" : "false")
              << "\nshutdown_recovery_policy_narrow_host="
              << (narrow_host ? "true" : "false")
              << "\nshutdown_recovery_policy_names="
              << (names ? "true" : "false")
              << "\nshutdown_recovery_policy_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
