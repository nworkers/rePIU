#include "fault_recovery_provenance_probe.h"

#include "repiu/engine/fault_recovery_provenance.h"

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
    std::cerr << "fault_recovery_provenance: " << name << " failed\n";
    return false;
}

}  // namespace

bool RunFaultRecoveryProvenanceProbe()
{
    using repiu::engine::FaultRecoveryPath;
    using repiu::engine::FaultRecoverySource;
    using repiu::engine::FaultRecoverySourceName;
    using repiu::engine::FaultRecoveryPathName;
    using repiu::engine::MakeFaultRecoveryProvenance;

    bool ok = true;
    const auto initial = MakeFaultRecoveryProvenance(
        0U, false, false, FaultRecoveryPath::kNone);
    ok = Check("initial_invalid", !initial.valid) && ok;
    ok = Check("initial_source", initial.source == FaultRecoverySource::kNone) &&
        ok;
    ok = Check("initial_path", initial.path == FaultRecoveryPath::kNone) && ok;

    const auto guest = MakeFaultRecoveryProvenance(
        0x01001234U, true, false, FaultRecoveryPath::kFaultCallback);
    ok = Check("guest_valid", guest.valid) && ok;
    ok = Check("guest_eip", guest.source_eip == 0x01001234U) && ok;
    ok = Check("guest_source", guest.source == FaultRecoverySource::kGuest) &&
        ok;
    ok = Check("guest_path",
               guest.path == FaultRecoveryPath::kFaultCallback) && ok;
    ok = Check("guest_source_name",
               std::string(FaultRecoverySourceName(guest.source)) == "guest") &&
        ok;
    ok = Check("guest_path_name",
               std::string(FaultRecoveryPathName(guest.path)) ==
                   "fault-callback") && ok;

    const auto cache = MakeFaultRecoveryProvenance(
        0x20001234U, false, true, FaultRecoveryPath::kShutdownInterrupt);
    ok = Check("cache_valid", cache.valid) && ok;
    ok = Check("cache_source", cache.source == FaultRecoverySource::kAotCache) &&
        ok;
    ok = Check("cache_path",
               cache.path == FaultRecoveryPath::kShutdownInterrupt) && ok;
    ok = Check("cache_source_name",
               std::string(FaultRecoverySourceName(cache.source)) ==
                   "aot-cache") && ok;
    ok = Check("cache_path_name",
               std::string(FaultRecoveryPathName(cache.path)) ==
                   "shutdown-interrupt") && ok;

    const auto outside = MakeFaultRecoveryProvenance(
        0x40001234U, false, false, FaultRecoveryPath::kShutdownInterrupt);
    ok = Check("outside_invalid", !outside.valid) && ok;
    const auto ambiguous = MakeFaultRecoveryProvenance(
        0x01001234U, true, true, FaultRecoveryPath::kFaultCallback);
    ok = Check("ambiguous_invalid", !ambiguous.valid) && ok;
    return ok;
}

}  // namespace repiu::tools
