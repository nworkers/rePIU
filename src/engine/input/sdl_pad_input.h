#ifndef REPIU_ENGINE_INPUT_SDL_PAD_INPUT_H_
#define REPIU_ENGINE_INPUT_SDL_PAD_INPUT_H_

#include "repiu/input/host_pad_state.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>

#include <cstdint>

namespace repiu::engine
{

// Issue #34. Gamepads and joysticks on the SDL host thread: opens devices as
// they connect, numbers them in connection order, keeps what each is holding,
// and turns that into the JAMMA inputs the active bindings name. Owned and
// called by the thread that pumps SDL events; nothing here is thread safe
// except the mask it publishes through PublishJammaPadMask.
class SdlPadInput
{
public:
    struct Change
    {
        // The event was a gamepad or joystick event and needs nothing else.
        bool handled = false;
        std::uint16_t before = 0U;
        std::uint16_t after = 0U;
    };

    SdlPadInput() = default;
    ~SdlPadInput();
    SdlPadInput(const SdlPadInput&) = delete;
    SdlPadInput& operator=(const SdlPadInput&) = delete;

    // Initializes SDL's gamepad subsystem (joysticks included) and opens the
    // devices already attached. Failing is not an error for the run: input
    // then stays keyboard only. Safe to call more than once.
    bool Initialize();
    void Shutdown();

    Change HandleEvent(const SDL_Event& event);

    // Releases everything held, as losing focus does for keys: SDL sends no
    // pad events to a background window, so a press there would never end.
    Change ReleaseAll();

    std::uint16_t pressed_mask() const
    {
        return mask_;
    }
    // Issue #52: what every pad is holding, for the exit chord.
    const input::HostPadState& state() const
    {
        return state_;
    }

    bool initialized() const
    {
        return initialized_;
    }

private:
    void OpenJoystick(SDL_JoystickID id);
    void OpenGamepad(SDL_JoystickID id);
    void CloseJoystick(SDL_JoystickID id);
    void CloseGamepad(SDL_JoystickID id);
    Change Recompute();

    bool initialized_ = false;
    input::HostPadState state_;
    input::HostPadSlotTable<input::kMaxGamepadSlots> gamepad_slots_;
    input::HostPadSlotTable<input::kMaxJoystickSlots> joystick_slots_;
    SDL_Gamepad* gamepads_[input::kMaxGamepadSlots] = {};
    SDL_Joystick* joysticks_[input::kMaxJoystickSlots] = {};
    std::uint16_t mask_ = 0U;
};

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_INPUT_SDL_PAD_INPUT_H_
