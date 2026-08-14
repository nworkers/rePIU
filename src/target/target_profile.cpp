#include "repiu/target/target_profile.h"

#include <string>

namespace repiu::target
{
namespace
{

TargetProfile MakePiuTargetProfile(std::string_view id,
                                   std::string_view parent_rom_set_id,
                                   std::string_view display_name,
                                   bool enable_piu10)
{
    const std::filesystem::path asset_root =
        std::filesystem::path("build/runtime_mounts") / std::string(id);
    const std::filesystem::path working_directory = asset_root / "PIU";
    return TargetProfile{
        id,
        display_name,
        working_directory / "PIU.EXE",
        working_directory,
        asset_root,
        ExecutableFormatHint::kDos4gwLe,
        "piu_common",
        id,
        parent_rom_set_id,
        TargetRuntimeReservationHint{
            true,
            0x00010000,
            0x005D7000,
        },
        enable_piu10,
        enable_piu10,
        true,
    };
}

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
            "",
            TargetRuntimeReservationHint{
                true,
                0x00010000,
                0x00200000,
            },
        },
        MakePiuTargetProfile(
            "pumpit1",
            "pumpitup",
            "Pump It Up: The 1st Dance Floor (ver 0.53.1999.9.31)",
            false),
        MakePiuTargetProfile(
            "pumpit2",
            "pumpitup",
            "Pump It Up: The 2nd Dance Floor (Feb 28 2000)",
            false),
        MakePiuTargetProfile(
            "pumpit2a",
            "pumpit2",
            "Pump It Up: The 2nd Dance Floor (Dec 27 1999)",
            false),
        MakePiuTargetProfile(
            "pumpit3",
            "pumpitup",
            "Pump It Up The O.B.G: The 3rd Dance Floor "
            "(v3.04 - Jun 02 2000)",
            false),
        MakePiuTargetProfile(
            "pumpit3a",
            "pumpit3",
            "Pump It Up The O.B.G: The 3rd Dance Floor "
            "(v3.03 - May 07 2000)",
            false),
        MakePiuTargetProfile(
            "pumpito",
            "pumpitup",
            "Pump It Up The O.B.G: The Season Evolution Dance Floor "
            "(R4/v3.25 - Aug 27 2000)",
            true),
        MakePiuTargetProfile(
            "pumpitc",
            "pumpitup",
            "Pump It Up: The Collection (R5/v3.43 - Nov 14 2000)",
            true),
        MakePiuTargetProfile(
            "pumpitpc",
            "pumpitup",
            "Pump It Up: The Perfect Collection "
            "(R5/v3.52 - Dec 18 2000)",
            true),
        MakePiuTargetProfile(
            "pumpitpr",
            "pumpitup",
            "Pump It Up The Premiere: The International Dance Floor "
            "(R6/v4.01 - Feb 22 2001)",
            true),
        MakePiuTargetProfile(
            "pumpitpru",
            "pumpitpr",
            "Pump It Up The Premiere: The International Dance Floor "
            "(R6/v4.01 - Feb 22 2001 USA)",
            true),
        MakePiuTargetProfile(
            "pumpite",
            "pumpitup",
            "Pump It Up Extra (Mar 21 2001)",
            true),
        MakePiuTargetProfile(
            "pumpitea",
            "pumpite",
            "Pump It Up Extra (Mar 08 2001)",
            true),
        MakePiuTargetProfile(
            "pumpitpx",
            "pumpitup",
            "Pump It Up The PREX: The International Dance Floor "
            "(2001 - REV2 / 101)",
            true),
        MakePiuTargetProfile(
            "pumpit8",
            "pumpitup",
            "Pump It Up The Rebirth: The 8th Dance Floor "
            "(Rebirth/2002)",
            true),
        MakePiuTargetProfile(
            "pumpitp2",
            "pumpitup",
            "Pump It Up The Premiere 2: The International 2nd Dance Floor "
            "(Premiere 2/2002)",
            true),
        MakePiuTargetProfile(
            "pumpipx2",
            "pumpitup",
            "Pump It Up The PREX 2 (Premiere 2/2003)",
            true),
        MakePiuTargetProfile(
            "pumpipx2p",
            "pumpipx2",
            "Pump It Up EXTRA + Plus (Premiere 2/2003)",
            true),
        MakePiuTargetProfile(
            "pumpitp3",
            "pumpitup",
            "Pump It Up The Premiere 3: The International 3rd Dance Floor "
            "(Premiere 3/2003 - 28th Mar 2003)",
            true),
        MakePiuTargetProfile(
            "pumpitp3a",
            "pumpitp3",
            "Pump It Up The Premiere 3: The International 3rd Dance Floor "
            "(Premiere 3/2003 - 17th Mar 2003)",
            true),
        MakePiuTargetProfile(
            "pumpipx3",
            "pumpitup",
            "Pump It Up The PREX 3: The International 4th Dance Floor "
            "(2003 - X3.2MK3)",
            true),
        MakePiuTargetProfile(
            "pumpipx3a",
            "pumpipx3",
            "Pump It Up The PREX 3: The International 4th Dance Floor "
            "(2003 - INT X3.1MK3)",
            true),
        MakePiuTargetProfile(
            "pumpipx3b",
            "pumpipx3",
            "Pump It Up The PREX 3: The International 4th Dance Floor "
            "(2003 - Korea X3.1MK3)",
            true),
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
