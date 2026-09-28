#include "repiu/platform/build_identity.h"

// Task 758. The Win32 build identity. The Win32 host is built as x86 only.
#if !defined(_M_IX86)
#error "the Win32 host is built as x86 only"
#endif

namespace repiu::platform
{

std::string_view BuildPlatformName()
{
    return "Win";
}

std::string_view BuildArchitectureName()
{
    return "x86";
}

}  // namespace repiu::platform
