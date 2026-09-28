#ifndef REPIU_PLATFORM_HOST_TELEMETRY_H_
#define REPIU_PLATFORM_HOST_TELEMETRY_H_

#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/host_thread.h"

#include <cstdint>

// Task 759. What the engine's live telemetry reads from the host. Win32
// answers all of it; Linux has no counterpart for any (Tasks 503d-14 and
// 503d-21) and says so. None of it is needed to run the guest.

namespace repiu::platform
{

// Task 412. The loader's own image range. Both stay as they were where it is
// not known, which the sampling path reads as "no bounds".
void ReadHostImageRange(std::uint32_t* base, std::uint32_t* size);

// A thread's kernel and user time in 100 ns units. False where the host does
// not report them in that unit.
bool ReadHostThreadTimes(const HostThread& thread,
                         std::uint64_t* kernel_100ns,
                         std::uint64_t* user_100ns);

// A section another process created and this one maps whole.
struct HostSharedMapping
{
    void* mapping = nullptr;
    void* view = nullptr;
};

// Opens the section named by the environment variable `variable_name`. Both
// members stay null when the variable is absent, the section cannot be opened
// or it cannot be mapped.
HostSharedMapping OpenHostSharedMappingFromEnvironment(
    const char* variable_name);

// Unmaps and closes, and clears both members.
void CloseHostSharedMapping(HostSharedMapping* mapping);

// Stops the thread behind `thread_handle`, reads its registers and lets it
// run again. False where the thread could not be stopped or read.
bool ReadSuspendedHostThreadContext(void* thread_handle,
                                    GuestCpuContext* context);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_TELEMETRY_H_
