#pragma once

#include "repiu/hle/bios_keyboard.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_events.h>

namespace repiu::platform::win32
{

bool TranslateSdlBiosKeystroke(SDL_Scancode scancode,
                               SDL_Keymod modifiers,
                               hle::BiosKeystroke* keystroke);
void HandleSdlBiosKeyboardEvent(const SDL_KeyboardEvent& event,
                                hle::BiosKeyboard* keyboard);
void HandleSdlBiosKeyboardFocusLost(hle::BiosKeyboard* keyboard);

}  // namespace repiu::platform::win32
