#include "sdl_bios_keyboard_adapter.h"

#include <cstddef>
#include <cstdint>

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint16_t MakeAx(std::uint8_t scan_code, std::uint8_t ascii)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(scan_code) << 8U) | ascii);
}

bool IsShifted(SDL_Keymod modifiers)
{
    return (modifiers & SDL_KMOD_SHIFT) != 0;
}

std::uint8_t LetterAscii(SDL_Scancode scancode, SDL_Keymod modifiers)
{
    const bool upper = IsShifted(modifiers) !=
        ((modifiers & SDL_KMOD_CAPS) != 0);
    const std::uint8_t lower = static_cast<std::uint8_t>(
        'a' + (scancode - SDL_SCANCODE_A));
    if ((modifiers & SDL_KMOD_CTRL) != 0)
    {
        return static_cast<std::uint8_t>(lower - 'a' + 1U);
    }
    if ((modifiers & SDL_KMOD_ALT) != 0)
    {
        return 0U;
    }
    return upper ? static_cast<std::uint8_t>(lower - 'a' + 'A') : lower;
}

std::uint16_t ShiftFlagsFromModifiers(SDL_Keymod modifiers)
{
    std::uint16_t flags = 0;
    if ((modifiers & SDL_KMOD_RSHIFT) != 0) flags |= 0x0001U;
    if ((modifiers & SDL_KMOD_LSHIFT) != 0) flags |= 0x0002U;
    if ((modifiers & SDL_KMOD_CTRL) != 0) flags |= 0x0004U;
    if ((modifiers & SDL_KMOD_ALT) != 0) flags |= 0x0008U;
    if ((modifiers & SDL_KMOD_SCROLL) != 0) flags |= 0x0010U;
    if ((modifiers & SDL_KMOD_NUM) != 0) flags |= 0x0020U;
    if ((modifiers & SDL_KMOD_CAPS) != 0) flags |= 0x0040U;
    if ((modifiers & SDL_KMOD_LCTRL) != 0) flags |= 0x0100U;
    if ((modifiers & SDL_KMOD_LALT) != 0) flags |= 0x0200U;
    return flags;
}

bool TranslatePunctuation(SDL_Scancode scancode,
                          SDL_Keymod modifiers,
                          std::uint8_t* scan_code,
                          std::uint8_t* ascii)
{
    if (scan_code == nullptr || ascii == nullptr)
    {
        return false;
    }
    const bool shifted = IsShifted(modifiers);
    switch (scancode)
    {
        case SDL_SCANCODE_MINUS: *scan_code = 0x0CU; *ascii = shifted ? '_' : '-'; break;
        case SDL_SCANCODE_EQUALS: *scan_code = 0x0DU; *ascii = shifted ? '+' : '='; break;
        case SDL_SCANCODE_LEFTBRACKET: *scan_code = 0x1AU; *ascii = shifted ? '{' : '['; break;
        case SDL_SCANCODE_RIGHTBRACKET: *scan_code = 0x1BU; *ascii = shifted ? '}' : ']'; break;
        case SDL_SCANCODE_BACKSLASH: *scan_code = 0x2BU; *ascii = shifted ? '|' : '\\'; break;
        case SDL_SCANCODE_SEMICOLON: *scan_code = 0x27U; *ascii = shifted ? ':' : ';'; break;
        case SDL_SCANCODE_APOSTROPHE: *scan_code = 0x28U; *ascii = shifted ? '"' : '\''; break;
        case SDL_SCANCODE_GRAVE: *scan_code = 0x29U; *ascii = shifted ? '~' : '`'; break;
        case SDL_SCANCODE_COMMA: *scan_code = 0x33U; *ascii = shifted ? '<' : ','; break;
        case SDL_SCANCODE_PERIOD: *scan_code = 0x34U; *ascii = shifted ? '>' : '.'; break;
        case SDL_SCANCODE_SLASH: *scan_code = 0x35U; *ascii = shifted ? '?' : '/'; break;
        default: return false;
    }
    if ((modifiers & SDL_KMOD_ALT) != 0)
    {
        *ascii = 0U;
    }
    else if ((modifiers & SDL_KMOD_CTRL) != 0)
    {
        switch (scancode)
        {
            case SDL_SCANCODE_LEFTBRACKET: *ascii = 0x1BU; break;
            case SDL_SCANCODE_BACKSLASH: *ascii = 0x1CU; break;
            case SDL_SCANCODE_RIGHTBRACKET: *ascii = 0x1DU; break;
            case SDL_SCANCODE_MINUS: *ascii = 0x1FU; break;
            default: break;
        }
    }
    return true;
}

}  // namespace

