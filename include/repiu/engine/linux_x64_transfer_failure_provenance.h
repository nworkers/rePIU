#ifndef REPIU_ENGINE_LINUX_X64_TRANSFER_FAILURE_PROVENANCE_H_
#define REPIU_ENGINE_LINUX_X64_TRANSFER_FAILURE_PROVENANCE_H_

#include <cstdint>

namespace repiu::engine
{

enum class LinuxX64TransferKind : std::uint8_t
{
    kNone,
    kReturn,
    kIndirectCall,
};

enum class LinuxX64TransferFailureReason : std::uint8_t
{
    kNone,
    kTranslationFailed,
    kPolicyRefused,
};

struct LinuxX64TransferFailureProvenance
{
    bool valid = false;
    std::uint32_t producer_eip = 0;
    std::uint32_t target_eip = 0;
    std::uint32_t guest_esp = 0;
    LinuxX64TransferKind kind = LinuxX64TransferKind::kNone;
    LinuxX64TransferFailureReason reason =
        LinuxX64TransferFailureReason::kNone;
};

// Builds a failure record from the x64 thunk's producer tag. A zero target is
// intentionally retained because it is the observation this record explains.
[[nodiscard]] LinuxX64TransferFailureProvenance
MakeLinuxX64TransferFailureProvenance(
    std::uint32_t producer_tag,
    std::uint32_t target_eip,
    std::uint32_t guest_esp,
    LinuxX64TransferFailureReason reason);

[[nodiscard]] const char* LinuxX64TransferKindName(
    LinuxX64TransferKind kind);
[[nodiscard]] const char* LinuxX64TransferFailureReasonName(
    LinuxX64TransferFailureReason reason);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_LINUX_X64_TRANSFER_FAILURE_PROVENANCE_H_
