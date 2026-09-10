#include "repiu/engine/fault_recovery_provenance.h"

namespace repiu::engine
{

FaultRecoveryProvenance MakeFaultRecoveryProvenance(
    const std::uint32_t source_eip,
    const bool in_guest,
    const bool in_aot_cache,
    const FaultRecoveryPath path)
{
    FaultRecoveryProvenance result;
    if (source_eip == 0U || in_guest == in_aot_cache ||
        path == FaultRecoveryPath::kNone)
    {
        return result;
    }

    result.valid = true;
    result.source_eip = source_eip;
    result.source = in_guest ? FaultRecoverySource::kGuest
                             : FaultRecoverySource::kAotCache;
    result.path = path;
    return result;
}

const char* FaultRecoverySourceName(const FaultRecoverySource source)
{
    switch (source)
    {
        case FaultRecoverySource::kGuest:
            return "guest";
        case FaultRecoverySource::kAotCache:
            return "aot-cache";
        case FaultRecoverySource::kNone:
        default:
            return "none";
    }
}

const char* FaultRecoveryPathName(const FaultRecoveryPath path)
{
    switch (path)
    {
        case FaultRecoveryPath::kFaultCallback:
            return "fault-callback";
        case FaultRecoveryPath::kShutdownInterrupt:
            return "shutdown-interrupt";
        case FaultRecoveryPath::kNone:
        default:
            return "none";
    }
}

}  // namespace repiu::engine
