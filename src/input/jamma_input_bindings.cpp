#include "repiu/input/jamma_input_bindings.h"

#include "repiu/config/config_name.h"

#include <SDL3/SDL_keyboard.h>

#include <sstream>

namespace repiu::input
{
namespace
{

// Bit assignments are the confirmed ones in
// docs/analysis/piu-io-port-specification.md. Inputs are active low: a bit
// reads 1 while released and drops to 0 while held.
constexpr JammaPortBit kJammaPortBits[] = {
    {0x02A8, 0x01, JammaInputKey::kP1UpLeft, "P1-UpLeft"},
    {0x02A8, 0x02, JammaInputKey::kP1UpRight, "P1-UpRight"},
    {0x02A8, 0x04, JammaInputKey::kP1Center, "P1-Center"},
    {0x02A8, 0x08, JammaInputKey::kP1DownLeft, "P1-DownLeft"},
    {0x02A8, 0x10, JammaInputKey::kP1DownRight, "P1-DownRight"},

    {0x02A9, 0x02, JammaInputKey::kTest, "TEST"},
    {0x02A9, 0x04, JammaInputKey::kCoin1, "COIN1"},
    // 0x40 is not a MAME-confirmed service input; it is rePIU's host
    // compatibility policy, recorded as such in the analysis document.
    {0x02A9, 0x40, JammaInputKey::kService, "SERVICE"},
    {0x02A9, 0x80, JammaInputKey::kClear, "CLEAR"},

    {0x02AA, 0x01, JammaInputKey::kP2UpLeft, "P2-UpLeft"},
    {0x02AA, 0x02, JammaInputKey::kP2UpRight, "P2-UpRight"},
    {0x02AA, 0x04, JammaInputKey::kP2Center, "P2-Center"},
    {0x02AA, 0x08, JammaInputKey::kP2DownLeft, "P2-DownLeft"},
    {0x02AA, 0x10, JammaInputKey::kP2DownRight, "P2-DownRight"},
};

constexpr std::uint32_t kJammaPortBitCount =
    static_cast<std::uint32_t>(sizeof(kJammaPortBits) /
                               sizeof(kJammaPortBits[0]));

static_assert(kJammaPortBitCount == kJammaInputKeyCount,
              "every JammaInputKey must map to exactly one port bit");

// Indexed by JammaInputKey. The P2 aliases are the NumLock-off keypad plus the
// editing keys Windows reports for those positions when NumLock is off, which
// is the pairing the hardcoded mapping used before this table existed.
constexpr std::string_view kDefaultBindingText[] = {
    "Q",
    "E",
    "Z",
    "C",
    "S",
    "F5",
    "F1",
    "F2",
    "F3",
    "Keypad7, Home",
    "Keypad9, PageUp",
    "Keypad1, End",
    "Keypad3, PageDown",
    "Keypad5, Clear",
};

static_assert(sizeof(kDefaultBindingText) / sizeof(kDefaultBindingText[0]) ==
                  kJammaInputKeyCount,
              "default binding table must cover every JammaInputKey");

}  // namespace

const JammaPortBit* JammaPortBitTable(std::uint32_t* count)
{
    if (count != nullptr)
    {
        *count = kJammaPortBitCount;
    }
    return kJammaPortBits;
}

std::string_view DefaultJammaBindingText(JammaInputKey key)
{
    const auto index = static_cast<std::uint32_t>(key);
    return index < kJammaInputKeyCount ? kDefaultBindingText[index]
                                       : std::string_view();
}

ResolvedJammaBindings DefaultJammaBindings()
{
    ResolvedJammaBindings bindings;
    for (std::uint32_t index = 0; index < kJammaInputKeyCount; ++index)
    {
        const std::vector<HostKeyAlias> aliases =
            ParseHostKeyBinding(kDefaultBindingText[index], nullptr);
        JammaInputBinding& binding = bindings.inputs[index];
        binding.alias_count = 0;
        for (const HostKeyAlias& alias : aliases)
        {
            if (binding.alias_count == kMaxAliasesPerInput)
            {
                break;
            }
            binding.aliases[binding.alias_count++] = alias;
        }
    }
    FinalizeJammaBindings(&bindings);
    return bindings;
}

void ApplyJammaInputSection(const config::IniDocument& document,
                            ResolvedJammaBindings* bindings,
                            std::vector<std::string>* warnings)
{
    if (bindings == nullptr)
    {
        return;
    }

    auto warn = [warnings](std::string message)
    {
        if (warnings != nullptr)
        {
            warnings->push_back(std::move(message));
        }
    };

    for (const config::IniEntry& entry : document.entries())
    {
        if (!config::EqualsConfigName(entry.section, kInputSectionName))
        {
            // Sections other than [Input] are not an error. The format is
            // meant to grow, and a file written for a later version must not
            // stop an earlier one from running.
            continue;
        }

        JammaInputKey key = JammaInputKey::kCount;
        if (!FindJammaInputKeyByConfigName(entry.key.c_str(), &key))
        {
            std::ostringstream stream;
            stream << "unknown [Input] entry \"" << entry.key << "\" on line "
                   << entry.line_number;
            warn(stream.str());
            continue;
        }

        std::vector<std::string> parse_errors;
        const std::vector<HostKeyAlias> aliases =
            ParseHostKeyBinding(entry.value, &parse_errors);
        for (std::string& error : parse_errors)
        {
            std::ostringstream stream;
            stream << entry.key << " on line " << entry.line_number << ": "
                   << error;
            warn(stream.str());
        }

        // The entry replaces the input's whole alias list rather than adding
        // to it, so the written value is the complete answer for that input.
        //
        // That includes an empty value: `TEST =` leaves the input with no key
        // at all and the guest never sees it pressed. Turning an input off is
        // the point of writing an empty value, so it is applied like any other
        // value rather than skipped.
        JammaInputBinding& binding =
            bindings->inputs[static_cast<std::uint32_t>(key)];
        binding.alias_count = 0;
        for (const HostKeyAlias& alias : aliases)
        {
            if (binding.alias_count == kMaxAliasesPerInput)
            {
                std::ostringstream stream;
                stream << entry.key << " on line " << entry.line_number
                       << ": more than " << kMaxAliasesPerInput
                       << " keys; the extra ones are ignored";
                warn(stream.str());
                break;
            }
            binding.aliases[binding.alias_count++] = alias;
        }
    }
}

void FinalizeJammaBindings(ResolvedJammaBindings* bindings)
{
    if (bindings == nullptr)
    {
        return;
    }

    HostKeyAlias* flattened[kJammaInputKeyCount * kMaxAliasesPerInput] = {};
    std::uint32_t flattened_count = 0;
    bool uses_modifiers = false;

    for (std::uint32_t index = 0; index < kJammaInputKeyCount; ++index)
    {
        JammaInputBinding& binding = bindings->inputs[index];
        for (std::uint32_t slot = 0; slot < binding.alias_count; ++slot)
        {
            HostKeyAlias& alias = binding.aliases[slot];
            // Contention is recomputed from scratch on every finalize, so a
            // forbidden mask derived from a layer that has since been
            // overridden cannot survive into the result.
            if (!alias.has_modifiers())
            {
                alias.forbidden = SDL_KMOD_NONE;
            }
            else
            {
                uses_modifiers = true;
            }
            flattened[flattened_count++] = &alias;
        }
    }

    ApplyModifierContention(flattened, flattened_count);
    bindings->any_binding_uses_modifiers = uses_modifiers;
}

std::string FormatJammaBinding(const ResolvedJammaBindings& bindings,
                               JammaInputKey key)
{
    const auto index = static_cast<std::uint32_t>(key);
    if (index >= kJammaInputKeyCount)
    {
        return std::string();
    }

    const JammaInputBinding& binding = bindings.inputs[index];
    std::string text;
    for (std::uint32_t slot = 0; slot < binding.alias_count; ++slot)
    {
        const std::string alias = FormatHostKeyAlias(binding.aliases[slot]);
        if (alias.empty())
        {
            continue;
        }
        if (!text.empty())
        {
            text.append(", ");
        }
        text.append(alias);
    }
    return text;
}

void ResolveJammaHostScancodes(ResolvedJammaBindings* bindings)
{
    if (bindings == nullptr)
    {
        return;
    }

    for (std::uint32_t index = 0; index < kJammaInputKeyCount; ++index)
    {
        JammaInputBinding& binding = bindings->inputs[index];
        for (std::uint32_t slot = 0; slot < binding.alias_count; ++slot)
        {
            HostKeyAlias& alias = binding.aliases[slot];
            // Before SDL has a keymap -- a tool that never initializes video,
            // or a static initializer running early -- this answers from SDL's
            // default layout rather than failing, which is why an unresolved
            // alias means the key genuinely has no place on the keyboard.
            alias.scancode = SDL_GetScancodeFromKey(alias.keycode, nullptr);
        }
    }
}

}  // namespace repiu::input
