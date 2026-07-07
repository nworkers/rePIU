#ifndef REPIU_EXE_DOS4GW_LOADER_H_
#define REPIU_EXE_DOS4GW_LOADER_H_

#include "repiu/exe/executable_headers.h"
#include "repiu/target/target_profile.h"

#include <cstdint>
#include <vector>

namespace repiu::exe
{

struct Dos4gwLoadResult
{
    MzHeader mz_header;
    LeHeader le_header;
    LeImage image;
    LeFixupInfo fixup_info;
    LeFixupRecordInfo fixup_record_info;
    LeRelocationDryRun relocation_dry_run;
};

bool LoadDos4gwExecutable(const std::vector<std::uint8_t>& data,
                          const target::TargetProfile& target_profile,
                          Dos4gwLoadResult* result,
                          ParseError* error);

}  // namespace repiu::exe

#endif  // REPIU_EXE_DOS4GW_LOADER_H_
