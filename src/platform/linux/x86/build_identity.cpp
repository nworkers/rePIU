#include "repiu/platform/build_identity.h"

// Task 758. The Linux x86 build's architecture name.
#if !defined(__i386__)
#error "src/platform/linux/x86/ is the i386 Linux platform layer"
#endif

namespace repiu::platform
{

std::string_view BuildArchitectureName()
{
    return "x86";
}

}  // namespace repiu::platform
