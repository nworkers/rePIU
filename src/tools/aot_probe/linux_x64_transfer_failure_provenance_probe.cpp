#include "linux_x64_transfer_failure_provenance_probe.h"

#include "repiu/engine/linux_x64_transfer_failure_provenance.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace repiu::tools
{
namespace
{

bool Check(const char* name, const bool condition)
{
    if (condition)
    {
        return true;
    }
    std::cerr << "linux_x64_transfer_failure_provenance: " << name
              << " failed\n";
    return false;
}

}  // namespace

bool RunLinuxX64TransferFailureProvenanceProbe()
{
    using repiu::engine::LinuxX64TransferFailureReason;
    using repiu::engine::LinuxX64TransferFailureReasonName;
    using repiu::engine::LinuxX64TransferKind;
    using repiu::engine::LinuxX64TransferKindName;
    using repiu::engine::MakeLinuxX64TransferFailureProvenance;

    bool ok = true;
    const auto initial = MakeLinuxX64TransferFailureProvenance(
        0U, 0U, 0U, LinuxX64TransferFailureReason::kNone);
    ok = Check("initial_invalid", !initial.valid) && ok;
    ok = Check("initial_kind", initial.kind == LinuxX64TransferKind::kNone) &&
        ok;
    ok = Check("initial_reason",
               initial.reason == LinuxX64TransferFailureReason::kNone) && ok;

    const auto ret = MakeLinuxX64TransferFailureProvenance(
        0x010F1E56U, 0U, 0x0158CC5CU,
        LinuxX64TransferFailureReason::kTranslationFailed);
    ok = Check("ret_valid", ret.valid) && ok;
    ok = Check("ret_producer", ret.producer_eip == 0x010F1E56U) && ok;
    ok = Check("ret_zero_target", ret.target_eip == 0U) && ok;
    ok = Check("ret_guest_esp", ret.guest_esp == 0x0158CC5CU) && ok;
    ok = Check("ret_kind", ret.kind == LinuxX64TransferKind::kReturn) && ok;
    ok = Check("ret_reason",
               ret.reason ==
                   LinuxX64TransferFailureReason::kTranslationFailed) && ok;
    ok = Check("ret_kind_name",
               std::string(LinuxX64TransferKindName(ret.kind)) == "ret") &&
        ok;
    ok = Check("ret_reason_name",
               std::string(LinuxX64TransferFailureReasonName(ret.reason)) ==
                   "translation-failed") && ok;

    const auto indirect = MakeLinuxX64TransferFailureProvenance(
        0x80000000U | 0x010F4ACFU, 0x010F4AD1U, 0x0158CC74U,
        LinuxX64TransferFailureReason::kPolicyRefused);
    ok = Check("indirect_valid", indirect.valid) && ok;
    ok = Check("indirect_producer", indirect.producer_eip == 0x010F4ACFU) &&
        ok;
    ok = Check("indirect_target", indirect.target_eip == 0x010F4AD1U) && ok;
    ok = Check("indirect_kind",
               indirect.kind == LinuxX64TransferKind::kIndirectCall) && ok;
    ok = Check("indirect_reason",
               indirect.reason == LinuxX64TransferFailureReason::kPolicyRefused) &&
        ok;
    ok = Check("indirect_kind_name",
               std::string(LinuxX64TransferKindName(indirect.kind)) ==
                   "indirect-call") && ok;
    ok = Check("indirect_reason_name",
               std::string(LinuxX64TransferFailureReasonName(indirect.reason)) ==
                   "policy-refused") && ok;

    const auto missing_producer = MakeLinuxX64TransferFailureProvenance(
        0x80000000U, 0U, 0x0158CC5CU,
        LinuxX64TransferFailureReason::kTranslationFailed);
    ok = Check("missing_producer_invalid", !missing_producer.valid) && ok;
    return ok;
}

}  // namespace repiu::tools
