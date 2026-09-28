#include "repiu/platform/build_identity.h"

// Task 758. The Linux x64 build's architecture name.
#if !defined(__x86_64__)
#error "src/platform/linux/x64/ is the x86-64 Linux platform layer"
#endif

namespace repiu::platform
{

std::string_view BuildArchitectureName()
{
    return "x64";
}

}  // namespace repiu::platform
