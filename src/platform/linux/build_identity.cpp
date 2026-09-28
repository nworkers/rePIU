#include "repiu/platform/build_identity.h"

// Task 758. The Linux build's platform name. The architecture name is in x86/
// and x64/.

namespace repiu::platform
{

std::string_view BuildPlatformName()
{
    return "Linux";
}

}  // namespace repiu::platform
