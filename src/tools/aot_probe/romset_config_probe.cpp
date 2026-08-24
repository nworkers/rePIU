#include "romset_config_probe.h"

#include "repiu/config/config_name.h"
#include "repiu/config/ini_document.h"
#include "repiu/config/romset_config.h"
#include "repiu/config/romset_config_template.h"
#include "repiu/input/host_key_binding.h"
#include "repiu/input/host_key_names.h"
#include "repiu/input/jamma_input_bindings.h"
#include "repiu/platform/win32/active_jamma_bindings.h"
#include <SDL3/SDL_keyboard.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace repiu::tools
{
namespace
{

using config::IniDocument;
using input::HostKeyAlias;
using input::HostKeyName;
using input::JammaInputKey;
using input::ResolvedJammaBindings;

std::uint32_t g_failure_count = 0;
std::uint32_t g_check_count = 0;

void Check(bool condition, std::string_view label)
{
    // Counted, not just failed. A probe that silently stops running its
    // assertions reports success exactly like one that passes them.
    ++g_check_count;
    if (condition)
    {
        return;
    }
    ++g_failure_count;
    std::cout << "[romset-config] FAIL " << label << "\n";
}

ResolvedJammaBindings BindingsFromText(std::string_view text)
{
    ResolvedJammaBindings bindings = input::DefaultJammaBindings();
    std::vector<std::string> warnings;
    const IniDocument document = IniDocument::Parse(text, "probe");
    input::ApplyJammaInputSection(document, &bindings, &warnings);
    input::FinalizeJammaBindings(&bindings);
    return bindings;
}

const HostKeyAlias* FirstAlias(const ResolvedJammaBindings& bindings,
                               JammaInputKey key)
{
    const input::JammaInputBinding& binding = bindings.Get(key);
    return binding.alias_count == 0 ? nullptr : &binding.aliases[0];
}

// 1. INI parsing: comments, whitespace, quotes, case, duplicate keys.
void ProbeIniParsing()
{
    const std::string text =
        "; leading comment\r\n"
        "# another comment\r\n"
        "  [ InPuT ]  \r\n"
        "  P1_UP_LEFT   =   'W'   \r\n"
        "p1upleft = \"R\"\r\n"
        "\r\n"
        "P1_CENTER =\r\n"
        "this line has no separator\r\n"
        "[Unterminated\r\n";

    const IniDocument document = IniDocument::Parse(text, "probe.ini");
    Check(document.entries().size() == 3, "ini entry count");
    Check(document.warnings().size() == 2, "ini warning count");

    const std::string* last = document.FindLast("Input", "P1_UP_LEFT");
    Check(last != nullptr && *last == "R", "duplicate key: last one wins");

    const std::string* center = document.FindLast("input", "p1_center");
    Check(center != nullptr && center->empty(), "empty value is preserved");

    // The section header had spaces inside the brackets and different case.
    Check(document.FindLast("INPUT", "P1UpLeft") != nullptr,
          "section and key comparison ignores case and underscores");
}

// 2. Alias lists, turning an input off, and invalid names.
void ProbeBindingApplication()
{
    const ResolvedJammaBindings aliases = BindingsFromText(
        "[Input]\nP1_UP_LEFT = W, Keypad4\n");
    Check(aliases.Get(JammaInputKey::kP1UpLeft).alias_count == 2,
          "comma separated aliases");
    Check(input::FormatJammaBinding(aliases, JammaInputKey::kP1UpLeft) ==
              "W, Keypad4",
          "alias list round trips through formatting");

    // An empty value turns the input off. This is how a cabinet button such as
    // TEST is disabled, so it has to survive to the scan path as "no key at
    // all" rather than falling back to the default.
    const ResolvedJammaBindings empty_value =
        BindingsFromText("[Input]\nTEST =\nP2_UP_LEFT =   \n");
    Check(empty_value.Get(JammaInputKey::kTest).alias_count == 0,
          "an empty value turns the input off");
    Check(empty_value.Get(JammaInputKey::kP2UpLeft).alias_count == 0,
          "a whitespace-only value turns the input off");
    Check(empty_value.Get(JammaInputKey::kService).alias_count == 1,
          "turning one input off leaves the others alone");

    // A disabled input must also disappear from the polling path, which is
    // what the guest actually reads. An alias-free binding contributes no
    // scancode, so its port bit stays released.
    ResolvedJammaBindings resolved = empty_value;
    input::ResolveJammaHostScancodes(&resolved);
    Check(resolved.Get(JammaInputKey::kTest).alias_count == 0,
          "a disabled input resolves to no scancode");

    ResolvedJammaBindings bindings = input::DefaultJammaBindings();
    std::vector<std::string> warnings;
    const IniDocument document = IniDocument::Parse(
        "[Input]\nP1_CENTER = NoSuchKey\nNOT_AN_INPUT = Q\n", "probe");
    input::ApplyJammaInputSection(document, &bindings, &warnings);
    input::FinalizeJammaBindings(&bindings);
    Check(warnings.size() == 2, "invalid key name and unknown entry warn");
    Check(bindings.Get(JammaInputKey::kP1Center).alias_count == 0,
          "an entry naming only invalid keys leaves the input off, with a "
          "warning saying why");

    // A partly valid value still applies its usable half.
    ResolvedJammaBindings partial = input::DefaultJammaBindings();
    std::vector<std::string> partial_warnings;
    const IniDocument partial_document =
        IniDocument::Parse("[Input]\nP1_CENTER = M, NoSuchKey\n", "probe");
    input::ApplyJammaInputSection(partial_document, &partial,
                                  &partial_warnings);
    input::FinalizeJammaBindings(&partial);
    Check(partial_warnings.size() == 1, "the unusable half warns");
    Check(input::FormatJammaBinding(partial, JammaInputKey::kP1Center) == "M",
          "the usable half of a value still applies");

    // A section the build does not know is not an error; the format is meant
    // to grow and an older build must still run a newer file.
    ResolvedJammaBindings future = input::DefaultJammaBindings();
    std::vector<std::string> future_warnings;
    const IniDocument future_document =
        IniDocument::Parse("[Video]\nScale = 3\n", "probe");
    input::ApplyJammaInputSection(future_document, &future, &future_warnings);
    Check(future_warnings.empty(), "unknown section does not warn");
}

// 3. Name table health.
void ProbeNameTable()
{
    std::uint32_t count = 0;
    const HostKeyName* table = input::HostKeyNameTable(&count);
    Check(count > 0, "name table is not empty");

    bool duplicate_name = false;
    bool duplicate_keycode = false;
    bool missing_scancode = false;
    bool round_trip_failed = false;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        for (std::uint32_t other = index + 1; other < count; ++other)
        {
            if (config::EqualsConfigName(table[index].name,
                                         table[other].name))
            {
                duplicate_name = true;
            }
            if (table[index].keycode == table[other].keycode)
            {
                duplicate_keycode = true;
            }
        }

        // Every configurable key must reach the polling path. Without this a
        // key added to the table but missing a scancode would bind fine in the
        // window path and be silently dead in the guest.
        if (SDL_GetScancodeFromKey(table[index].keycode, nullptr) ==
            SDL_SCANCODE_UNKNOWN)
        {
            missing_scancode = true;
        }

        SDL_Keycode resolved = SDLK_UNKNOWN;
        if (!input::FindHostKeyByName(table[index].name, &resolved) ||
            resolved != table[index].keycode)
        {
            round_trip_failed = true;
        }
    }

    Check(!duplicate_name, "no duplicate key names");
    Check(!duplicate_keycode, "no duplicate keycodes");
    Check(!missing_scancode, "every key name resolves to a scancode");
    Check(!round_trip_failed, "every key name resolves to its own keycode");

    SDL_Keycode keycode = SDLK_UNKNOWN;
    Check(input::FindHostKeyByName("KEYPAD_7", &keycode) &&
              keycode == SDLK_KP_7,
          "name lookup ignores case and underscores");
    Check(!input::FindHostKeyByName("Keypad 7", &keycode),
          "SDL's own spelling with a space is not accepted");
}

// 4. Combination parsing, including the malformed shapes.
void ProbeCombinationParsing()
{
    struct Accepted
    {
        const char* text;
        SDL_Keycode keycode;
        std::uint32_t modifier_count;
    };
    constexpr Accepted kAccepted[] = {
        {"F1", SDLK_F1, 0},
        {"Ctrl+F1", SDLK_F1, 1},
        {"Ctrl+Shift+F2", SDLK_F2, 2},
        {"LeftAlt+F3", SDLK_F3, 1},
        {"  ctrl + f1  ", SDLK_F1, 1},
        {"LeftShift", SDLK_LSHIFT, 0},
    };
    for (const Accepted& entry : kAccepted)
    {
        HostKeyAlias alias;
        std::string error;
        const bool parsed = input::ParseHostKeyAlias(entry.text, &alias,
                                                     &error);
        std::ostringstream label;
        label << "parse \"" << entry.text << "\"";
        Check(parsed && alias.keycode == entry.keycode &&
                  alias.required_count == entry.modifier_count,
              label.str());
    }

    constexpr const char* kRejected[] = {
        "F1+Ctrl", "Ctrl+", "Ctrl+Ctrl+F1", "+F1", "", "Ctrl+NoSuchKey",
    };
    for (const char* text : kRejected)
    {
        HostKeyAlias alias;
        std::string error;
        std::ostringstream label;
        label << "reject \"" << text << "\"";
        Check(!input::ParseHostKeyAlias(text, &alias, &error) &&
                  !error.empty(),
              label.str());
    }

    Check(input::FormatHostKeyAlias(
              [] {
                  HostKeyAlias alias;
                  std::string error;
                  input::ParseHostKeyAlias("Ctrl+Shift+F2", &alias, &error);
                  return alias;
              }()) == "Ctrl+Shift+F2",
          "combination round trips through formatting");
}

// 5. Modifier matching, contention, and the lock-bit guard.
void ProbeModifierMatching()
{
    const ResolvedJammaBindings contended = BindingsFromText(
        "[Input]\nTEST = Ctrl+F1\nCLEAR = F1\n");

    const HostKeyAlias* test = FirstAlias(contended, JammaInputKey::kTest);
    const HostKeyAlias* clear = FirstAlias(contended, JammaInputKey::kClear);
    Check(test != nullptr && clear != nullptr, "contended bindings resolved");
    if (test == nullptr || clear == nullptr)
    {
        return;
    }

    Check(test->required_count == 1 && test->required[0] == SDL_KMOD_CTRL,
          "Ctrl is satisfied by either control key");
    Check(test->forbidden ==
              static_cast<SDL_Keymod>(SDL_KMOD_SHIFT | SDL_KMOD_ALT),
          "a qualified alias forbids every other modifier");
    Check(clear->forbidden == SDL_KMOD_CTRL,
          "the plain alias inherits only the contended modifier");

    Check(test->ModifiersMatch(SDL_KMOD_LCTRL), "Ctrl+F1 matches left ctrl");
    Check(test->ModifiersMatch(SDL_KMOD_RCTRL), "Ctrl+F1 matches right ctrl");
    Check(!test->ModifiersMatch(SDL_KMOD_NONE), "Ctrl+F1 needs ctrl");
    Check(!test->ModifiersMatch(static_cast<SDL_Keymod>(SDL_KMOD_LCTRL |
                                                        SDL_KMOD_LSHIFT)),
          "Ctrl+F1 does not fire on Ctrl+Shift+F1");
    Check(clear->ModifiersMatch(SDL_KMOD_NONE), "plain F1 matches unmodified");
    Check(!clear->ModifiersMatch(SDL_KMOD_LCTRL),
          "plain F1 yields to the contended Ctrl+F1");
    Check(clear->ModifiersMatch(SDL_KMOD_LSHIFT),
          "plain F1 still matches an uncontended modifier");

    // The regression this whole rule exists to avoid: with nothing contending,
    // a plain key must keep matching regardless of modifier state, exactly as
    // the hardcoded mapping did.
    const ResolvedJammaBindings defaults = input::DefaultJammaBindings();
    const HostKeyAlias* p1 = FirstAlias(defaults, JammaInputKey::kP1UpLeft);
    Check(p1 != nullptr && p1->forbidden == SDL_KMOD_NONE,
          "an uncontended plain alias forbids nothing");
    Check(p1 != nullptr && p1->ModifiersMatch(SDL_KMOD_LCTRL),
          "Ctrl+Q still presses P1 up-left");

    // Lock bits must never participate. The P2 defaults assume a NumLock-off
    // keypad, so a NumLock bit leaking into a forbidden mask would be
    // especially destructive.
    constexpr auto kLocks = static_cast<SDL_Keymod>(
        SDL_KMOD_NUM | SDL_KMOD_CAPS | SDL_KMOD_SCROLL);
    Check(clear->ModifiersMatch(kLocks), "lock bits do not block a plain key");
    Check(test->ModifiersMatch(
              static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | kLocks)),
          "lock bits do not block a combination");
    Check(defaults.any_binding_uses_modifiers == false,
          "the default configuration needs no modifier query");
    Check(contended.any_binding_uses_modifiers,
          "a combination sets the modifier query flag");
}

