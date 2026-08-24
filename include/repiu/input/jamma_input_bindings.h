#ifndef REPIU_INPUT_JAMMA_INPUT_BINDINGS_H_
#define REPIU_INPUT_JAMMA_INPUT_BINDINGS_H_

#include "repiu/config/ini_document.h"
#include "repiu/input/host_key_binding.h"
#include "repiu/input/jamma_input_key.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::input
{

// The config file section these bindings are read from.
constexpr std::string_view kInputSectionName = "Input";

constexpr std::uint32_t kMaxAliasesPerInput = 4;

// Port, bit, logical input, and log name in one table.
//
// These used to be two structures in port_io_emulator.cpp -- a bit-name table
// for edge logging and the bit manipulation in the scan code -- which is how
// the log came to call bit 0x02 "P1-UpRight" while the scan called it
// kP1Down. One table makes that disagreement impossible.
struct JammaPortBit
{
    std::uint16_t port;
    std::uint8_t mask;
    JammaInputKey key;
    const char* log_name;
};

const JammaPortBit* JammaPortBitTable(std::uint32_t* count);

struct JammaInputBinding
{
    HostKeyAlias aliases[kMaxAliasesPerInput];
    std::uint32_t alias_count = 0;
};

// The resolved configuration the input paths read. Fixed storage with no
// strings and no allocation: the guest polls these bindings inside the port
// I/O trap, which Task 403 measured as the dominant cost of that handler.
struct ResolvedJammaBindings
{
    JammaInputBinding inputs[kJammaInputKeyCount];

    // False for the default configuration, and the reason the modifier query
    // stays free: when nothing needs modifiers the scan path never reads them,
    // so the host key call count is exactly what it is today.
    bool any_binding_uses_modifiers = false;

    const JammaInputBinding& Get(JammaInputKey key) const
    {
        return inputs[static_cast<std::uint32_t>(key)];
    }
};

// The config-file text of an input's built-in default, e.g. "Keypad7, Home".
// The defaults are stored as text and parsed through the same path a file
// takes, so a default can never mean something the equivalent file would not.
std::string_view DefaultJammaBindingText(JammaInputKey key);

// Built-in defaults, already finalized.
ResolvedJammaBindings DefaultJammaBindings();

// Overlays the `[Input]` section onto `bindings`, replacing an input's alias
// list whenever the file names it.
//
// An empty value is applied, not skipped: it leaves the input with no key, so
// the guest never sees it pressed. That is how a config file turns an input
// off. Unknown entry names and unparsable key names are appended to `warnings`
// and skipped.
//
// Does not finalize; call FinalizeJammaBindings once after the last layer.
void ApplyJammaInputSection(const config::IniDocument& document,
                            ResolvedJammaBindings* bindings,
                            std::vector<std::string>* warnings);

// Resolves cross-alias state after every layer has been applied: the modifier
// contention rule and the any_binding_uses_modifiers flag.
void FinalizeJammaBindings(ResolvedJammaBindings* bindings);

// Fills HostKeyAlias::scancode for every alias from its keycode, against the
// keyboard layout in force now.
//
// Separate from FinalizeJammaBindings because the two answer to different
// events: finalizing follows a configuration layer, while this follows a layout
// change and has to be repeated when SDL reports one. The scan path must never
// do this conversion itself -- Task 403 measured the host key query as 99.21%
// of the port I/O handler body, and a per-read lookup would put the cost back.
void ResolveJammaHostScancodes(ResolvedJammaBindings* bindings);

// Renders an input's current binding as config-file text. Returns an empty
// string when the input has no key bound, which round trips: reading that back
// leaves the input off again.
std::string FormatJammaBinding(const ResolvedJammaBindings& bindings,
                               JammaInputKey key);

}  // namespace repiu::input

#endif  // REPIU_INPUT_JAMMA_INPUT_BINDINGS_H_
