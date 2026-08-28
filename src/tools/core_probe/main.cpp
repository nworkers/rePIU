// Task 501. The probes that depend on nothing platform-specific, gathered into
// one binary that builds on every host.
//
// repiu_aot_probe cannot serve this purpose: most of its 56 probes reach into
// the Win32 execution layer, so making it build elsewhere would mean excluding
// most of it and leaving one binary that verifies different things depending on
// the operating system. This target instead holds only what is genuinely
// shared, so the same assertions can be compared directly across platforms.
// Windows loses no coverage, because repiu_aot_probe still runs these same
// probes there.
//
// Membership was decided by compiling and linking, not by reading includes: a
// probe can name a header like `dos/dos_int21_services.h` that resolves into
// the Win32 layer only through an include directory, which no amount of
// grepping for "win32" reveals.

// Task 513: six of the fifteen are compiled out for wasm32, and the binary says
// so on every run rather than reporting a smaller total in silence.
//
// The reason differs by probe and both reasons are real. Two of them --
// stack_bridge and guest_stack_switch -- are inline x86 assembly and do not
// reach the compiler at all. The other four exercise the platform facilities
// wasm does not have, so on this host they would only ever measure that the
// Stage 1 stubs return false, which is a fact the stubs already state.
//
// Excluding them is not the same as their passing. `core_probe_skipped` below
// keeps that distinction on screen, because "9 of 9 passed" printed alone would
// read as a complete run.
#include "dos_file_handle_cache_probe.h"
#include "env_toggle_probe.h"
#include "execution_backend_probe.h"
#include "execution_timeout_probe.h"
#include "glide_lfb_region_probe.h"
#include "jump_table_guard_probe.h"
#include "launcher_probe.h"
#include "nvram_path_probe.h"
#include "pit_timer_probe.h"

#if !defined(__EMSCRIPTEN__)
#include "fault_handler_probe.h"
#include "guest_cpu_context_probe.h"
#include "guest_stack_switch_probe.h"
#include "host_thread_probe.h"
#include "stack_bridge_probe.h"
#include "virtual_memory_probe.h"
#endif

#include <cstddef>
#include <iostream>

namespace
{

struct CoreProbe
{
    const char* name;
    bool (*run)();
};

constexpr CoreProbe kCoreProbes[] = {
    {"env_toggle", &repiu::tools::RunEnvToggleProbe},
    {"execution_backend", &repiu::tools::RunExecutionBackendProbe},
    {"execution_timeout", &repiu::tools::RunExecutionTimeoutProbe},
    {"dos_file_handle_cache", &repiu::tools::RunDosFileHandleCacheProbe},
    {"pit_timer", &repiu::tools::RunPitTimerProbe},
    {"glide_lfb_region", &repiu::tools::RunGlideLfbRegionProbe},
    {"jump_table_guard", &repiu::tools::RunJumpTableGuardProbe},
    {"nvram_path", &repiu::tools::RunNvramPathProbe},
#if !defined(__EMSCRIPTEN__)
    {"guest_cpu_context", &repiu::tools::RunGuestCpuContextProbe},
    {"virtual_memory", &repiu::tools::RunVirtualMemoryProbe},
    {"fault_handler", &repiu::tools::RunFaultHandlerProbe},
    {"stack_bridge", &repiu::tools::RunStackBridgeProbe},
    {"guest_stack_switch", &repiu::tools::RunGuestStackSwitchProbe},
    {"host_thread", &repiu::tools::RunHostThreadProbe},
#endif
    {"launcher", &repiu::tools::RunLauncherProbe},
};

// Task 513. Named, not counted: a list of what this host cannot ask is worth
// more than a number, and the next reader wants to know which six.
#if defined(__EMSCRIPTEN__)
constexpr const char* kSkippedProbes[] = {
    "guest_cpu_context", "virtual_memory",     "fault_handler",
    "stack_bridge",      "guest_stack_switch", "host_thread",
};
#endif

}  // namespace

int main()
{
    std::size_t failures = 0;
    for (const CoreProbe& probe : kCoreProbes)
    {
        std::cout << "== " << probe.name << " ==\n";
        // Every probe is run even after one fails, because a platform bring-up
        // wants the whole picture rather than the first stumble.
        if (!probe.run())
        {
            ++failures;
            std::cout << "!! " << probe.name << " failed\n";
        }
    }
    const std::size_t total =
        sizeof(kCoreProbes) / sizeof(kCoreProbes[0]);
    std::cout << "core_probe_total=" << total
              << "\ncore_probe_failures=" << failures
              << "\ncore_probe_all=" << (failures == 0 ? "true" : "false")
              << "\n";
#if defined(__EMSCRIPTEN__)
    std::cout << "core_probe_skipped="
              << sizeof(kSkippedProbes) / sizeof(kSkippedProbes[0]);
    for (const char* name : kSkippedProbes)
    {
        std::cout << ' ' << name;
    }
    std::cout << "\ncore_probe_host=wasm32 (Task 513 Stage 1: the execution "
                 "engine is not built here)\n";
#endif
    return failures == 0 ? 0 : 1;
}
