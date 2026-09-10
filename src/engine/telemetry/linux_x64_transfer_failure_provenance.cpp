#include "repiu/engine/linux_x64_transfer_failure_provenance.h"

namespace repiu::engine
{

namespace
{

constexpr std::uint32_t kIndirectCallProducerMask = 0x80000000U;
constexpr std::uint32_t kProducerSiteMask = 0x7FFFFFFFU;

bool IsKnownFailureReason(const LinuxX64TransferFailureReason reason)
{
    return reason == LinuxX64TransferFailureReason::kTranslationFailed ||
           reason == LinuxX64TransferFailureReason::kPolicyRefused;
}

}  // namespace

LinuxX64TransferFailureProvenance
MakeLinuxX64TransferFailureProvenance(
    const std::uint32_t producer_tag,
    const std::uint32_t target_eip,
    const std::uint32_t guest_esp,
    const LinuxX64TransferFailureReason reason)
{
    LinuxX64TransferFailureProvenance result;
    const std::uint32_t producer_eip = producer_tag & kProducerSiteMask;
    if (producer_eip == 0U || !IsKnownFailureReason(reason))
    {
        return result;
    }

    result.valid = true;
    result.producer_eip = producer_eip;
    result.target_eip = target_eip;
    result.guest_esp = guest_esp;
    result.kind = (producer_tag & kIndirectCallProducerMask) != 0U
        ? LinuxX64TransferKind::kIndirectCall
        : LinuxX64TransferKind::kReturn;
    result.reason = reason;
    return result;
}

const char* LinuxX64TransferKindName(const LinuxX64TransferKind kind)
{
    switch (kind)
    {
        case LinuxX64TransferKind::kReturn:
            return "ret";
        case LinuxX64TransferKind::kIndirectCall:
            return "indirect-call";
        case LinuxX64TransferKind::kNone:
        default:
            return "none";
    }
}

const char* LinuxX64TransferFailureReasonName(
    const LinuxX64TransferFailureReason reason)
{
    switch (reason)
    {
        case LinuxX64TransferFailureReason::kTranslationFailed:
            return "translation-failed";
        case LinuxX64TransferFailureReason::kPolicyRefused:
            return "policy-refused";
        case LinuxX64TransferFailureReason::kNone:
        default:
            return "none";
    }
}

}  // namespace repiu::engine