bool TranslateSdlBiosKeystroke(SDL_Scancode scancode,
                               SDL_Keymod modifiers,
                               hle::BiosKeystroke* keystroke)
{
    if (keystroke == nullptr)
    {
        return false;
    }
    std::uint8_t scan_code = 0;
    std::uint8_t ascii = 0;
    bool enhanced = false;

    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
    {
        static constexpr std::uint8_t kLetterScanCodes[] = {
            0x1EU, 0x30U, 0x2EU, 0x20U, 0x12U, 0x21U, 0x22U, 0x23U, 0x17U,
            0x24U, 0x25U, 0x26U, 0x32U, 0x31U, 0x18U, 0x19U, 0x10U, 0x13U,
            0x1FU, 0x14U, 0x16U, 0x2FU, 0x11U, 0x2DU, 0x15U, 0x2CU};
        scan_code = kLetterScanCodes[scancode - SDL_SCANCODE_A];
        ascii = LetterAscii(scancode, modifiers);
    }
    else if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0)
    {
        static constexpr char kPlain[] = "1234567890";
        static constexpr char kShifted[] = "!@#$%^&*()";
        const std::size_t index = static_cast<std::size_t>(
            scancode - SDL_SCANCODE_1);
        scan_code = static_cast<std::uint8_t>(0x02U + index);
        ascii = static_cast<std::uint8_t>(IsShifted(modifiers)
            ? kShifted[index] : kPlain[index]);
        if ((modifiers & SDL_KMOD_ALT) != 0)
        {
            scan_code = static_cast<std::uint8_t>(0x78U + index);
            ascii = 0U;
        }
    }
    else if (!TranslatePunctuation(
                 scancode, modifiers, &scan_code, &ascii))
    {
        switch (scancode)
        {
            case SDL_SCANCODE_ESCAPE: scan_code = 0x01U; ascii = 0x1BU; break;
            case SDL_SCANCODE_BACKSPACE: scan_code = 0x0EU; ascii = 0x08U; break;
            case SDL_SCANCODE_TAB: scan_code = 0x0FU; ascii = 0x09U; break;
            case SDL_SCANCODE_RETURN: scan_code = 0x1CU; ascii = 0x0DU; break;
            case SDL_SCANCODE_SPACE: scan_code = 0x39U; ascii = 0x20U; break;
            case SDL_SCANCODE_F1: scan_code = 0x3BU; break;
            case SDL_SCANCODE_F2: scan_code = 0x3CU; break;
            case SDL_SCANCODE_F3: scan_code = 0x3DU; break;
            case SDL_SCANCODE_F4: scan_code = 0x3EU; break;
            case SDL_SCANCODE_F5: scan_code = 0x3FU; break;
            case SDL_SCANCODE_F6: scan_code = 0x40U; break;
            case SDL_SCANCODE_F7: scan_code = 0x41U; break;
            case SDL_SCANCODE_F8: scan_code = 0x42U; break;
            case SDL_SCANCODE_F9: scan_code = 0x43U; break;
            case SDL_SCANCODE_F10: scan_code = 0x44U; break;
            case SDL_SCANCODE_F11: scan_code = 0x85U; break;
            case SDL_SCANCODE_F12: scan_code = 0x86U; break;
            case SDL_SCANCODE_HOME: scan_code = 0x47U; enhanced = true; break;
            case SDL_SCANCODE_UP: scan_code = 0x48U; enhanced = true; break;
            case SDL_SCANCODE_PAGEUP: scan_code = 0x49U; enhanced = true; break;
            case SDL_SCANCODE_LEFT: scan_code = 0x4BU; enhanced = true; break;
            case SDL_SCANCODE_RIGHT: scan_code = 0x4DU; enhanced = true; break;
            case SDL_SCANCODE_END: scan_code = 0x4FU; enhanced = true; break;
            case SDL_SCANCODE_DOWN: scan_code = 0x50U; enhanced = true; break;
            case SDL_SCANCODE_PAGEDOWN: scan_code = 0x51U; enhanced = true; break;
            case SDL_SCANCODE_INSERT: scan_code = 0x52U; enhanced = true; break;
            case SDL_SCANCODE_DELETE: scan_code = 0x53U; ascii = 0x7FU; enhanced = true; break;
            case SDL_SCANCODE_KP_ENTER: scan_code = 0x1CU; ascii = 0x0DU; enhanced = true; break;
            case SDL_SCANCODE_KP_DIVIDE: scan_code = 0x35U; ascii = '/'; enhanced = true; break;
            case SDL_SCANCODE_KP_MULTIPLY: scan_code = 0x37U; ascii = '*'; break;
            case SDL_SCANCODE_KP_MINUS: scan_code = 0x4AU; ascii = '-'; break;
            case SDL_SCANCODE_KP_PLUS: scan_code = 0x4EU; ascii = '+'; break;
            case SDL_SCANCODE_KP_7: scan_code = 0x47U; ascii = '7'; break;
            case SDL_SCANCODE_KP_8: scan_code = 0x48U; ascii = '8'; break;
            case SDL_SCANCODE_KP_9: scan_code = 0x49U; ascii = '9'; break;
            case SDL_SCANCODE_KP_4: scan_code = 0x4BU; ascii = '4'; break;
            case SDL_SCANCODE_KP_5: scan_code = 0x4CU; ascii = '5'; break;
            case SDL_SCANCODE_KP_6: scan_code = 0x4DU; ascii = '6'; break;
            case SDL_SCANCODE_KP_1: scan_code = 0x4FU; ascii = '1'; break;
            case SDL_SCANCODE_KP_2: scan_code = 0x50U; ascii = '2'; break;
            case SDL_SCANCODE_KP_3: scan_code = 0x51U; ascii = '3'; break;
            case SDL_SCANCODE_KP_0: scan_code = 0x52U; ascii = '0'; break;
            case SDL_SCANCODE_KP_PERIOD: scan_code = 0x53U; ascii = '.'; break;
            default: return false;
        }
    }

    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F10)
    {
        const std::uint8_t index = static_cast<std::uint8_t>(
            scancode - SDL_SCANCODE_F1);
        if ((modifiers & SDL_KMOD_ALT) != 0)
        {
            scan_code = static_cast<std::uint8_t>(0x68U + index);
        }
        else if ((modifiers & SDL_KMOD_CTRL) != 0)
        {
            scan_code = static_cast<std::uint8_t>(0x5EU + index);
        }
        else if (IsShifted(modifiers))
        {
            scan_code = static_cast<std::uint8_t>(0x54U + index);
        }
    }
    else if (scancode == SDL_SCANCODE_F11 || scancode == SDL_SCANCODE_F12)
    {
        const std::uint8_t index = scancode == SDL_SCANCODE_F11 ? 0U : 1U;
        if ((modifiers & SDL_KMOD_ALT) != 0)
        {
            scan_code = static_cast<std::uint8_t>(0x8BU + index);
        }
        else if ((modifiers & SDL_KMOD_CTRL) != 0)
        {
            scan_code = static_cast<std::uint8_t>(0x89U + index);
        }
        else if (IsShifted(modifiers))
        {
            scan_code = static_cast<std::uint8_t>(0x87U + index);
        }
    }

    if ((scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0) ||
        scancode == SDL_SCANCODE_KP_PERIOD)
    {
        if ((modifiers & SDL_KMOD_NUM) == 0)
        {
            ascii = 0U;
        }
    }

    if ((modifiers & SDL_KMOD_ALT) != 0 && !enhanced)
    {
        ascii = 0U;
    }
    keystroke->legacy_ax = MakeAx(scan_code, enhanced ? 0U : ascii);
    keystroke->enhanced_ax = MakeAx(scan_code, enhanced ? 0xE0U : ascii);
    return true;
}

