#include "repiu/runtime/env_toggle.h"

#include <string_view>

namespace repiu::runtime
{
namespace
{

// The only true values are these three lowercase spellings, and case is not
// folded. The six call sites that existed before Task 424 accepted exactly this
// set, so becoming more permissive here would silently change what the existing
// run procedures mean.
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
    // An unknown value is OFF. A typo passing silently as ON would make an A/B
    // result be read wrongly, so this stays fail-closed.
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
