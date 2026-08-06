#include "env_toggle_probe.h"

#include "repiu/runtime/env_toggle.h"

#include <iostream>

namespace repiu::tools
{

bool RunEnvToggleProbe()
{
    // The complete truth table from §1 of the Task 424 work order. The only two
    // rows where the functions answer differently are unset and empty; every
    // other row must agree.
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
        // An unknown value is a fail-closed OFF in both conventions. Spellings
        // that differ only in case belong here too, so a regression toward
        // permissiveness is caught.
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
