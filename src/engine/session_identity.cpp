#include "repiu/engine/session_identity.h"

namespace repiu::engine
{
namespace
{

std::string& TargetProfileStorage()
{
    static std::string value;
    return value;
}

}  // namespace

void SetSessionTargetProfile(std::string_view profile_id)
{
    TargetProfileStorage().assign(profile_id);
}

std::string SessionTargetProfile()
{
    const std::string& value = TargetProfileStorage();
    return value.empty() ? std::string("unknown") : value;
}

}  // namespace repiu::engine
