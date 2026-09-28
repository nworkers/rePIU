#include "../host_gpu_driver_arch.h"

// Task 760. The i386 driver directories. `/usr/lib/dri` is left out: on a
// multiarch system it holds the x86-64 drivers.
#if !defined(__i386__)
#error "src/platform/linux/x86/ is the i386 Linux layer"
#endif

namespace repiu::platform::host_gpu_driver_arch
{

const char* const* WslD3d12DriverPaths(std::size_t* count)
{
    static const char* const kDrivers[] = {
        "/usr/lib/i386-linux-gnu/dri/d3d12_dri.so",
        "/usr/lib32/dri/d3d12_dri.so",
    };
    *count = sizeof(kDrivers) / sizeof(kDrivers[0]);
    return kDrivers;
}

}  // namespace repiu::platform::host_gpu_driver_arch
