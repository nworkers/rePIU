#ifndef REPIU_INPUT_HOST_KEY_NAMES_H_
#define REPIU_INPUT_HOST_KEY_NAMES_H_

#include <SDL3/SDL_keycode.h>

#include <cstdint>
#include <string_view>

namespace repiu::input
{

// Which line of the generated config file's comment block a key belongs to.
// Presentation only; it has no effect on how a name resolves.
enum class HostKeyGroup : std::uint8_t
{
    kLetter,
    kDigit,
    kFunction,
    kKeypad,
    kNavigation,
    kOther,
    kModifier,
    kCount,
};

struct HostKeyName
{
    std::string_view name;
    SDL_Keycode keycode;
    HostKeyGroup group;
};

// The project's own name table, and the single source of the configurable key
// names. Both the user guide and the comment block of a generated config file
// are derived from it.
//
// SDL's own names are deliberately not used. SDL_GetScancodeName documents
// that its names are "unsuitable for creating a stable cross-platform two-way
// mapping between strings and scancodes", which is exactly what a config file
// needs; the names also contain spaces ("Keypad 7", "Page Up") and both
// SDL_GetKeyName and SDL_GetScancodeName are documented as not thread safe.
const HostKeyName* HostKeyNameTable(std::uint32_t* count);

// Resolves a config-file spelling to its keycode. Comparison ignores case and
// underscores. Returns false when the name is not in the table.
bool FindHostKeyByName(std::string_view name, SDL_Keycode* keycode);

// The table spelling of a keycode, or an empty view when it is not in the
// table. Used to render a binding back into config-file text.
std::string_view FindHostKeyName(SDL_Keycode keycode);

}  // namespace repiu::input

#endif  // REPIU_INPUT_HOST_KEY_NAMES_H_
