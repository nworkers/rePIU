#pragma once

#include "repiu/platform/host_thread.h"

#include <cstdint>

// Task 758. What host_thread.cpp needs from one platform. The public functions in
// host_thread.cpp check their arguments and call these; win32/ and linux/ each
// define them (the web build uses the linux/ one). Internal to src/platform/.

namespace repiu::platform::host_thread_platform
{

// Starts the thread. `thread` has been reset and `entry` checked.
bool StartThread(HostThreadEntry entry,
                 void* parameter,
                 HostThread* thread,
                 std::uint32_t* host_error);

// `thread` is valid.
HostThreadStatus QueryThread(const HostThread& thread);
bool JoinThread(const HostThread& thread,
                std::uint32_t timeout_milliseconds,
                std::uint32_t* exit_code);

// `thread` is valid and one of the callbacks is set. Returns kNone on success.
ThreadInterruptFailure InterruptThread(const HostThread& thread,
                                       ThreadInterruptCallback callback,
                                       ThreadInterruptContextCallback context_callback,
                                       void* user_data,
                                       std::uint32_t timeout_milliseconds);

// `thread` is valid; the caller resets it afterwards.
void CloseThread(HostThread* thread);
void DetachThread(HostThread* thread);

}  // namespace repiu::platform::host_thread_platform
