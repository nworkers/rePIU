#include "bios_keyboard_input_probe.h"

#include "repiu/hle/bios_keyboard.h"
#include "sdl_bios_keyboard_adapter.h"

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

SDL_KeyboardEvent MakeKeyEvent(SDL_EventType type,
                               SDL_Scancode scancode,
                               SDL_Keymod modifiers,
                               bool repeat = false)
{
    SDL_KeyboardEvent event{};
    event.type = type;
    event.scancode = scancode;
    event.mod = modifiers;
    event.repeat = repeat;
    return event;
}

bool Translates(SDL_Scancode scancode,
                SDL_Keymod modifiers,
                std::uint16_t legacy_ax,
                std::uint16_t enhanced_ax)
{
    hle::BiosKeystroke keystroke;
    return platform::win32::TranslateSdlBiosKeystroke(
               scancode, modifiers, &keystroke) &&
        keystroke.legacy_ax == legacy_ax &&
        keystroke.enhanced_ax == enhanced_ax;
}

}  // namespace

bool RunBiosKeyboardInputProbe()
{
    const bool translation_valid =
        Translates(SDL_SCANCODE_A, SDL_KMOD_NONE, 0x1E61U, 0x1E61U) &&
        Translates(SDL_SCANCODE_A, SDL_KMOD_SHIFT, 0x1E41U, 0x1E41U) &&
        Translates(SDL_SCANCODE_A, SDL_KMOD_CAPS, 0x1E41U, 0x1E41U) &&
        Translates(SDL_SCANCODE_A, SDL_KMOD_CTRL, 0x1E01U, 0x1E01U) &&
        Translates(SDL_SCANCODE_1, SDL_KMOD_SHIFT, 0x0221U, 0x0221U) &&
        Translates(SDL_SCANCODE_1, SDL_KMOD_ALT, 0x7800U, 0x7800U) &&
        Translates(SDL_SCANCODE_F5, SDL_KMOD_NONE, 0x3F00U, 0x3F00U) &&
        Translates(SDL_SCANCODE_F5, SDL_KMOD_SHIFT, 0x5800U, 0x5800U) &&
        Translates(SDL_SCANCODE_UP, SDL_KMOD_NONE, 0x4800U, 0x48E0U) &&
        Translates(SDL_SCANCODE_KP_ENTER, SDL_KMOD_NONE, 0x1C00U, 0x1CE0U) &&
        Translates(SDL_SCANCODE_KP_1, SDL_KMOD_NONE, 0x4F00U, 0x4F00U) &&
        Translates(SDL_SCANCODE_KP_1, SDL_KMOD_NUM, 0x4F31U, 0x4F31U);

    hle::BiosKeyboard keyboard;
    platform::win32::HandleSdlBiosKeyboardEvent(
        MakeKeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A, SDL_KMOD_LSHIFT),
        &keyboard);
    platform::win32::HandleSdlBiosKeyboardEvent(
        MakeKeyEvent(
            SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A, SDL_KMOD_LSHIFT, true),
        &keyboard);
    std::uint16_t first_peek = 0;
    std::uint16_t first_read = 0;
    std::uint16_t second_read = 0;
    const bool fifo_valid =
        keyboard.Peek(true, &first_peek) && first_peek == 0x1E41U &&
        keyboard.Pop(true, &first_read) && first_read == first_peek &&
        keyboard.Pop(false, &second_read) && second_read == 0x1E41U &&
        !keyboard.Peek(false, &first_peek);

    platform::win32::HandleSdlBiosKeyboardEvent(
        MakeKeyEvent(
            SDL_EVENT_KEY_DOWN, SDL_SCANCODE_CAPSLOCK,
            static_cast<SDL_Keymod>(SDL_KMOD_LSHIFT | SDL_KMOD_CAPS)),
        &keyboard);
    const std::uint16_t flags_before_focus = keyboard.shift_flags();
    platform::win32::HandleSdlBiosKeyboardFocusLost(&keyboard);
    const std::uint16_t flags_after_focus = keyboard.shift_flags();
    const bool modifier_valid =
        (flags_before_focus & 0x0042U) == 0x0042U &&
        (flags_before_focus & 0x4000U) != 0U &&
        flags_after_focus == 0x0040U;

    hle::BiosKeyboard overflow_keyboard;
    hle::BiosKeystroke numbered;
    bool accepted_capacity = true;
    for (std::size_t index = 0; index < hle::BiosKeyboard::kBufferCapacity; ++index)
    {
        numbered.legacy_ax = static_cast<std::uint16_t>(index + 1U);
        numbered.enhanced_ax = numbered.legacy_ax;
        accepted_capacity = accepted_capacity && overflow_keyboard.Push(numbered);
    }
    const bool rejected_overflow = !overflow_keyboard.Push(numbered);
    const hle::BiosKeyboardSnapshot snapshot = overflow_keyboard.Snapshot();
    const bool overflow_valid = accepted_capacity && rejected_overflow &&
        snapshot.queued_count == hle::BiosKeyboard::kBufferCapacity &&
        snapshot.accepted_count == hle::BiosKeyboard::kBufferCapacity &&
        snapshot.overflow_count == 1U;

    const bool valid = translation_valid && fifo_valid && modifier_valid &&
        overflow_valid;
    std::cout << "bios_keyboard_input_probe=" << (valid ? "true" : "false")
              << ",translated=" << (translation_valid ? "true" : "false")
              << ",fifo=" << (fifo_valid ? "true" : "false")
              << ",modifiers=" << (modifier_valid ? "true" : "false")
              << ",overflow=" << snapshot.overflow_count << "\n";
    return valid;
}

}  // namespace repiu::tools
