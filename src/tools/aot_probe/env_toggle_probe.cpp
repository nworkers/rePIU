#include "env_toggle_probe.h"

#include "repiu/platform/host_environment.h"
#include "repiu/runtime/env_toggle.h"

#include <cstddef>
#include <cstring>
#include <iostream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace repiu::tools
{
namespace
{

// Task 503d-16. What the engine copies into the guest's DOS environment block.
//
// The two backends walk different structures -- a double-terminated block on
// Windows, a pointer array on POSIX -- so what this asserts is only what they
// are required to have in common.
struct EnumerationTally
{
    std::size_t entries = 0;
    bool found_injected = false;
    bool every_entry_nonempty = true;
};

constexpr const char* kInjectedName = "REPIU_ENV_BLOCK_PROBE";
constexpr const char* kInjectedEntry =
    "REPIU_ENV_BLOCK_PROBE=repiu-probe-value";

void TallyEntry(const char* entry, void* user_data)
{
    auto& tally = *static_cast<EnumerationTally*>(user_data);
    ++tally.entries;
    if (entry == nullptr || entry[0] == 0)
    {
        tally.every_entry_nonempty = false;
        return;
    }
    if (std::strcmp(entry, kInjectedEntry) == 0)
    {
        tally.found_injected = true;
    }
}

// Set through the host's own interface rather than the C runtime's. On Windows
// that distinction is the point of the backend: `SetEnvironmentVariableA`
// writes the process environment block, and an implementation reading the CRT's
// `_environ` copy instead would not necessarily see this at all.
void SetProbeVariable(const char* value)
{
#if defined(_WIN32)
    SetEnvironmentVariableA(kInjectedName, value);
#else
    if (value == nullptr)
    {
        unsetenv(kInjectedName);
    }
    else
    {
        setenv(kInjectedName, value, 1);
    }
#endif
}

bool ProbeEnvironmentEnumeration()
{
    SetProbeVariable("repiu-probe-value");

    EnumerationTally tally;
    repiu::platform::ForEachEnvironmentEntry(&TallyEntry, &tally);

    // A process always has some environment, so an empty walk means the backend
    // answered without looking -- which is exactly what the fenced-out call site
    // this layer replaces was doing on Linux.
    bool ok = tally.entries > 0 && tally.every_entry_nonempty &&
        tally.found_injected;

    // And what it reports is the environment as it stands, not a snapshot taken
    // when the layer was first called.
    SetProbeVariable(nullptr);
    EnumerationTally after;
    repiu::platform::ForEachEnvironmentEntry(&TallyEntry, &after);
    ok = ok && after.entries > 0 && !after.found_injected;

    return ok;
}

}  // namespace

bool RunEnvToggleProbe()
{
    // The complete truth table from §1 of the Task 424 work order. The only two
    // rows where the functions answer differently are unset and empty; every
    // other row must agree.
    const struct
    {
        const char* value;
        bool promoted;
        bool opt_in;
    } cases[] = {
        {nullptr, true, false},
        {"", true, false},
        {"1", true, true},
        {"on", true, true},
        {"true", true, true},
        {"0", false, false},
        {"off", false, false},
        {"false", false, false},
        // An unknown value is a fail-closed OFF in both conventions. Spellings
        // that differ only in case belong here too, so a regression toward
        // permissiveness is caught.
        {"yes", false, false},
        {"no", false, false},
        {"2", false, false},
        {"ON", false, false},
        {"True", false, false},
        {"TRUE", false, false},
        {" 1", false, false},
        {"1 ", false, false},
        {"enable", false, false},
    };

    bool all = true;
    for (const auto& test : cases)
    {
        all = all &&
            runtime::ResolvePromotedToggle(test.value) == test.promoted &&
            runtime::ResolveOptInToggle(test.value) == test.opt_in;
    }

    const bool enumeration_ok = ProbeEnvironmentEnumeration();

    std::cout << "env_toggle_policy=" << (all ? "true" : "false")
              << "\nenv_block_enumeration="
              << (enumeration_ok ? "true" : "false") << "\n";
    return all && enumeration_ok;
}

}  // namespace repiu::tools
