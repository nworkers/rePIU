#include "sdl_pad_input.h"

#include "repiu/engine/active_jamma_bindings.h"
#include "repiu/input/jamma_input_bindings.h"

#include <SDL3/SDL_init.h>

#include <cstdio>

namespace repiu::engine
{
namespace
{

// A trigger counts as held past half travel. SDL reports 0..32767.
constexpr Sint16 kTriggerThreshold = input::kHostPadTriggerThreshold;

}  // namespace

SdlPadInput::~SdlPadInput()
{
    Shutdown();
}

bool SdlPadInput::Initialize()
{
    if (initialized_)
    {
        return true;
    }
    // SDL_INIT_GAMEPAD implies SDL_INIT_JOYSTICK.
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
    {
        std::fprintf(stderr,
                     "[repiu-pad] gamepad input unavailable: %s\n",
                     SDL_GetError());
        return false;
    }
    initialized_ = true;

    // Devices present before the subsystem started may or may not arrive as
    // added events, so they are opened here; a later added event for the same
    // id finds its slot taken and does nothing.
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (ids != nullptr)
    {
        for (int index = 0; index < count; ++index)
        {
            OpenJoystick(ids[index]);
            if (SDL_IsGamepad(ids[index]))
            {
                OpenGamepad(ids[index]);
            }
        }
        SDL_free(ids);
    }
    return true;
}

void SdlPadInput::Shutdown()
{
    if (!initialized_)
    {
        return;
    }
    for (std::uint32_t slot = 0; slot < input::kMaxGamepadSlots; ++slot)
    {
        if (gamepads_[slot] != nullptr)
        {
            SDL_CloseGamepad(gamepads_[slot]);
            gamepads_[slot] = nullptr;
        }
    }
    for (std::uint32_t slot = 0; slot < input::kMaxJoystickSlots; ++slot)
    {
        if (joysticks_[slot] != nullptr)
        {
            SDL_CloseJoystick(joysticks_[slot]);
            joysticks_[slot] = nullptr;
        }
    }
    gamepad_slots_ = {};
    joystick_slots_ = {};
    state_.ClearAll();
    mask_ = 0U;
    PublishJammaPadMask(0U);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    initialized_ = false;
}

void SdlPadInput::OpenJoystick(SDL_JoystickID id)
{
    if (joystick_slots_.Find(id) >= 0)
    {
        return;
    }
    SDL_Joystick* joystick = SDL_OpenJoystick(id);
    if (joystick == nullptr)
    {
        return;
    }
    const int slot = joystick_slots_.Assign(id);
    if (slot < 0)
    {
        // More devices than numbers: this one stays unbound.
        SDL_CloseJoystick(joystick);
        return;
    }
    joysticks_[slot] = joystick;
    state_.ClearJoystick(static_cast<std::uint32_t>(slot));
    const char* name = SDL_GetJoystickName(joystick);
    std::fprintf(stderr, "[repiu-pad] Joy%d connected: %s\n", slot + 1,
                 name != nullptr ? name : "(unnamed)");
}

void SdlPadInput::OpenGamepad(SDL_JoystickID id)
{
    if (gamepad_slots_.Find(id) >= 0)
    {
        return;
    }
    SDL_Gamepad* gamepad = SDL_OpenGamepad(id);
    if (gamepad == nullptr)
    {
        return;
    }
    const int slot = gamepad_slots_.Assign(id);
    if (slot < 0)
    {
        SDL_CloseGamepad(gamepad);
        return;
    }
    gamepads_[slot] = gamepad;
    state_.ClearGamepad(static_cast<std::uint32_t>(slot));
    const char* name = SDL_GetGamepadName(gamepad);
    std::fprintf(stderr, "[repiu-pad] Pad%d connected: %s\n", slot + 1,
                 name != nullptr ? name : "(unnamed)");
}

void SdlPadInput::CloseJoystick(SDL_JoystickID id)
{
    const int slot = joystick_slots_.Release(id);
    if (slot < 0)
    {
        return;
    }
    if (joysticks_[slot] != nullptr)
    {
        SDL_CloseJoystick(joysticks_[slot]);
        joysticks_[slot] = nullptr;
    }
    state_.ClearJoystick(static_cast<std::uint32_t>(slot));
    std::fprintf(stderr, "[repiu-pad] Joy%d disconnected\n", slot + 1);
}

void SdlPadInput::CloseGamepad(SDL_JoystickID id)
{
    const int slot = gamepad_slots_.Release(id);
    if (slot < 0)
    {
        return;
    }
    if (gamepads_[slot] != nullptr)
    {
        SDL_CloseGamepad(gamepads_[slot]);
        gamepads_[slot] = nullptr;
    }
    state_.ClearGamepad(static_cast<std::uint32_t>(slot));
    std::fprintf(stderr, "[repiu-pad] Pad%d disconnected\n", slot + 1);
}

SdlPadInput::Change SdlPadInput::Recompute()
{
    Change change;
    change.handled = true;
    change.before = mask_;
    mask_ = game_gate_.Apply(
        input::ComputeJammaPadMask(ActiveJammaBindings(), state_));
    change.after = mask_;
    if (change.after != change.before)
    {
        PublishJammaPadMask(mask_);
    }
    return change;
}

SdlPadInput::Change SdlPadInput::SetGameInputSuppressed(bool suppressed)
{
    game_gate_.SetSuppressed(
        suppressed, input::ComputeJammaPadMask(ActiveJammaBindings(), state_));
    return Recompute();
}

SdlPadInput::Change SdlPadInput::ReleaseAll()
{
    state_.ClearAll();
    return Recompute();
}

SdlPadInput::Change SdlPadInput::HandleEvent(const SDL_Event& event)
{
    if (!initialized_)
    {
        return Change{};
    }

    switch (event.type)
    {
        case SDL_EVENT_JOYSTICK_ADDED:
            OpenJoystick(event.jdevice.which);
            return Recompute();
        case SDL_EVENT_JOYSTICK_REMOVED:
            CloseJoystick(event.jdevice.which);
            return Recompute();
        case SDL_EVENT_GAMEPAD_ADDED:
            OpenGamepad(event.gdevice.which);
            return Recompute();
        case SDL_EVENT_GAMEPAD_REMOVED:
            CloseGamepad(event.gdevice.which);
            return Recompute();

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            const int slot = gamepad_slots_.Find(event.gbutton.which);
            if (slot >= 0 && event.gbutton.button < 32U)
            {
                const std::uint32_t bit = 1U << event.gbutton.button;
                if (event.gbutton.down)
                {
                    state_.gamepad_buttons[slot] |= bit;
                }
                else
                {
                    state_.gamepad_buttons[slot] &= ~bit;
                }
            }
            return Recompute();
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            const int slot = gamepad_slots_.Find(event.gaxis.which);
            std::uint8_t bit = 0U;
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            {
                bit = 0x01U;
            }
            else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            {
                bit = 0x02U;
            }
            if (slot < 0 || bit == 0U)
            {
                // Stick motion arrives constantly and is not bound; it must
                // not cost a recompute.
                Change ignored;
                ignored.handled = true;
                ignored.before = mask_;
                ignored.after = mask_;
                return ignored;
            }
            if (event.gaxis.value >= kTriggerThreshold)
            {
                state_.gamepad_triggers[slot] |= bit;
            }
            else
            {
                state_.gamepad_triggers[slot] &= static_cast<std::uint8_t>(~bit);
            }
            return Recompute();
        }

        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
        {
            const int slot = joystick_slots_.Find(event.jbutton.which);
            if (slot >= 0 && event.jbutton.button < input::kMaxJoystickButtons)
            {
                const std::uint32_t bit = 1U << event.jbutton.button;
                if (event.jbutton.down)
                {
                    state_.joystick_buttons[slot] |= bit;
                }
                else
                {
                    state_.joystick_buttons[slot] &= ~bit;
                }
            }
            return Recompute();
        }
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        {
            const int slot = joystick_slots_.Find(event.jhat.which);
            if (slot >= 0 && event.jhat.hat < input::kMaxJoystickHats)
            {
                state_.joystick_hats[slot][event.jhat.hat] = event.jhat.value;
            }
            return Recompute();
        }
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        case SDL_EVENT_JOYSTICK_BALL_MOTION:
        case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
        case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
        case SDL_EVENT_GAMEPAD_REMAPPED:
        case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
        case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        {
            Change ignored;
            ignored.handled = true;
            ignored.before = mask_;
            ignored.after = mask_;
            return ignored;
        }
        default:
            break;
    }
    return Change{};
}

}  // namespace repiu::engine
