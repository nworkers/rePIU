#pragma once

namespace repiu::engine
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
void InstallHostCrashReporter();

}  // namespace repiu::engine
