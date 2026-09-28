#include "repiu/platform/host_gpu_driver.h"

// Task 759. Windows draws on its own GPU driver; there is no WSL driver to pick.

namespace repiu::platform
{

bool SelectWslD3d12Driver()
{
    return false;
}

}  // namespace repiu::platform
