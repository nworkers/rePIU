#include "repiu/platform/host_environment.h"

#if !defined(_WIN32)

#include <cstdlib>
#include <unistd.h>

namespace repiu::platform
{

// POSIX keeps the environment as a NULL-terminated array of `NAME=VALUE`
// pointers rather than as one double-terminated block, so the walk differs
// while what the caller sees does not.
//
// Nothing is freed here. `environ` points at storage the C runtime owns, which
// is the one respect in which this is simpler than the Windows backend.
void ForEachEnvironmentEntry(void (*visit)(const char* entry, void* user_data),
                             void* user_data)
{
    if (visit == nullptr || environ == nullptr)
    {
        return;
    }
    for (char** entry = environ; *entry != nullptr; ++entry)
    {
        visit(*entry, user_data);
    }
}

// Task 503d-17. POSIX keeps one environment, so there is no second place to
// write and nothing to keep in step. Overwrite is what the launcher means: a
// setting it publishes is the one the engine should read.
bool PublishEnvironmentSetting(const char* name, const char* value)
{
    if (name == nullptr || value == nullptr)
    {
        return false;
    }
    return setenv(name, value, 1) == 0;
}

}  // namespace repiu::platform

#endif  // !_WIN32
