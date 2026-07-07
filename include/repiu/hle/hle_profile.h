#ifndef REPIU_HLE_HLE_PROFILE_H_
#define REPIU_HLE_HLE_PROFILE_H_

#include <string_view>
#include <vector>

namespace repiu::hle
{

enum class HleService
{
    kDosFile,
    kDosMemory,
    kDpmi,
    kTimer,
    kInput,
    kVideo,
    kAudio,
};

struct HleProfile
{
    std::string_view id;
    std::string_view display_name;
    std::string_view description;
    std::vector<HleService> services;
};

const std::vector<HleProfile>& GetBuiltInHleProfiles();

const HleProfile* FindHleProfileById(std::string_view id);

std::string_view HleServiceName(HleService service);

}  // namespace repiu::hle

#endif  // REPIU_HLE_HLE_PROFILE_H_
