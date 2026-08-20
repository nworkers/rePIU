#include "win32_host_key_translation.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repiu::platform::win32
{
namespace
{

struct KeycodeToVirtualKey
{
    SDL_Keycode keycode;
    int virtual_key;
};

// Only the keys the name table can produce need an entry here; the probe
// asserts that every name-table keycode resolves, so a key added there without
// a virtual key is caught rather than silently going dead.
constexpr KeycodeToVirtualKey kExplicitVirtualKeys[] = {
    {SDLK_KP_0, VK_NUMPAD0},
    {SDLK_KP_1, VK_NUMPAD1},
    {SDLK_KP_2, VK_NUMPAD2},
    {SDLK_KP_3, VK_NUMPAD3},
    {SDLK_KP_4, VK_NUMPAD4},
    {SDLK_KP_5, VK_NUMPAD5},
    {SDLK_KP_6, VK_NUMPAD6},
    {SDLK_KP_7, VK_NUMPAD7},
    {SDLK_KP_8, VK_NUMPAD8},
    {SDLK_KP_9, VK_NUMPAD9},
    // Windows reports keypad Enter as VK_RETURN and separates it from the main
    // Enter only by the extended-key flag, which GetAsyncKeyState does not
    // expose. The two therefore share a virtual key on the polling path while
    // the SDL event path still tells them apart.
    {SDLK_KP_ENTER, VK_RETURN},
    {SDLK_KP_PLUS, VK_ADD},
    {SDLK_KP_MINUS, VK_SUBTRACT},
    {SDLK_KP_MULTIPLY, VK_MULTIPLY},
    {SDLK_KP_DIVIDE, VK_DIVIDE},
    {SDLK_KP_PERIOD, VK_DECIMAL},

    {SDLK_UP, VK_UP},
    {SDLK_DOWN, VK_DOWN},
    {SDLK_LEFT, VK_LEFT},
    {SDLK_RIGHT, VK_RIGHT},
    {SDLK_HOME, VK_HOME},
    {SDLK_END, VK_END},
    {SDLK_PAGEUP, VK_PRIOR},
    {SDLK_PAGEDOWN, VK_NEXT},
    {SDLK_INSERT, VK_INSERT},
    {SDLK_DELETE, VK_DELETE},
    // VK_CLEAR is the virtual key Windows reports for keypad 5 with NumLock
    // off. It has nothing to do with the JAMMA CLEAR system input.
    {SDLK_CLEAR, VK_CLEAR},

    {SDLK_SPACE, VK_SPACE},
    {SDLK_RETURN, VK_RETURN},
    {SDLK_TAB, VK_TAB},
    {SDLK_BACKSPACE, VK_BACK},
    {SDLK_ESCAPE, VK_ESCAPE},

    {SDLK_LSHIFT, VK_LSHIFT},
    {SDLK_RSHIFT, VK_RSHIFT},
    {SDLK_LCTRL, VK_LCONTROL},
    {SDLK_RCTRL, VK_RCONTROL},
    {SDLK_LALT, VK_LMENU},
    {SDLK_RALT, VK_RMENU},
};

struct ModifierProbe
{
    int virtual_key;
    SDL_Keymod mask;
};

constexpr ModifierProbe kModifierProbes[] = {
    {VK_LSHIFT, SDL_KMOD_LSHIFT}, {VK_RSHIFT, SDL_KMOD_RSHIFT},
    {VK_LCONTROL, SDL_KMOD_LCTRL}, {VK_RCONTROL, SDL_KMOD_RCTRL},
    {VK_LMENU, SDL_KMOD_LALT},    {VK_RMENU, SDL_KMOD_RALT},
};

}  // namespace

int SdlKeycodeToVirtualKey(SDL_Keycode keycode)
{
    // SDL reports letter keys as lowercase keycodes while Windows virtual keys
    // use the uppercase character values.
    if (keycode >= SDLK_A && keycode <= SDLK_Z)
    {
        return static_cast<int>('A' + (keycode - SDLK_A));
    }
    if (keycode >= SDLK_0 && keycode <= SDLK_9)
    {
        return static_cast<int>('0' + (keycode - SDLK_0));
    }
    if (keycode >= SDLK_F1 && keycode <= SDLK_F12)
    {
        return static_cast<int>(VK_F1 + (keycode - SDLK_F1));
    }

    for (const KeycodeToVirtualKey& entry : kExplicitVirtualKeys)
    {
        if (entry.keycode == keycode)
        {
            return entry.virtual_key;
        }
    }
    return 0;
}

void ResolveWin32VirtualKeys(input::ResolvedJammaBindings* bindings)
{
    if (bindings == nullptr)
    {
        return;
    }

    for (std::uint32_t index = 0; index < input::kJammaInputKeyCount; ++index)
    {
        input::JammaInputBinding& binding = bindings->inputs[index];
        for (std::uint32_t slot = 0; slot < binding.alias_count; ++slot)
        {
            input::HostKeyAlias& alias = binding.aliases[slot];
            alias.virtual_key = SdlKeycodeToVirtualKey(alias.keycode);
        }
    }
}

SDL_Keymod ReadWin32ModifierState()
{
    int state = 0;
    for (const ModifierProbe& probe : kModifierProbes)
    {
        if ((GetAsyncKeyState(probe.virtual_key) & 0x8000) != 0)
        {
            state |= probe.mask;
        }
    }
    return static_cast<SDL_Keymod>(state);
}

}  // namespace repiu::platform::win32
