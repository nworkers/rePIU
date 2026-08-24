#ifndef REPIU_PLATFORM_HOST_ENVIRONMENT_H_
#define REPIU_PLATFORM_HOST_ENVIRONMENT_H_

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>

// Task 503d-9. Reading a setting out of the environment.
//
// The engine does this seventeen times, always in the same shape: copy into a
// small fixed buffer with `GetEnvironmentVariableA` and read the returned
// length, where zero means absent and a length at or beyond the buffer means
// the value was too long to be one of the words being looked for.
//
// `std::getenv` reports none of that by itself, so the three outcomes are named
// here rather than each caller re-deriving them -- and re-deriving them subtly
// differently, which is how the same idiom copied seventeen times usually ends.
//
// One difference is worth stating because it is not a mistake: on Windows,
// setting a variable to the empty string removes it, so `GetEnvironmentVariableA`
// never distinguished "set to empty" from "absent". POSIX does, and this treats
// an empty value as absent to keep the two hosts answering alike.

namespace repiu::platform
{

struct EnvironmentSetting
{
    // False when the variable is unset, or set to an empty value.
    bool present = false;
    // True when the value would not have fitted the caller's buffer. Several
    // callers treat that as a deliberate "off" rather than as a parse failure,
    // which is why it is reported apart from `present`.
    bool too_long = false;
    // Empty unless the value is present and fits. Points into the environment
    // block, which outlives every use here.
    std::string_view value;
};

inline EnvironmentSetting ReadEnvironmentSetting(const char* name,
                                                 const std::size_t capacity)
{
    EnvironmentSetting setting;
    if (name == nullptr)
    {
        return setting;
    }
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == 0)
    {
        return setting;
    }
    setting.present = true;
    const std::size_t length = std::strlen(value);
    if (length >= capacity)
    {
        setting.too_long = true;
        return setting;
    }
    setting.value = std::string_view(value, length);
    return setting;
}

// The common case: is the variable set to anything at all.
inline bool IsEnvironmentSettingPresent(const char* name)
{
    return ReadEnvironmentSetting(name, static_cast<std::size_t>(-1)).present;
}

// Task 503d-16. The whole environment, one `NAME=VALUE` entry at a time, in the
// order the host keeps them.
//
// This is a different question from the one above and needs a different answer.
// The DOS environment block the loader hands the guest is a copy of every
// variable, so nothing that reads one variable by name can build it.
//
// Declared rather than defined inline, because Windows answers it with
// `GetEnvironmentStringsA` and that would put `windows.h` back into a header --
// which is what 3d-2 spent a sub-stage taking out. `_environ`, the CRT's copy,
// would have kept this header-only, and it is not the same thing: it is
// populated at startup and does not see a later `SetEnvironmentVariable`. The
// port's rule is that Windows behaviour does not change, so Windows keeps
// asking the process.
//
// A callback rather than a returned container: the block is assembled by the
// caller into its own buffer, and the layer has no reason to allocate.
void ForEachEnvironmentEntry(void (*visit)(const char* entry, void* user_data),
                             void* user_data);

// Task 503d-17. The write half: the launcher publishes its settings into the
// environment so the engine reads them back by name.
//
// Windows answers this with `_putenv_s` rather than `SetEnvironmentVariableA`,
// and that is not interchangeable. This header reads one variable with
// `std::getenv`, which sees the CRT's copy, while `ForEachEnvironmentEntry`
// reads the process block; `SetEnvironmentVariableA` writes only the second, so
// a value published that way would be invisible to every `ReadEnvironmentSetting`
// caller in the engine. `_putenv_s` writes both, which is why it is what the
// launcher has always used and what this keeps.
//
// POSIX has no such split -- `setenv` updates `environ`, which is what both
// `getenv` and the enumeration read.
//
// Returns false when the host refused the write; the caller decides whether a
// setting that did not reach the engine is worth reporting.
bool PublishEnvironmentSetting(const char* name, const char* value);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_ENVIRONMENT_H_
