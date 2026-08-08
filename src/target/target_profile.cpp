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
            "",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x00200000,
            },
        },
        TargetProfile{
            "piu_1st",
            "PIU 1st",
            std::filesystem::path("MASTER/PIU_1ST/PIU/PIU.EXE"),
            std::filesystem::path("MASTER/PIU_1ST/PIU"),
            std::filesystem::path("MASTER/PIU_1ST"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
        },
        TargetProfile{
            "pumpit1",
            "Pump It Up 1st (MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpit1/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpit1/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpit1"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpit1",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
        },
        TargetProfile{
            "pumpit2",
            "Pump It Up 2nd Dance Floor (MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpit2/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpit2/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpit2"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpit2",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
        },
        TargetProfile{
            "pumpit3",
            "Pump It Up The O.B.G: The 3rd Dance Floor (MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpit3/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpit3/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpit3"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpit3",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
        },
        TargetProfile{
            "pumpito",
            "Pump It Up The O.B.G.: The Season Evolution Dance Floor "
            "(MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpito/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpito/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpito"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpito",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
            true,
        },
        TargetProfile{
            "pumpitc",
            "Pump It Up: The Collection (MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpitc/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpitc/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpitc"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpitc",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
            true,
        },
        TargetProfile{
            "pumpitpc",
            "Pump It Up: The Perfect Collection (MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpitpc/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpitpc/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpitpc"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpitpc",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
            true,
        },
        TargetProfile{
            "pumpite",
            "Pump It Up Extra (MAME CHD)",
            std::filesystem::path(
                "build/runtime_mounts/pumpite/PIU/PIU.EXE"),
            std::filesystem::path("build/runtime_mounts/pumpite/PIU"),
            std::filesystem::path("build/runtime_mounts/pumpite"),
            ExecutableFormatHint::kDos4gwLe,
            "piu_common",
            "pumpite",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x005D7000,
            },
            true,
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
