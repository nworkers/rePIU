#include "repiu/input/host_key_names.h"

#include "repiu/config/config_name.h"

namespace repiu::input
{
namespace
{

// Spelled out rather than generated. An earlier attempt built the letter,
// digit, function, and keypad runs at startup from their contiguous SDL
// keycode ranges, but the generated spellings had to live somewhere and every
// HostKeyName::name then pointed into that storage -- one copy of the table on
// return and every view dangles. A constexpr table of literals has no such
// lifetime to get wrong, needs no runtime initialization, and is checkable at
// compile time.
constexpr HostKeyName kHostKeyNames[] = {
    {"A", SDLK_A, HostKeyGroup::kLetter},
    {"B", SDLK_B, HostKeyGroup::kLetter},
    {"C", SDLK_C, HostKeyGroup::kLetter},
    {"D", SDLK_D, HostKeyGroup::kLetter},
    {"E", SDLK_E, HostKeyGroup::kLetter},
    {"F", SDLK_F, HostKeyGroup::kLetter},
    {"G", SDLK_G, HostKeyGroup::kLetter},
    {"H", SDLK_H, HostKeyGroup::kLetter},
    {"I", SDLK_I, HostKeyGroup::kLetter},
    {"J", SDLK_J, HostKeyGroup::kLetter},
    {"K", SDLK_K, HostKeyGroup::kLetter},
    {"L", SDLK_L, HostKeyGroup::kLetter},
    {"M", SDLK_M, HostKeyGroup::kLetter},
    {"N", SDLK_N, HostKeyGroup::kLetter},
    {"O", SDLK_O, HostKeyGroup::kLetter},
    {"P", SDLK_P, HostKeyGroup::kLetter},
    {"Q", SDLK_Q, HostKeyGroup::kLetter},
    {"R", SDLK_R, HostKeyGroup::kLetter},
    {"S", SDLK_S, HostKeyGroup::kLetter},
    {"T", SDLK_T, HostKeyGroup::kLetter},
    {"U", SDLK_U, HostKeyGroup::kLetter},
    {"V", SDLK_V, HostKeyGroup::kLetter},
    {"W", SDLK_W, HostKeyGroup::kLetter},
    {"X", SDLK_X, HostKeyGroup::kLetter},
    {"Y", SDLK_Y, HostKeyGroup::kLetter},
    {"Z", SDLK_Z, HostKeyGroup::kLetter},

    {"0", SDLK_0, HostKeyGroup::kDigit},
    {"1", SDLK_1, HostKeyGroup::kDigit},
    {"2", SDLK_2, HostKeyGroup::kDigit},
    {"3", SDLK_3, HostKeyGroup::kDigit},
    {"4", SDLK_4, HostKeyGroup::kDigit},
    {"5", SDLK_5, HostKeyGroup::kDigit},
    {"6", SDLK_6, HostKeyGroup::kDigit},
    {"7", SDLK_7, HostKeyGroup::kDigit},
    {"8", SDLK_8, HostKeyGroup::kDigit},
    {"9", SDLK_9, HostKeyGroup::kDigit},

    {"F1", SDLK_F1, HostKeyGroup::kFunction},
    {"F2", SDLK_F2, HostKeyGroup::kFunction},
    {"F3", SDLK_F3, HostKeyGroup::kFunction},
    {"F4", SDLK_F4, HostKeyGroup::kFunction},
    {"F5", SDLK_F5, HostKeyGroup::kFunction},
    {"F6", SDLK_F6, HostKeyGroup::kFunction},
    {"F7", SDLK_F7, HostKeyGroup::kFunction},
    {"F8", SDLK_F8, HostKeyGroup::kFunction},
    {"F9", SDLK_F9, HostKeyGroup::kFunction},
    {"F10", SDLK_F10, HostKeyGroup::kFunction},
    {"F11", SDLK_F11, HostKeyGroup::kFunction},
    {"F12", SDLK_F12, HostKeyGroup::kFunction},

    {"Keypad0", SDLK_KP_0, HostKeyGroup::kKeypad},
    {"Keypad1", SDLK_KP_1, HostKeyGroup::kKeypad},
    {"Keypad2", SDLK_KP_2, HostKeyGroup::kKeypad},
    {"Keypad3", SDLK_KP_3, HostKeyGroup::kKeypad},
    {"Keypad4", SDLK_KP_4, HostKeyGroup::kKeypad},
    {"Keypad5", SDLK_KP_5, HostKeyGroup::kKeypad},
    {"Keypad6", SDLK_KP_6, HostKeyGroup::kKeypad},
    {"Keypad7", SDLK_KP_7, HostKeyGroup::kKeypad},
    {"Keypad8", SDLK_KP_8, HostKeyGroup::kKeypad},
    {"Keypad9", SDLK_KP_9, HostKeyGroup::kKeypad},
    {"KeypadEnter", SDLK_KP_ENTER, HostKeyGroup::kKeypad},
    {"KeypadPlus", SDLK_KP_PLUS, HostKeyGroup::kKeypad},
    {"KeypadMinus", SDLK_KP_MINUS, HostKeyGroup::kKeypad},
    {"KeypadMultiply", SDLK_KP_MULTIPLY, HostKeyGroup::kKeypad},
    {"KeypadDivide", SDLK_KP_DIVIDE, HostKeyGroup::kKeypad},
    {"KeypadPeriod", SDLK_KP_PERIOD, HostKeyGroup::kKeypad},

    {"Up", SDLK_UP, HostKeyGroup::kNavigation},
    {"Down", SDLK_DOWN, HostKeyGroup::kNavigation},
    {"Left", SDLK_LEFT, HostKeyGroup::kNavigation},
    {"Right", SDLK_RIGHT, HostKeyGroup::kNavigation},
    {"Home", SDLK_HOME, HostKeyGroup::kNavigation},
    {"End", SDLK_END, HostKeyGroup::kNavigation},
    {"PageUp", SDLK_PAGEUP, HostKeyGroup::kNavigation},
    {"PageDown", SDLK_PAGEDOWN, HostKeyGroup::kNavigation},
    {"Insert", SDLK_INSERT, HostKeyGroup::kNavigation},
    {"Delete", SDLK_DELETE, HostKeyGroup::kNavigation},
    // Keypad 5 with NumLock off. Unrelated to the JAMMA CLEAR system input.
    {"Clear", SDLK_CLEAR, HostKeyGroup::kNavigation},

    {"Space", SDLK_SPACE, HostKeyGroup::kOther},
    {"Enter", SDLK_RETURN, HostKeyGroup::kOther},
    {"Tab", SDLK_TAB, HostKeyGroup::kOther},
    {"Backspace", SDLK_BACKSPACE, HostKeyGroup::kOther},
    {"Escape", SDLK_ESCAPE, HostKeyGroup::kOther},

    {"LeftShift", SDLK_LSHIFT, HostKeyGroup::kModifier},
    {"RightShift", SDLK_RSHIFT, HostKeyGroup::kModifier},
    {"LeftCtrl", SDLK_LCTRL, HostKeyGroup::kModifier},
    {"RightCtrl", SDLK_RCTRL, HostKeyGroup::kModifier},
    {"LeftAlt", SDLK_LALT, HostKeyGroup::kModifier},
    {"RightAlt", SDLK_RALT, HostKeyGroup::kModifier},
};

constexpr std::uint32_t kHostKeyNameCount =
    static_cast<std::uint32_t>(sizeof(kHostKeyNames) /
                               sizeof(kHostKeyNames[0]));

}  // namespace

const HostKeyName* HostKeyNameTable(std::uint32_t* count)
{
    if (count != nullptr)
    {
        *count = kHostKeyNameCount;
    }
    return kHostKeyNames;
}

bool FindHostKeyByName(std::string_view name, SDL_Keycode* keycode)
{
    if (keycode == nullptr || name.empty())
    {
        return false;
    }

    for (const HostKeyName& entry : kHostKeyNames)
    {
        if (config::EqualsConfigName(name, entry.name))
        {
            *keycode = entry.keycode;
            return true;
        }
    }
    return false;
}

std::string_view FindHostKeyName(SDL_Keycode keycode)
{
    for (const HostKeyName& entry : kHostKeyNames)
    {
        if (entry.keycode == keycode)
        {
            return entry.name;
        }
    }
    return std::string_view();
}

}  // namespace repiu::input
