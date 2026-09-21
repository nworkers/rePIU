#include "repiu/engine/shutdown_recovery_policy.h"

namespace repiu::engine
{

bool HostAddressFitsGuestSpace(const std::uintptr_t address)
{
    return static_cast<std::uint64_t>(address) <= 0xFFFFFFFFULL;
}

ShutdownRecoveryDecision DecideShutdownRecovery(
    const ShutdownRecoveryPosition& position)
{
    // Zero is what `ReadHostInstructionPointer` returns when it has nothing,
    // and no thread is ever executing there.
    if (position.host_address_required &&
        (!position.host_address_known || position.host_address == 0U))
    {
        return ShutdownRecoveryDecision::kHostAddressUnavailable;
    }
    if (!position.low_half_in_guest_code)
    {
        return ShutdownRecoveryDecision::kOutsideGuestCode;
    }
    if (position.host_address_required &&
        !HostAddressFitsGuestSpace(position.host_address))
    {
        return ShutdownRecoveryDecision::kAliasedHostAddress;
    }
    return ShutdownRecoveryDecision::kRecover;
}

const char* ShutdownRecoveryDecisionName(const ShutdownRecoveryDecision decision)
{
    switch (decision)
    {
        case ShutdownRecoveryDecision::kRecover:
            return "recover";
        case ShutdownRecoveryDecision::kOutsideGuestCode:
            return "outside-guest-code";
        case ShutdownRecoveryDecision::kAliasedHostAddress:
            return "aliased-host-address";
        case ShutdownRecoveryDecision::kHostAddressUnavailable:
            return "host-address-unavailable";
    }
    return "unknown";
}

}  // namespace repiu::engine
