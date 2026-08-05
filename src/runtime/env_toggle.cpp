#include "repiu/runtime/env_toggle.h"

#include <string_view>

namespace repiu::runtime
{
namespace
{

// 참 값은 소문자 세 가지뿐이며 대소문자 변환을 하지 않습니다. Task 424 이전의
// 여섯 호출 지점이 정확히 이 집합만 받아들이고 있었으므로, 여기서 관대해지면
// 기존 실행 절차의 의미가 조용히 바뀝니다.
bool IsAffirmative(std::string_view value)
{
    return value == "1" || value == "on" || value == "true";
}

}  // namespace

bool ResolvePromotedToggle(const char* value)
{
    if (value == nullptr || *value == '\0')
    {
        return true;
    }
    // 알 수 없는 값은 OFF입니다. 오타가 조용히 ON으로 통과하면 A/B 결과를
    // 잘못 읽게 되므로 fail-closed로 둡니다.
    return IsAffirmative(value);
}

bool ResolveOptInToggle(const char* value)
{
    if (value == nullptr)
    {
        return false;
    }
    return IsAffirmative(value);
}

}  // namespace repiu::runtime
