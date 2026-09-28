#include "../host_gpu_driver_arch.h"

// Task 760. The x86-64 driver directories.
#if !defined(__x86_64__)
#error "src/platform/linux/x64/ is the x86-64 Linux layer"
#endif

namespace repiu::platform::host_gpu_driver_arch
{

const char* const* WslD3d12DriverPaths(std::size_t* count)
{
    static const char* const kDrivers[] = {
        "/usr/lib/x86_64-linux-gnu/dri/d3d12_dri.so",
        "/usr/lib64/dri/d3d12_dri.so",
        "/usr/lib/dri/d3d12_dri.so",
    };
    *count = sizeof(kDrivers) / sizeof(kDrivers[0]);
    return kDrivers;
}

}  // namespace repiu::platform::host_gpu_driver_arch
