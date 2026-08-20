#include "repiu/input/host_key_binding.h"

#include "repiu/config/config_name.h"
#include "repiu/input/host_key_names.h"

#include <sstream>

namespace repiu::input
{
namespace
{

constexpr std::string_view kWhitespace = " \t\r\f\v";

struct ModifierName
{
    std::string_view name;
    SDL_Keymod mask;
};

// Side-agnostic spellings come first only for readability; lookup is a linear
// scan by name. SDL already defines the combined masks, and "any bit of the
// mask is down" is exactly the semantics "Ctrl" needs.
constexpr ModifierName kModifierNames[] = {
    {"Ctrl", SDL_KMOD_CTRL},     {"Shift", SDL_KMOD_SHIFT},
    {"Alt", SDL_KMOD_ALT},       {"LeftCtrl", SDL_KMOD_LCTRL},
    {"RightCtrl", SDL_KMOD_RCTRL}, {"LeftShift", SDL_KMOD_LSHIFT},
    {"RightShift", SDL_KMOD_RSHIFT}, {"LeftAlt", SDL_KMOD_LALT},
    {"RightAlt", SDL_KMOD_RALT},
};

std::string_view Trim(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(kWhitespace);
    if (first == std::string_view::npos)
    {
        return std::string_view();
    }
    const std::size_t last = text.find_last_not_of(kWhitespace);
    return text.substr(first, last - first + 1);
}

bool FindModifier(std::string_view name, SDL_Keymod* mask)
{
    for (const ModifierName& entry : kModifierNames)
    {
        if (config::EqualsConfigName(name, entry.name))
        {
            *mask = entry.mask;
            return true;
        }
    }
    return false;
}

std::string_view FormatModifier(SDL_Keymod mask)
{
    for (const ModifierName& entry : kModifierNames)
    {
        if (entry.mask == mask)
        {
            return entry.name;
        }
    }
    return std::string_view();
}

}  // namespace

bool ParseHostKeyAlias(std::string_view text, HostKeyAlias* alias,
                       std::string* error)
{
    if (alias == nullptr)
    {
        return false;
    }

    auto fail = [error](std::string message) -> bool
    {
        if (error != nullptr)
        {
            *error = std::move(message);
        }
        return false;
    };

    const std::string_view trimmed = Trim(text);
    if (trimmed.empty())
    {
        return fail("empty key binding");
    }

    HostKeyAlias parsed;
    SDL_Keymod required_union = SDL_KMOD_NONE;

    std::size_t offset = 0;
    while (true)
    {
        const std::size_t separator = trimmed.find('+', offset);
        const std::string_view token = Trim(
            separator == std::string_view::npos
                ? trimmed.substr(offset)
                : trimmed.substr(offset, separator - offset));

        if (token.empty())
        {
            std::ostringstream stream;
            stream << "empty token in \"" << trimmed << "\"";
            return fail(stream.str());
        }

        if (separator == std::string_view::npos)
        {
            // The last token is the base key. Modifier spellings such as
            // "LeftShift" are also valid base keys, so this resolves against
            // the key name table and not the modifier table.
            SDL_Keycode keycode = SDLK_UNKNOWN;
            if (!FindHostKeyByName(token, &keycode))
            {
                std::ostringstream stream;
                stream << "unknown key name \"" << token << "\"";
                return fail(stream.str());
            }
            parsed.keycode = keycode;
            break;
        }

        SDL_Keymod mask = SDL_KMOD_NONE;
        if (!FindModifier(token, &mask))
        {
            std::ostringstream stream;
            stream << "\"" << token
                   << "\" is not a modifier; write the base key last";
            return fail(stream.str());
        }
        if (parsed.required_count == kMaxModifiersPerAlias)
        {
            std::ostringstream stream;
            stream << "too many modifiers in \"" << trimmed << "\"";
            return fail(stream.str());
        }
        // A repeated modifier would otherwise pass silently and consume a
        // slot, so it is rejected outright rather than deduplicated.
        if ((required_union & mask) != 0)
        {
            std::ostringstream stream;
            stream << "duplicate modifier \"" << token << "\"";
            return fail(stream.str());
        }
        parsed.required[parsed.required_count++] = mask;
        required_union = static_cast<SDL_Keymod>(required_union | mask);
        offset = separator + 1;
    }

    // Exact match: a qualified alias fires only for the modifiers it names, so
    // Ctrl+F1 does not also fire on Ctrl+Shift+F1.
    if (parsed.required_count != 0)
    {
        parsed.forbidden = static_cast<SDL_Keymod>(
            kConfigurableModifierMask & ~required_union);
    }

    *alias = parsed;
    return true;
}

std::vector<HostKeyAlias> ParseHostKeyBinding(std::string_view text,
                                              std::vector<std::string>* errors)
{
    std::vector<HostKeyAlias> aliases;
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

        const std::string_view trimmed = Trim(element);
        if (trimmed.empty())
        {
            // A wholly empty value means "no key for this input" and is not an
            // error. A stray comma inside a non-empty value is reported by the
            // alias parser only when it leaves a real token empty.
            continue;
        }

        HostKeyAlias alias;
        std::string error;
        if (!ParseHostKeyAlias(trimmed, &alias, &error))
        {
            if (errors != nullptr)
            {
                errors->push_back(std::move(error));
            }
            continue;
        }
        aliases.push_back(alias);
    }

    return aliases;
}

std::string FormatHostKeyAlias(const HostKeyAlias& alias)
{
    const std::string_view key_name = FindHostKeyName(alias.keycode);
    if (key_name.empty())
    {
        return std::string();
    }

    std::string text;
    for (std::uint32_t index = 0; index < alias.required_count; ++index)
    {
        const std::string_view modifier = FormatModifier(alias.required[index]);
        if (modifier.empty())
        {
            return std::string();
        }
        text.append(modifier);
        text.push_back('+');
    }
    text.append(key_name);
    return text;
}

void ApplyModifierContention(HostKeyAlias** aliases, std::uint32_t count)
{
    if (aliases == nullptr)
    {
        return;
    }

    for (std::uint32_t index = 0; index < count; ++index)
    {
        HostKeyAlias* subject = aliases[index];
        if (subject == nullptr || subject->has_modifiers())
        {
            continue;
        }

        // Collect the modifiers any other alias demands of this same base key.
        // Only those become forbidden, which keeps an uncontended plain key
        // matching regardless of modifier state exactly as it does today.
        SDL_Keymod contended = SDL_KMOD_NONE;
        for (std::uint32_t other = 0; other < count; ++other)
        {
            const HostKeyAlias* candidate = aliases[other];
            if (candidate == nullptr || candidate == subject ||
                candidate->keycode != subject->keycode)
            {
                continue;
            }
            for (std::uint32_t slot = 0; slot < candidate->required_count;
                 ++slot)
            {
                contended =
                    static_cast<SDL_Keymod>(contended |
                                            candidate->required[slot]);
            }
        }
        subject->forbidden = contended;
    }
}

}  // namespace repiu::input