// 6/7. Layering, generation, and the no-config regression guard.
void ProbeLayeringAndGeneration(const std::filesystem::path& root)
{
    std::error_code error;
    std::filesystem::create_directories(root, error);

    auto write_file = [](const std::filesystem::path& path,
                         std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    };

    config::RomSetConfigRequest request;
    request.cfg_directory_override = root;
    request.write_default_when_missing = false;
    request.layer_ids = {"probeparent", "probechild"};

    // 7. With no file at all the result must equal the built-in defaults,
    // scancode for scancode. This is the guard that configuration did not
    // change what the emulator does out of the box.
    const config::RomSetConfigResult bare = config::LoadRomSetConfig(request);
    ResolvedJammaBindings expected = input::DefaultJammaBindings();
    input::ResolveJammaHostScancodes(&expected);
    ResolvedJammaBindings actual = bare.bindings;
    input::ResolveJammaHostScancodes(&actual);

    bool identical = true;
    for (std::uint32_t index = 0; index < input::kJammaInputKeyCount; ++index)
    {
        const input::JammaInputBinding& left = expected.inputs[index];
        const input::JammaInputBinding& right = actual.inputs[index];
        if (left.alias_count != right.alias_count)
        {
            identical = false;
            break;
        }
        for (std::uint32_t slot = 0; slot < left.alias_count; ++slot)
        {
            if (left.aliases[slot].scancode !=
                    right.aliases[slot].scancode ||
                left.aliases[slot].keycode != right.aliases[slot].keycode)
            {
                identical = false;
            }
        }
    }
    Check(identical, "no config file yields exactly the built-in defaults");
    Check(bare.applied_files.empty(), "no files reported as applied");
    Check(!bare.generated_default_file,
          "generation stays off when disabled");

    // 6. The parent supplies a value, the child overrides a different one, and
    // the untouched inputs keep their defaults.
    write_file(root / "probeparent.ini",
               "[Input]\nP1_CENTER = M\nTEST = F9\n");
    write_file(root / "probechild.ini", "[Input]\nTEST = F10\n");

    const config::RomSetConfigResult layered =
        config::LoadRomSetConfig(request);
    Check(layered.applied_files.size() == 2, "both layers applied");
    Check(input::FormatJammaBinding(layered.bindings,
                                    JammaInputKey::kP1Center) == "M",
          "a parent value survives when the child does not name it");
    Check(input::FormatJammaBinding(layered.bindings, JammaInputKey::kTest) ==
              "F10",
          "the child overrides the parent");
    Check(input::FormatJammaBinding(layered.bindings,
                                    JammaInputKey::kP1UpLeft) == "Q",
          "inputs no layer names keep the built-in default");

    // 9. Generation must not defeat layering. After the child file is created,
    // a parent value must still reach the resolved bindings -- which is only
    // true because the generated entries are commented out.
    std::filesystem::remove(root / "probechild.ini", error);
    config::RomSetConfigRequest generating = request;
    generating.write_default_when_missing = true;

    const config::RomSetConfigResult created =
        config::LoadRomSetConfig(generating);
    Check(created.generated_default_file, "missing child file is created");
    Check(created.generated_file == root / "probechild.ini",
          "the created file is the launched ROM set's");
    Check(std::filesystem::exists(root / "probechild.ini"),
          "the created file is on disk");
    Check(!std::filesystem::exists(root / "probeparent.ini.tmp") &&
              !std::filesystem::exists(root / "probechild.ini.tmp"),
          "no temporary file is left behind");

    const config::RomSetConfigResult after_generation =
        config::LoadRomSetConfig(generating);
    Check(!after_generation.generated_default_file,
          "an existing file is never overwritten");
    Check(input::FormatJammaBinding(after_generation.bindings,
                                    JammaInputKey::kP1Center) == "M",
          "a parent value still applies after the child file is generated");
    Check(input::FormatJammaBinding(after_generation.bindings,
                                    JammaInputKey::kTest) == "F9",
          "the generated child contributes no active entry");
}