void HandleSdlBiosKeyboardEvent(const SDL_KeyboardEvent& event,
                                hle::BiosKeyboard* keyboard)
{
    if (keyboard == nullptr)
    {
        return;
    }
    constexpr std::uint16_t kModifierAndToggleMask = 0x03FFU;
    const std::uint16_t preserved_pressed_lock_flags =
        keyboard->shift_flags() & 0x7000U;
    keyboard->SetShiftFlags(
        preserved_pressed_lock_flags |
        (ShiftFlagsFromModifiers(event.mod) & kModifierAndToggleMask));

    const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
    switch (event.scancode)
    {
        case SDL_SCANCODE_SCROLLLOCK: keyboard->UpdateShiftFlags(0x1000U, pressed); break;
        case SDL_SCANCODE_NUMLOCKCLEAR: keyboard->UpdateShiftFlags(0x2000U, pressed); break;
        case SDL_SCANCODE_CAPSLOCK: keyboard->UpdateShiftFlags(0x4000U, pressed); break;
        default: break;
    }
    if (!pressed)
    {
        return;
    }
    hle::BiosKeystroke keystroke;
    if (TranslateSdlBiosKeystroke(event.scancode, event.mod, &keystroke))
    {
        keyboard->Push(keystroke);
    }
}

void HandleSdlBiosKeyboardFocusLost(hle::BiosKeyboard* keyboard)
{
    if (keyboard != nullptr)
    {
        keyboard->ReleasePressedModifiers();
    }
}

}  // namespace repiu::platform::win32
