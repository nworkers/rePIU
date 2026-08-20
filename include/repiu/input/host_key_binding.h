#ifndef REPIU_INPUT_HOST_KEY_BINDING_H_
#define REPIU_INPUT_HOST_KEY_BINDING_H_

#include <SDL3/SDL_keycode.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::input
{

// The modifier bits a configuration is allowed to speak about.
//
// SDL_Keymod also carries lock state -- SDL_KMOD_NUM, SDL_KMOD_CAPS,
// SDL_KMOD_SCROLL. Those must never reach a match test: including NumLock in a
// forbidden mask would break every binding the moment the key is toggled, and
// the P2 defaults specifically assume a NumLock-off keypad. Every modifier
// value is masked with this before it is compared.
constexpr SDL_Keymod kConfigurableModifierMask =
    static_cast<SDL_Keymod>(SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT);

constexpr std::uint32_t kMaxModifiersPerAlias = 3;

// One host key, optionally qualified by modifiers: "Q", "Ctrl+F1",
// "Ctrl+Shift+F2".
//
// Matching reduces to mask comparison because the parser resolves the rules at
// load time. `required` holds one entry per written modifier and each is
// satisfied when ANY of its bits is down, which is what makes side-agnostic
// "Ctrl" mean either control key. `forbidden` is satisfied when ALL of its
// bits are up.
struct HostKeyAlias
{
    SDL_Keycode keycode = SDLK_UNKNOWN;
    // Filled in by the platform layer at load time; 0 until then. Kept here so
    // the scan path reads an already-translated value and never converts.
    int virtual_key = 0;
    SDL_Keymod required[kMaxModifiersPerAlias] = {};
    std::uint32_t required_count = 0;
    SDL_Keymod forbidden = SDL_KMOD_NONE;

    bool has_modifiers() const
    {
        return required_count != 0;
    }

    bool ModifiersMatch(SDL_Keymod state) const
    {
        const auto masked =
            static_cast<SDL_Keymod>(state & kConfigurableModifierMask);
        for (std::uint32_t index = 0; index < required_count; ++index)
        {
            if ((masked & required[index]) == 0)
            {
                return false;
            }
        }
        return (masked & forbidden) == 0;
    }
};

// Parses one alias, i.e. one comma-separated element of a config value.
// Modifiers are joined with '+' and the base key comes last. Returns false and
// fills `error` when the text is not a valid alias.
bool ParseHostKeyAlias(std::string_view text, HostKeyAlias* alias,
                       std::string* error);

// Parses a full config value: aliases separated by commas. An empty or
// whitespace-only value yields no aliases, which is how an input is turned
// off. Unparsable aliases are appended to `errors` and skipped so one typo
// cannot discard the rest of the line.
std::vector<HostKeyAlias> ParseHostKeyBinding(std::string_view text,
                                              std::vector<std::string>* errors);

// Renders an alias back into its config-file spelling, e.g. "Ctrl+F1". Returns
// an empty string when the keycode is not in the name table.
std::string FormatHostKeyAlias(const HostKeyAlias& alias);

// Applies the contention rule across a set of aliases that share one config
// file: an alias with no modifiers normally ignores modifier state entirely,
// preserving today's behavior, but gains a forbidden mask when some other
// alias qualifies the SAME base key. That is what keeps "TEST = Ctrl+F1" and
// "CLEAR = F1" from both firing on Ctrl+F1 without making a plain "Q" stop
// working while Ctrl happens to be held.
void ApplyModifierContention(HostKeyAlias** aliases, std::uint32_t count);

}  // namespace repiu::input

#endif  // REPIU_INPUT_HOST_KEY_BINDING_H_