// 8. The generated file's own shape.
void ProbeGeneratedTemplate()
{
    const ResolvedJammaBindings defaults = input::DefaultJammaBindings();
    const std::string text =
        config::RenderRomSetConfigTemplate("pumpit1", defaults);

    Check(text.find("[Input]\r\n") != std::string::npos,
          "the section header stays active and uses CRLF");
    Check(text.find("\r\n") != std::string::npos, "CRLF line endings");
    Check(text.find("\n;P1_UP_LEFT") != std::string::npos,
          "input entries are commented out");

    // Read the rendered file exactly as written: nothing may be active, which
    // is what makes generating the file unable to change behavior.
    ResolvedJammaBindings untouched = input::DefaultJammaBindings();
    std::vector<std::string> warnings;
    const IniDocument as_written = IniDocument::Parse(text, "generated");
    input::ApplyJammaInputSection(as_written, &untouched, &warnings);
    Check(as_written.entries().empty(),
          "the generated file contributes no entry as written");
    Check(as_written.warnings().empty(),
          "the generated file parses without warnings");

    // Uncomment every entry and it must reproduce exactly what was rendered.
    std::string uncommented;
    uncommented.reserve(text.size());
    std::size_t offset = 0;
    while (offset < text.size())
    {
        const std::size_t end = text.find('\n', offset);
        const std::size_t stop = end == std::string::npos ? text.size()
                                                          : end + 1;
        std::string line = text.substr(offset, stop - offset);
        offset = stop;
        // Only the generated entries look like ";NAME = value"; the prose
        // comments all have a space after the semicolon.
        if (line.size() > 1 && line[0] == ';' && line[1] != ' ' &&
            line.find('=') != std::string::npos)
        {
            line.erase(0, 1);
        }
        uncommented.append(line);
    }

    const ResolvedJammaBindings restored = BindingsFromText(uncommented);
    bool round_trip = true;
    for (std::uint32_t index = 0; index < input::kJammaInputKeyCount; ++index)
    {
        const auto key = static_cast<JammaInputKey>(index);
        if (input::FormatJammaBinding(restored, key) !=
            input::FormatJammaBinding(defaults, key))
        {
            round_trip = false;
        }
    }
    Check(round_trip, "uncommenting the generated file restores the bindings");

    // Drift guard: a key added to the name table must reach the comment block.
    // Single-character families are collapsed into a range, so those are
    // checked through their endpoints instead of by name.
    std::uint32_t count = 0;
    const HostKeyName* table = input::HostKeyNameTable(&count);
    bool every_name_documented = true;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (table[index].name.size() == 1)
        {
            continue;
        }
        if (text.find(std::string(table[index].name)) == std::string::npos)
        {
            every_name_documented = false;
            std::cout << "[romset-config] missing from comment block: "
                      << table[index].name << "\n";
        }
    }
    Check(every_name_documented,
          "every multi-character key name appears in the comment block");
    Check(text.find("A .. Z") != std::string::npos, "letters documented");
    Check(text.find("0 .. 9") != std::string::npos, "digits documented");
}

