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

#include "dos_file_handle_cache_probe.h"
#include "env_toggle_probe.h"
#include "execution_backend_probe.h"
#include "execution_timeout_probe.h"
#include "glide_lfb_region_probe.h"
#include "jump_table_guard_probe.h"
#include "launcher_probe.h"
#include "nvram_path_probe.h"
#include "pit_timer_probe.h"

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
    {"launcher", &repiu::tools::RunLauncherProbe},
};

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
    return failures == 0 ? 0 : 1;
}
