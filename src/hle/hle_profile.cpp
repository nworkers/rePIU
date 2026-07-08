#include "repiu/hle/hle_profile.h"

namespace repiu::hle
{
namespace
{

const std::vector<HleProfile>& BuiltInHleProfiles()
{
    static const std::vector<HleProfile> profiles = {
        HleProfile{
            "dos4gw_console_sample",
            "DOS/4GW console sample HLE",
            "Minimal HLE service scope for DOS/4GW console samples",
            std::vector<HleService>{
                HleService::kDosConsole,
                HleService::kDpmi,
            },
        },
        HleProfile{
            "piu_common",
            "PIU common HLE",
            "Shared HLE service scope for PIU DOS/4GW targets",
            std::vector<HleService>{
                HleService::kDosFile,
                HleService::kDosMemory,
                HleService::kDpmi,
                HleService::kTimer,
                HleService::kInput,
                HleService::kVideo,
                HleService::kAudio,
            },
        },
    };
    return profiles;
}

}  // namespace

const std::vector<HleProfile>& GetBuiltInHleProfiles()
{
    return BuiltInHleProfiles();
}

const HleProfile* FindHleProfileById(std::string_view id)
{
    for (const HleProfile& profile : BuiltInHleProfiles())
    {
        if (profile.id == id)
        {
            return &profile;
        }
    }

    return nullptr;
}

std::string_view HleServiceName(HleService service)
{
    switch (service)
    {
        case HleService::kDosFile:
            return "DOS file";
        case HleService::kDosMemory:
            return "DOS memory";
        case HleService::kDpmi:
            return "DPMI";
        case HleService::kDosConsole:
            return "DOS console";
        case HleService::kTimer:
            return "timer";
        case HleService::kInput:
            return "input";
        case HleService::kVideo:
            return "video";
        case HleService::kAudio:
            return "audio";
    }

    return "unknown";
}

}  // namespace repiu::hle
