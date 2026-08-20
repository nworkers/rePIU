#include "repiu/input/jamma_input_key.h"

#include "repiu/config/config_name.h"

namespace repiu::input
{
namespace
{

// Indexed by JammaInputKey, so this array and the enumerator order must stay
// in step. The static_assert below is what enforces that.
constexpr const char* kConfigNames[] = {
    "P1_UP_LEFT",
    "P1_UP_RIGHT",
    "P1_DOWN_LEFT",
    "P1_DOWN_RIGHT",
    "P1_CENTER",
    "COIN1",
    "TEST",
    "SERVICE",
    "CLEAR",
    "P2_UP_LEFT",
    "P2_UP_RIGHT",
    "P2_DOWN_LEFT",
    "P2_DOWN_RIGHT",
    "P2_CENTER",
};

static_assert(sizeof(kConfigNames) / sizeof(kConfigNames[0]) ==
                  kJammaInputKeyCount,
              "config name table must cover every JammaInputKey");

}  // namespace

const char* JammaInputKeyConfigName(JammaInputKey key)
{
    const auto index = static_cast<std::uint32_t>(key);
    return index < kJammaInputKeyCount ? kConfigNames[index] : "";
}

bool FindJammaInputKeyByConfigName(const char* name, JammaInputKey* key)
{
    if (name == nullptr || key == nullptr)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < kJammaInputKeyCount; ++index)
    {
        if (config::EqualsConfigName(name, kConfigNames[index]))
        {
            *key = static_cast<JammaInputKey>(index);
            return true;
        }
    }

    return false;
}

}  // namespace repiu::input
