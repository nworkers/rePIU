#include "repiu/input/jamma_input_bindings.h"

#include "repiu/config/config_name.h"
#include "repiu/input/host_pad_binding.h"

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
//
// Issue #34 adds the standard gamepad after the keys: Pad1 for P1 and Pad2 for
// P2. The upper panels take the shoulder buttons, the lower ones the D-pad's
// down with left and B with right, and A or Start is the center -- the
// user's layout.
// Back inserts a coin on either pad; TEST and SERVICE stay keyboard only so a
// pad cannot open the operator menus, and CLEAR sits on the right stick click,
// which is hard to press by accident.
constexpr std::string_view kDefaultBindingText[] = {
    "Q, Pad1_LeftShoulder",
    "E, Pad1_RightShoulder",
    "Z, Pad1_DpadDown, Pad1_DpadLeft",
    "C, Pad1_B, Pad1_DpadRight",
    "S, Pad1_A, Pad1_Start",
    "F5, Pad1_Back, Pad2_Back",
    "F1",
    "F2",
    "F3, Pad1_RightStick",
    "Keypad7, Home, Pad2_LeftShoulder",
    "Keypad9, PageUp, Pad2_RightShoulder",
    "Keypad1, End, Pad2_DpadDown, Pad2_DpadLeft",
    "Keypad3, PageDown, Pad2_B, Pad2_DpadRight",
    "Keypad5, Clear, Pad2_A, Pad2_Start",
};

std::string_view TrimBlanks(std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t'))
    {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t'))
    {
        --end;
    }
    return text.substr(begin, end - begin);
}

// One config value, split into the binding's two alias lists. Each item is a
// pad control when it has the Pad<N>/Joy<N> shape and a host key otherwise.
// Unparsable items are reported and skipped, and items past either list's cap
// are reported once per list; an empty value leaves both lists empty, which is
// how an input is turned off.
void ParseBindingValue(std::string_view text, JammaInputBinding* binding,
                       std::vector<std::string>* errors)
{
    binding->alias_count = 0;
    binding->pad_alias_count = 0;
    bool keys_overflowed = false;
    bool pads_overflowed = false;

    std::size_t offset = 0;
    while (offset <= text.size())
    {
        const std::size_t separator = text.find(',', offset);
        const std::string_view element =
            separator == std::string_view::npos
                ? text.substr(offset)
                : text.substr(offset, separator - offset);
        offset = separator == std::string_view::npos ? text.size() + 1
                                                     : separator + 1;
        const std::string_view item = TrimBlanks(element);
        if (item.empty())
        {
            continue;
        }

        std::string error;
        if (LooksLikeHostPadAlias(item))
        {
            HostPadAlias pad;
            if (!ParseHostPadAlias(item, &pad, &error))
            {
                if (errors != nullptr)
                {
                    errors->push_back(std::move(error));
                }
                continue;
            }
            if (binding->pad_alias_count == kMaxAliasesPerInput)
            {
                pads_overflowed = true;
                continue;
            }
            binding->pad_aliases[binding->pad_alias_count++] = pad;
            continue;
        }

        HostKeyAlias key;
        if (!ParseHostKeyAlias(item, &key, &error))
        {
            if (errors != nullptr)
            {
                errors->push_back(std::move(error));
            }
            continue;
        }
        if (binding->alias_count == kMaxAliasesPerInput)
        {
            keys_overflowed = true;
            continue;
        }
        binding->aliases[binding->alias_count++] = key;
    }

    if (errors != nullptr)
    {
        if (keys_overflowed)
        {
            std::ostringstream stream;
            stream << "more than " << kMaxAliasesPerInput
                   << " keys; the extra ones are ignored";
            errors->push_back(stream.str());
        }
        if (pads_overflowed)
        {
            std::ostringstream stream;
            stream << "more than " << kMaxAliasesPerInput
                   << " pad controls; the extra ones are ignored";
            errors->push_back(stream.str());
        }
    }
}

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
        ParseBindingValue(kDefaultBindingText[index], &bindings.inputs[index],
                          nullptr);
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

        // The entry replaces the input's whole alias lists rather than
        // adding to them, so the written value is the complete answer for
        // that input -- keys and pad controls alike.
        //
        // That includes an empty value: `TEST =` leaves the input with no key
        // at all and the guest never sees it pressed. Turning an input off is
        // the point of writing an empty value, so it is applied like any other
        // value rather than skipped.
        std::vector<std::string> parse_errors;
        ParseBindingValue(entry.value,
                          &bindings->inputs[static_cast<std::uint32_t>(key)],
                          &parse_errors);
        for (std::string& error : parse_errors)
        {
            std::ostringstream stream;
            stream << entry.key << " on line " << entry.line_number << ": "
                   << error;
            warn(stream.str());
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
    for (std::uint32_t slot = 0; slot < binding.pad_alias_count; ++slot)
    {
        const std::string alias = FormatHostPadAlias(binding.pad_aliases[slot]);
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

std::uint16_t ComputeJammaPadMask(const ResolvedJammaBindings& bindings,
                                  const HostPadState& state)
{
    std::uint16_t mask = 0U;
    for (std::uint32_t index = 0; index < kJammaInputKeyCount; ++index)
    {
        const JammaInputBinding& binding = bindings.inputs[index];
        for (std::uint32_t slot = 0; slot < binding.pad_alias_count; ++slot)
        {
            if (IsHostPadAliasDown(binding.pad_aliases[slot], state))
            {
                mask = static_cast<std::uint16_t>(
                    mask |
                    JammaInputKeyMask(static_cast<JammaInputKey>(index)));
                break;
            }
        }
    }
    return mask;
}

}  // namespace repiu::input
