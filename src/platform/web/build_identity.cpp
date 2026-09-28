#include "repiu/platform/build_identity.h"

// Task 758. The web build identity.

namespace repiu::platform
{

std::string_view BuildPlatformName()
{
    return "Web";
}

std::string_view BuildArchitectureName()
{
    return "wasm32";
}

}  // namespace repiu::platform
