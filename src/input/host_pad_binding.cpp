#include "repiu/input/host_pad_binding.h"

#include "repiu/config/config_name.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>

#include <sstream>

namespace repiu::input
{
namespace
{

// Canonical names first: these are what FormatHostPadAlias writes and what
// the generated config file lists.
constexpr HostPadButtonName kButtonNames[] = {
    {"A", SDL_GAMEPAD_BUTTON_SOUTH},
    {"B", SDL_GAMEPAD_BUTTON_EAST},
    {"X", SDL_GAMEPAD_BUTTON_WEST},
    {"Y", SDL_GAMEPAD_BUTTON_NORTH},
    {"Back", SDL_GAMEPAD_BUTTON_BACK},
    {"Guide", SDL_GAMEPAD_BUTTON_GUIDE},
    {"Start", SDL_GAMEPAD_BUTTON_START},
    {"LeftStick", SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"RightStick", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"LeftShoulder", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"RightShoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"DpadUp", SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"DpadDown", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"DpadLeft", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"DpadRight", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"Misc1", SDL_GAMEPAD_BUTTON_MISC1},
    {"Touchpad", SDL_GAMEPAD_BUTTON_TOUCHPAD},
};

// SDL names the face buttons by position because the printed labels differ
// between makers; the positional spellings are accepted too.
constexpr HostPadButtonName kButtonSynonyms[] = {
    {"South", SDL_GAMEPAD_BUTTON_SOUTH},
    {"East", SDL_GAMEPAD_BUTTON_EAST},
    {"West", SDL_GAMEPAD_BUTTON_WEST},
    {"North", SDL_GAMEPAD_BUTTON_NORTH},
};

static_assert(SDL_GAMEPAD_BUTTON_COUNT <= 32,
              "gamepad buttons are kept in a 32-bit mask");

struct HatDirectionName
{
    std::string_view name;
    std::uint8_t bit;
};

constexpr HatDirectionName kHatDirections[] = {
    {"Up", SDL_HAT_UP},
    {"Down", SDL_HAT_DOWN},
    {"Left", SDL_HAT_LEFT},
    {"Right", SDL_HAT_RIGHT},
};

// Lower case with underscores and surrounding blanks removed, the same rule
// EqualsConfigName applies, so prefixes and numbers can be read positionally.
std::string Normalize(std::string_view text)
{
    std::string normalized;
    normalized.reserve(text.size());
    for (const char value : text)
    {
        if (value == '_' || value == ' ' || value == '\t')
        {
            continue;
        }
        normalized.push_back(value >= 'A' && value <= 'Z'
                                 ? static_cast<char>(value - 'A' + 'a')
                                 : value);
    }
    return normalized;
}

bool IsDigit(char value)
{
    return value >= '0' && value <= '9';
}

// Reads the decimal number starting at `*offset`. At most four digits, so an
// absurd number fails the range check rather than overflowing.
bool ReadNumber(std::string_view text, std::size_t* offset,
                std::uint32_t* value)
{
    std::size_t index = *offset;
    std::uint32_t result = 0;
    std::size_t digits = 0;
    while (index < text.size() && IsDigit(text[index]) && digits < 4)
    {
        result = result * 10U + static_cast<std::uint32_t>(text[index] - '0');
        ++index;
        ++digits;
    }
    if (digits == 0)
    {
        return false;
    }
    *offset = index;
    *value = result;
    return true;
}

bool Fail(std::string_view text, std::string_view reason, std::string* error)
{
    if (error != nullptr)
    {
        std::ostringstream stream;
        stream << "\"" << text << "\": " << reason;
        *error = stream.str();
    }
    return false;
}

}  // namespace

bool LooksLikeHostPadAlias(std::string_view text)
{
    const std::string normalized = Normalize(text);
    return normalized.size() > 3 &&
           (normalized.compare(0, 3, "pad") == 0 ||
            normalized.compare(0, 3, "joy") == 0) &&
           IsDigit(normalized[3]);
}

bool ParseHostPadAlias(std::string_view text, HostPadAlias* alias,
                       std::string* error)
{
    if (alias == nullptr)
    {
        return false;
    }
    const std::string normalized = Normalize(text);
    if (!LooksLikeHostPadAlias(text))
    {
        return Fail(text, "not a Pad<N>_ or Joy<N>_ name", error);
    }

    const bool gamepad = normalized.compare(0, 3, "pad") == 0;
    std::size_t offset = 3;
    std::uint32_t device = 0;
    ReadNumber(normalized, &offset, &device);
    const std::uint32_t slot_limit =
        gamepad ? kMaxGamepadSlots : kMaxJoystickSlots;
    if (device < 1 || device > slot_limit)
    {
        return Fail(text,
                    gamepad ? "gamepad number must be 1 to 4"
                            : "joystick number must be 1 to 8",
                    error);
    }
    const std::string_view rest = std::string_view(normalized).substr(offset);

    HostPadAlias parsed;
    parsed.slot = static_cast<std::uint8_t>(device - 1U);

    if (gamepad)
    {
        if (config::EqualsConfigName(rest, "LeftTrigger"))
        {
            parsed.kind = HostPadControlKind::kGamepadTrigger;
            parsed.control = 0;
            *alias = parsed;
            return true;
        }
        if (config::EqualsConfigName(rest, "RightTrigger"))
        {
            parsed.kind = HostPadControlKind::kGamepadTrigger;
            parsed.control = 1;
            *alias = parsed;
            return true;
        }
        for (const HostPadButtonName& entry : kButtonNames)
        {
            if (config::EqualsConfigName(rest, entry.name))
            {
                parsed.kind = HostPadControlKind::kGamepadButton;
                parsed.control = entry.button;
                *alias = parsed;
                return true;
            }
        }
        for (const HostPadButtonName& entry : kButtonSynonyms)
        {
            if (config::EqualsConfigName(rest, entry.name))
            {
                parsed.kind = HostPadControlKind::kGamepadButton;
                parsed.control = entry.button;
                *alias = parsed;
                return true;
            }
        }
        return Fail(text, "unknown gamepad button", error);
    }

    if (rest.compare(0, 6, "button") == 0)
    {
        std::size_t number_offset = 6;
        std::uint32_t button = 0;
        if (!ReadNumber(rest, &number_offset, &button) ||
            number_offset != rest.size())
        {
            return Fail(text, "expected Joy<N>_Button<K>", error);
        }
        if (button < 1 || button > kMaxJoystickButtons)
        {
            return Fail(text, "joystick button must be 1 to 32", error);
        }
        parsed.kind = HostPadControlKind::kJoystickButton;
        parsed.control = static_cast<std::uint8_t>(button - 1U);
        *alias = parsed;
        return true;
    }

    if (rest.compare(0, 3, "hat") == 0)
    {
        std::size_t number_offset = 3;
        std::uint32_t hat = 0;
        if (!ReadNumber(rest, &number_offset, &hat))
        {
            return Fail(text, "expected Joy<N>_Hat<H><Up|Down|Left|Right>",
                        error);
        }
        if (hat < 1 || hat > kMaxJoystickHats)
        {
            return Fail(text, "joystick hat must be 1 to 4", error);
        }
        const std::string_view direction = rest.substr(number_offset);
        for (const HatDirectionName& entry : kHatDirections)
        {
            if (config::EqualsConfigName(direction, entry.name))
            {
                parsed.kind = HostPadControlKind::kJoystickHat;
                parsed.control = static_cast<std::uint8_t>(hat - 1U);
                parsed.hat_direction = entry.bit;
                *alias = parsed;
                return true;
            }
        }
        return Fail(text, "hat direction must be Up, Down, Left or Right",
                    error);
    }

    return Fail(text, "expected Joy<N>_Button<K> or Joy<N>_Hat<H><Dir>",
                error);
}

std::string FormatHostPadAlias(const HostPadAlias& alias)
{
    std::ostringstream stream;
    switch (alias.kind)
    {
        case HostPadControlKind::kGamepadButton:
            for (const HostPadButtonName& entry : kButtonNames)
            {
                if (entry.button == alias.control)
                {
                    stream << "Pad" << (alias.slot + 1U) << "_" << entry.name;
                    return stream.str();
                }
            }
            return std::string();
        case HostPadControlKind::kGamepadTrigger:
            stream << "Pad" << (alias.slot + 1U) << "_"
                   << (alias.control == 0 ? "LeftTrigger" : "RightTrigger");
            return stream.str();
        case HostPadControlKind::kJoystickButton:
            stream << "Joy" << (alias.slot + 1U) << "_Button"
                   << (alias.control + 1U);
            return stream.str();
        case HostPadControlKind::kJoystickHat:
            for (const HatDirectionName& entry : kHatDirections)
            {
                if (entry.bit == alias.hat_direction)
                {
                    stream << "Joy" << (alias.slot + 1U) << "_Hat"
                           << (alias.control + 1U) << entry.name;
                    return stream.str();
                }
            }
            return std::string();
        case HostPadControlKind::kNone:
            break;
    }
    return std::string();
}

const HostPadButtonName* HostPadButtonNameTable(std::uint32_t* count)
{
    if (count != nullptr)
    {
        *count = static_cast<std::uint32_t>(sizeof(kButtonNames) /
                                            sizeof(kButtonNames[0]));
    }
    return kButtonNames;
}

}  // namespace repiu::input