// The regression guard that actually matters.
//
// Comparing the resolved defaults against DefaultJammaBindings() only proves
// the loader is self-consistent. This transcribes the mapping as it was
// hardcoded in ScanJammaPort8 before configuration existed -- port, bit, and
// the exact set of host keys that drove it -- and asserts the default
// configuration still produces precisely that.
//
// Task 503d-13 restated the keys as scancodes. The keypad entries used to need
// their second alias because Windows reports a different virtual key for the
// same physical key while NumLock is off; a scancode names the physical key, so
// the first alias now covers both states and the second still covers the
// dedicated navigation key it also always covered.
void ProbeHistoricalDefaultMapping()
{
    struct HistoricalBit
    {
        std::uint16_t port;
        std::uint8_t mask;
        SDL_Scancode scancodes[2];
        std::uint32_t scancode_count;
    };
    constexpr HistoricalBit kHistorical[] = {
        {0x02A8, 0x01, {SDL_SCANCODE_Q, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A8, 0x02, {SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A8, 0x04, {SDL_SCANCODE_S, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A8, 0x08, {SDL_SCANCODE_Z, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A8, 0x10, {SDL_SCANCODE_C, SDL_SCANCODE_UNKNOWN}, 1},

        {0x02A9, 0x02, {SDL_SCANCODE_F1, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A9, 0x04, {SDL_SCANCODE_F5, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A9, 0x40, {SDL_SCANCODE_F2, SDL_SCANCODE_UNKNOWN}, 1},
        {0x02A9, 0x80, {SDL_SCANCODE_F3, SDL_SCANCODE_UNKNOWN}, 1},

        {0x02AA, 0x01, {SDL_SCANCODE_KP_7, SDL_SCANCODE_HOME}, 2},
        {0x02AA, 0x02, {SDL_SCANCODE_KP_9, SDL_SCANCODE_PAGEUP}, 2},
        {0x02AA, 0x04, {SDL_SCANCODE_KP_5, SDL_SCANCODE_CLEAR}, 2},
        {0x02AA, 0x08, {SDL_SCANCODE_KP_1, SDL_SCANCODE_END}, 2},
        {0x02AA, 0x10, {SDL_SCANCODE_KP_3, SDL_SCANCODE_PAGEDOWN}, 2},
    };

    ResolvedJammaBindings defaults = input::DefaultJammaBindings();
    input::ResolveJammaHostScancodes(&defaults);

    std::uint32_t bit_count = 0;
    const input::JammaPortBit* bits = input::JammaPortBitTable(&bit_count);
    Check(bit_count == sizeof(kHistorical) / sizeof(kHistorical[0]),
          "port bit table covers the historical bit set");

    for (const HistoricalBit& expected : kHistorical)
    {
        const input::JammaPortBit* bit = nullptr;
        for (std::uint32_t index = 0; index < bit_count; ++index)
        {
            if (bits[index].port == expected.port &&
                bits[index].mask == expected.mask)
            {
                bit = &bits[index];
                break;
            }
        }

        std::ostringstream label;
        label << "port 0x" << std::hex << expected.port << " bit 0x"
              << static_cast<unsigned>(expected.mask);
        if (bit == nullptr)
        {
            Check(false, label.str() + " is still mapped");
            continue;
        }

        const input::JammaInputBinding& binding = defaults.Get(bit->key);
        bool matches = binding.alias_count == expected.scancode_count;
        for (std::uint32_t slot = 0;
             matches && slot < expected.scancode_count; ++slot)
        {
            const HostKeyAlias& alias = binding.aliases[slot];
            matches = alias.scancode == expected.scancodes[slot] &&
                      !alias.has_modifiers();
        }
        Check(matches, label.str() + " keeps its historical host keys");
    }
}

void ProbeLayerChain()
{
    const auto lookup = [](std::string_view id) -> std::string_view
    {
        if (id == "child")
        {
            return "middle";
        }
        if (id == "middle")
        {
            return "root";
        }
        return std::string_view();
    };

    const std::vector<std::string> chain =
        config::BuildRomSetLayerIds("child", lookup);
    Check(chain.size() == 3 && chain[0] == "root" && chain[1] == "middle" &&
              chain[2] == "child",
          "layer ids are ordered root first");

    // A cycle in the profile data must terminate rather than spin.
    const auto cyclic = [](std::string_view id) -> std::string_view
    {
        return id == "a" ? "b" : "a";
    };
    const std::vector<std::string> bounded =
        config::BuildRomSetLayerIds("a", cyclic);
    Check(bounded.size() <= config::kMaxRomSetLayerCount,
          "a cyclic parent chain is bounded");
    Check(config::BuildRomSetLayerIds("", lookup).empty(),
          "an empty ROM set id yields no layers");
}

}  // namespace

bool RunRomSetConfigProbe()
{
    g_failure_count = 0;
    g_check_count = 0;

    ProbeIniParsing();
    ProbeBindingApplication();
    ProbeNameTable();
    ProbeCombinationParsing();
    ProbeModifierMatching();
    ProbeHistoricalDefaultMapping();
    ProbeGeneratedTemplate();
    ProbeLayerChain();

    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) / "repiu_romset_config_probe";
    std::filesystem::remove_all(root, error);
    ProbeLayeringAndGeneration(root);
    if (std::getenv("REPIU_PROBE_KEEP_ARTIFACTS") == nullptr)
    {
        std::filesystem::remove_all(root, error);
    }
    else
    {
        std::cout << "[romset-config] artifacts kept in " << root.string()
                  << "\n";
    }

    std::cout << "[romset-config] checks=" << g_check_count
              << " failures=" << g_failure_count << "\n";
    return g_failure_count == 0;
}

}  // namespace repiu::tools
