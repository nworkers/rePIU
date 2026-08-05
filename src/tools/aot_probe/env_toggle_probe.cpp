#include "env_toggle_probe.h"

#include "repiu/runtime/env_toggle.h"

#include <iostream>

namespace repiu::tools
{

bool RunEnvToggleProbe()
{
    // Task 424 작업 지시 §1의 진리표 전체입니다. 두 함수가 다르게 답하는 행은
    // 미지정과 빈 값 두 개뿐이고, 나머지는 모두 같아야 합니다.
    const struct
    {
        const char* value;
        bool promoted;
        bool opt_in;
    } cases[] = {
        {nullptr, true, false},
        {"", true, false},
        {"1", true, true},
        {"on", true, true},
        {"true", true, true},
        {"0", false, false},
        {"off", false, false},
        {"false", false, false},
        // 알 수 없는 값은 두 계열 모두 fail-closed OFF입니다. 대소문자만 다른
        // 철자도 여기에 포함되므로, 관대해지는 회귀가 생기면 걸립니다.
        {"yes", false, false},
        {"no", false, false},
        {"2", false, false},
        {"ON", false, false},
        {"True", false, false},
        {"TRUE", false, false},
        {" 1", false, false},
        {"1 ", false, false},
        {"enable", false, false},
    };

    bool all = true;
    for (const auto& test : cases)
    {
        all = all &&
            runtime::ResolvePromotedToggle(test.value) == test.promoted &&
            runtime::ResolveOptInToggle(test.value) == test.opt_in;
    }

    std::cout << "env_toggle_policy=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
