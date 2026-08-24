#ifndef REPIU_PLATFORM_HOST_PROCESS_H_
#define REPIU_PLATFORM_HOST_PROCESS_H_

#include <cstddef>
#include <cstdint>

// Task 503d-17. Starting this executable again as a child, and waiting for it.
//
// The launcher does this for a reason particular to Task 500: drawing anything
// loads a GPU driver, and that driver claims low address space the guest needs,
// so the rom set the user picked is carried into a fresh process whose address
// space is still intact. Whether Linux has the same constraint is **not
// measured** -- Task 502 deferred the question until an execution engine
// existed, and it still stands. The behaviour is carried across so both hosts
// do the same thing; if the measurement later says Linux does not need it, this
// is where that is undone.

namespace repiu::platform
{

// The same launch described twice, because the two hosts take it differently.
//
// `command_line` is the Windows command line, quoted by the caller;
// `arguments` is the POSIX argv tail, which the backend prefixes with the
// executable path. Each host uses the one its API accepts. Deriving one from
// the other is what this deliberately avoids -- splitting a command line back
// into arguments means implementing the quoting rules a second time, and the
// second implementation is the one nothing tests.
struct ChildProcessLaunch
{
    const char* executable_path = nullptr;
    const char* command_line = nullptr;
    const char* const* arguments = nullptr;
    std::size_t argument_count = 0;
};

// What could not be an exit code, so a caller can tell "the child failed" from
// "the child never started".
inline constexpr int kChildProcessDidNotStart = -1;

// Runs the child to completion and returns its exit code, or
// `kChildProcessDidNotStart`. `host_error` receives the host's own error number
// when the launch fails -- for the log line, not for control flow, which is
// 3d-5's rule applied to a second kind of failure.
int RunChildProcessAndWait(const ChildProcessLaunch& launch,
                           std::uint32_t* host_error);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_PROCESS_H_
