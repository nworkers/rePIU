#ifndef REPIU_PLATFORM_HOST_CRASH_REPORT_H_
#define REPIU_PLATFORM_HOST_CRASH_REPORT_H_

namespace repiu::platform
{

// Task 441: prints the faulting address and a symbolised host stack when an
// exception reaches the top of the process without being handled.
//
// It exists because a host-side crash was previously invisible: the process
// vanished with an exit code and no output, and Task 440 spent five build-and-run
// rounds guessing at a teardown fault that a stack would have named immediately.
// The Windows debugging tools are not installed on this machine, and the SDK
// ships only the dbghelp DLLs, so the loader reports its own crash.
//
// Installed once from `main`. It fires only on an exception that would have
// terminated the process anyway, so it can never mask a fault -- it prints and
// then lets the process die.
//
// Task 759: in the platform layer, because all of it is the host's own
// reporting. Linux installs nothing: an unhandled fault there is reported by
// the platform fault handler (Task 578).
void InstallHostCrashReporter();

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_CRASH_REPORT_H_
