#ifndef REPIU_ENGINE_FAULT_RECOVERY_PROVENANCE_H_
#define REPIU_ENGINE_FAULT_RECOVERY_PROVENANCE_H_

#include <cstdint>

namespace repiu::engine
{

enum class FaultRecoverySource : std::uint8_t
{
    kNone,
    kGuest,
    kAotCache,
};

enum class FaultRecoveryPath : std::uint8_t
{
    kNone,
    kFaultCallback,
    kShutdownInterrupt,
};

struct FaultRecoveryProvenance
{
    bool valid = false;
    std::uint32_t source_eip = 0;
    FaultRecoverySource source = FaultRecoverySource::kNone;
    FaultRecoveryPath path = FaultRecoveryPath::kNone;
};

// Builds a recovery record only when exactly one executable guest region owns
// the source. Ambiguous or out-of-range sources remain invalid.
[[nodiscard]] FaultRecoveryProvenance MakeFaultRecoveryProvenance(
    std::uint32_t source_eip,
    bool in_guest,
    bool in_aot_cache,
    FaultRecoveryPath path);

[[nodiscard]] const char* FaultRecoverySourceName(FaultRecoverySource source);
[[nodiscard]] const char* FaultRecoveryPathName(FaultRecoveryPath path);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_FAULT_RECOVERY_PROVENANCE_H_
