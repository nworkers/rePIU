#pragma once

#include <cstddef>

// Task 760. Where Mesa's D3D12 driver for this process's architecture may be.
// A 32-bit process loads 32-bit drivers, so the list differs by architecture:
// looking for the x86-64 driver from an i386 process chose a driver that
// process cannot load, and its window failed to open. Defined in x86/ and
// x64/.

namespace repiu::platform::host_gpu_driver_arch
{

const char* const* WslD3d12DriverPaths(std::size_t* count);

}  // namespace repiu::platform::host_gpu_driver_arch
