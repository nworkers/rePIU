#ifndef REPIU_INPUT_HOST_PAD_BINDING_H_
#define REPIU_INPUT_HOST_PAD_BINDING_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace repiu::input
{

// Issue #34. Gamepad and joystick controls a config file can bind next to host
// keys. Two name spaces, because two kinds of device turn up:
//
//   Pad<N>_<Button>        a device SDL maps to its standard gamepad layout,
//                          named by position (A is the bottom face button).
//   Joy<N>_Button<K>       any joystick by raw number, which is how a dance
//   Joy<N>_Hat<H><Dir>     pad or another device without a standard mapping
//                          reports itself.
//
// N is the device's number in connection order, K and H count from one. The
// parsed form is plain data so the state test below needs no SDL device.

constexpr std::uint32_t kMaxGamepadSlots = 4;
constexpr std::uint32_t kMaxJoystickSlots = 8;
constexpr std::uint32_t kMaxJoystickButtons = 32;
constexpr std::uint32_t kMaxJoystickHats = 4;

enum class HostPadControlKind : std::uint8_t
{
    kNone,
    // `control` is an SDL_GamepadButton.
    kGamepadButton,
    // `control` is 0 for the left trigger and 1 for the right one.
    kGamepadTrigger,
    // `control` is the zero-based joystick button index.
    kJoystickButton,
    // `control` is the zero-based hat index and `hat_direction` one SDL_HAT_*
    // direction bit.
    kJoystickHat,
};

struct HostPadAlias
{
    HostPadControlKind kind = HostPadControlKind::kNone;
    // Zero-based: "Pad1" is slot 0.
    std::uint8_t slot = 0;
    std::uint8_t control = 0;
    std::uint8_t hat_direction = 0;
};

// True when the text names a pad control rather than a host key: it starts
// with "Pad" or "Joy" followed by a digit, ignoring case and underscores. No
// host key name has that shape, so a value item is routed by this alone.
bool LooksLikeHostPadAlias(std::string_view text);

// Parses one pad name. Returns false and fills `error` when the text is not a
// valid name or a number is out of range.
bool ParseHostPadAlias(std::string_view text, HostPadAlias* alias,
                       std::string* error);

// The config-file spelling, e.g. "Pad1_DpadUp", "Joy2_Button5", "Joy1_Hat1Up".
std::string FormatHostPadAlias(const HostPadAlias& alias);

// The gamepad button names in the order the generated config file lists them.
struct HostPadButtonName
{
    std::string_view name;
    std::uint8_t button;
};
const HostPadButtonName* HostPadButtonNameTable(std::uint32_t* count);

}  // namespace repiu::input

#endif  // REPIU_INPUT_HOST_PAD_BINDING_H_
