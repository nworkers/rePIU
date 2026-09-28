#include "repiu/platform/host_telemetry.h"

// Task 759. The Linux half of the live telemetry's host reads: none of them
// is available.

namespace repiu::platform
{

void ReadHostImageRange(std::uint32_t* base, std::uint32_t* size)
{
    // Left as they were on Linux, which the sampling path reads as "no bounds
    // known" -- it only compares an address against them.
    (void)base;
    (void)size;
}

bool ReadHostThreadTimes(const HostThread& thread,
                         std::uint64_t* kernel_100ns,
                         std::uint64_t* user_100ns)
{
    // Task 503d-21: fenced on its own now. Linux reports the same split
    // in /proc/<pid>/task/<tid>/stat, but as jiffies against a
    // configurable tick rather than as 100ns units, so a counterpart
    // would be a different measurement under this one's name.
    (void)thread;
    (void)kernel_100ns;
    (void)user_100ns;
    return false;
}

// 3d-14: the section another process maps is cross-process diagnostics, not
// something the guest needs, so Linux starts without it rather than with an
// invented counterpart.
HostSharedMapping OpenHostSharedMappingFromEnvironment(
    const char* variable_name)
{
    (void)variable_name;
    return HostSharedMapping{};
}

void CloseHostSharedMapping(HostSharedMapping* mapping)
{
    if (mapping != nullptr)
    {
        mapping->mapping = nullptr;
        mapping->view = nullptr;
    }
}

// Linux cannot stop a thread and read its registers from inside the same
// process; the native-phase sampler interrupts it instead (Task 503d-20).
bool ReadSuspendedHostThreadContext(void* thread_handle,
                                    GuestCpuContext* context)
{
    (void)thread_handle;
    (void)context;
    return false;
}

}  // namespace repiu::platform
