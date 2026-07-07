#include "repiu/exe/dos4gw_loader.h"

namespace repiu::exe
{

bool LoadDos4gwExecutable(const std::vector<std::uint8_t>& data,
                          const target::TargetProfile& target_profile,
                          Dos4gwLoadResult* result,
                          ParseError* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            error->file_offset = 0;
            error->message = "DOS/4GW load result is null";
        }
        return false;
    }

    *result = Dos4gwLoadResult{};

    if (target_profile.format_hint !=
        target::ExecutableFormatHint::kDos4gwLe)
    {
        if (error != nullptr)
        {
            error->file_offset = 0;
            error->message = "target is not a DOS/4GW LE executable";
        }
        return false;
    }

    if (!ParseMzHeader(data, &result->mz_header, error))
    {
        return false;
    }

    if (!ParseLeHeader(data, result->mz_header.le_offset, &result->le_header,
                       error))
    {
        return false;
    }

    if (!BuildLeImage(data, result->le_header, &result->image, error))
    {
        return false;
    }

    if (!AnalyzeLeFixups(data, result->le_header, &result->fixup_info, error))
    {
        return false;
    }

    if (!DecodeLeFixupRecords(data, result->le_header, result->fixup_info,
                              &result->fixup_record_info, error))
    {
        return false;
    }

    if (!ApplyLeInternalRelocations(result->le_header,
                                    result->fixup_record_info,
                                    &result->image,
                                    &result->relocation_dry_run,
                                    error))
    {
        return false;
    }

    return true;
}

}  // namespace repiu::exe
