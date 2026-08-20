#ifndef REPIU_PLATFORM_WIN32_INPUT_WIN32_HOST_KEY_TRANSLATION_H_
#define REPIU_PLATFORM_WIN32_INPUT_WIN32_HOST_KEY_TRANSLATION_H_

#include "repiu/input/jamma_input_bindings.h"

#include <SDL3/SDL_keycode.h>

namespace repiu::platform::win32
{

// SDL_Keycode is the canonical key type everywhere else. This is the one
// remaining translation, and it exists only because the polling path in
// ScanJammaPort8 reads key state through GetAsyncKeyState, which speaks Win32
// virtual keys.
//
// Returns 0 when the keycode has no virtual key.
int SdlKeycodeToVirtualKey(SDL_Keycode keycode);

// Fills HostKeyAlias::virtual_key for every alias, once, at load time. The
// scan path must never convert: Task 403 measured the host key query as 99.21%
// of the port I/O handler body, and adding a lookup per read would put the
// cost straight back.
void ResolveWin32VirtualKeys(input::ResolvedJammaBindings* bindings);

// Assembles the current modifier state as an SDL_Keymod so the polling path
// and the SDL event path compare the same type through the same match logic.
//
// Costs six GetAsyncKeyState calls, so the caller must only reach here when
// ResolvedJammaBindings::any_binding_uses_modifiers is set, and must hoist it
// out of any per-alias loop.
SDL_Keymod ReadWin32ModifierState();

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_INPUT_WIN32_HOST_KEY_TRANSLATION_H_
