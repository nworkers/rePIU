#include "repiu/target/target_profile.h"

namespace repiu::target
{
namespace
{

const std::vector<TargetProfile>& BuiltInTargetProfiles()
{
    static const std::vector<TargetProfile> profiles = {
        TargetProfile{
            "dos4gw_hello",
            "DOS/4GW Hello",
            std::filesystem::path(
                "samples/dos4gw_hello/build/hello.exe"),
            std::filesystem::path("samples/dos4gw_hello/build"),
            std::filesystem::path("samples/dos4gw_hello"),
            ExecutableFormatHint::kDos4gwLe,
            "dos4gw_console_sample",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x00200000,
            },
        },
        TargetProfile{
            "piu_1st",
            "PIU 1st",
            std::filesystem::path("MASTER/PIU_1ST/PIU.EXE"),
            std::filesystem::path("MASTER/PIU_1ST"),
            std::filesystem::path("MASTER/PIU_1ST"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
        },
    };
    return profiles;
}

}  // namespace

const std::vector<TargetProfile>& GetBuiltInTargetProfiles()
{
    return BuiltInTargetProfiles();
}

const TargetProfile* FindTargetProfileById(std::string_view id)
{
    for (const TargetProfile& profile : BuiltInTargetProfiles())
    {
        if (profile.id == id)
        {
            return &profile;
        }
    }

    return nullptr;
}

std::string_view ExecutableFormatHintName(ExecutableFormatHint format_hint)
{
    switch (format_hint)
    {
        case ExecutableFormatHint::kDos4gwLe:
            return "DOS4GW_LE";
    }

    return "unknown";
}

}  // namespace repiu::target
