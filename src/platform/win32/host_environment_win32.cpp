#include "repiu/platform/host_environment.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdlib>

namespace repiu::platform
{

// The environment block is a run of NUL-terminated `NAME=VALUE` strings ending
// in an empty one. Entries whose name is empty exist and are handed on
// unchanged: `cmd.exe` records the current directory of each drive as `=C:`,
// and the loader has always copied them into the guest's block.
void ForEachEnvironmentEntry(void (*visit)(const char* entry, void* user_data),
                             void* user_data)
{
    if (visit == nullptr)
    {
        return;
    }
    LPCH environment = GetEnvironmentStringsA();
    if (environment == nullptr)
    {
        return;
    }
    for (const char* cursor = environment; *cursor != '\0';)
    {
        visit(cursor, user_data);
        while (*cursor != '\0')
        {
            ++cursor;
        }
        ++cursor;
    }
    FreeEnvironmentStringsA(environment);
}

// Task 503d-17. `_putenv_s` rather than `SetEnvironmentVariableA`: it updates
// the CRT's copy as well as the process block, and `ReadEnvironmentSetting`
// reads the copy. See the header for why that matters.
bool PublishEnvironmentSetting(const char* name, const char* value)
{
    if (name == nullptr || value == nullptr)
    {
        return false;
    }
    return _putenv_s(name, value) == 0;
}

}  // namespace repiu::platform

#endif  // _WIN32
