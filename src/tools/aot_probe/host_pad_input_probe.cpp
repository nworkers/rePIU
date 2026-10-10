#include "host_pad_input_probe.h"

#include "repiu/config/ini_document.h"
#include "repiu/input/host_pad_binding.h"
#include "repiu/input/host_pad_state.h"
#include "repiu/input/jamma_input_bindings.h"
#include "repiu/input/pad_exit_chord.h"
#include "repiu/input/pad_osd_chord.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>

#include <iostream>
#include <string>
#include <vector>

namespace repiu::tools
{
namespace
{

using input::HostPadAlias;
using input::HostPadControlKind;
using input::HostPadState;
using input::JammaInputKey;
using input::JammaInputKeyMask;
using input::ResolvedJammaBindings;

bool Parses(const char* text, HostPadAlias* alias)
{
    std::string error;
    return input::ParseHostPadAlias(text, alias, &error);
}

bool Rejects(const char* text)
{
    HostPadAlias alias;
    std::string error;
    return !input::ParseHostPadAlias(text, &alias, &error) && !error.empty();
}

ResolvedJammaBindings Apply(const char* ini,
                            std::vector<std::string>* warnings)
{
    ResolvedJammaBindings bindings = input::DefaultJammaBindings();
    const config::IniDocument document =
        config::IniDocument::Parse(ini, "pad-probe");
    input::ApplyJammaInputSection(document, &bindings, warnings);
    input::FinalizeJammaBindings(&bindings);
    return bindings;
}

// Routing: only Pad<N>/Joy<N> shapes leave the key parser.
bool ProbeShape()
{
    return input::LooksLikeHostPadAlias("Pad1_A") &&
           input::LooksLikeHostPadAlias("pad1a") &&
           input::LooksLikeHostPadAlias("JOY_2_BUTTON_3") &&
           !input::LooksLikeHostPadAlias("Padding") &&
           !input::LooksLikeHostPadAlias("Joy") &&
           !input::LooksLikeHostPadAlias("Q") &&
           !input::LooksLikeHostPadAlias("PageUp");
}

bool ProbeParse()
{
    HostPadAlias alias;
    bool ok = Parses("Pad1_A", &alias) &&
              alias.kind == HostPadControlKind::kGamepadButton &&
              alias.slot == 0 && alias.control == SDL_GAMEPAD_BUTTON_SOUTH;
    ok = ok && Parses("pad4_dpad_up", &alias) && alias.slot == 3 &&
         alias.control == SDL_GAMEPAD_BUTTON_DPAD_UP;
    HostPadAlias south;
    ok = ok && Parses("Pad2_South", &south) && Parses("Pad2_A", &alias) &&
         south.control == alias.control && south.slot == alias.slot;
    ok = ok && Parses("Pad1_RightTrigger", &alias) &&
         alias.kind == HostPadControlKind::kGamepadTrigger &&
         alias.control == 1;
    ok = ok && Parses("Joy8_Button32", &alias) &&
         alias.kind == HostPadControlKind::kJoystickButton &&
         alias.slot == 7 && alias.control == 31;
    ok = ok && Parses("Joy1_Hat4Left", &alias) &&
         alias.kind == HostPadControlKind::kJoystickHat &&
         alias.control == 3 && alias.hat_direction == SDL_HAT_LEFT;
    return ok;
}

bool ProbeRejects()
{
    return Rejects("Pad0_A") && Rejects("Pad5_A") && Rejects("Pad1_Z") &&
           Rejects("Joy9_Button1") && Rejects("Joy1_Button0") &&
           Rejects("Joy1_Button33") && Rejects("Joy1_Hat5Up") &&
           Rejects("Joy1_Hat1Middle") && Rejects("Joy1_Axis1") &&
           Rejects("Joy1_Button3x");
}

// Every canonical spelling formats back to itself.
bool ProbeFormat()
{
    const char* names[] = {"Pad1_A",        "Pad2_B",       "Pad3_X",
                           "Pad4_Y",        "Pad1_Back",    "Pad1_Start",
                           "Pad1_DpadLeft", "Pad2_LeftTrigger",
                           "Pad2_RightTrigger", "Joy1_Button1",
                           "Joy8_Button32", "Joy3_Hat2Up", "Joy3_Hat2Right"};
    for (const char* name : names)
    {
        HostPadAlias alias;
        if (!Parses(name, &alias) || input::FormatHostPadAlias(alias) != name)
        {
            return false;
        }
    }
    HostPadAlias synonym;
    return Parses("pad1_north", &synonym) &&
           input::FormatHostPadAlias(synonym) == "Pad1_Y";
}

// A value mixes keys and pads, round trips, and an empty value clears both.
bool ProbeMixedValue()
{
    std::vector<std::string> warnings;
    const ResolvedJammaBindings mixed = Apply(
        "[Input]\nP1_CENTER = S, Pad1_A, Joy1_Button5\nTEST =\n", &warnings);
    const auto& center = mixed.Get(JammaInputKey::kP1Center);
    const auto& test = mixed.Get(JammaInputKey::kTest);
    bool ok = warnings.empty() && center.alias_count == 1 &&
              center.pad_alias_count == 2 &&
              input::FormatJammaBinding(mixed, JammaInputKey::kP1Center) ==
                  "S, Pad1_A, Joy1_Button5" &&
              test.alias_count == 0 && test.pad_alias_count == 0;

    std::vector<std::string> bad_warnings;
    const ResolvedJammaBindings bad = Apply(
        "[Input]\nP1_CENTER = S, Pad9_A\n"
        "P2_CENTER = Pad1_A, Pad1_B, Pad1_X, Pad1_Y, Pad2_A\n",
        &bad_warnings);
    ok = ok && bad_warnings.size() == 2 &&
         bad.Get(JammaInputKey::kP1Center).alias_count == 1 &&
         bad.Get(JammaInputKey::kP1Center).pad_alias_count == 0 &&
         bad.Get(JammaInputKey::kP2Center).pad_alias_count == 4;
    return ok;
}

bool ProbeDefaults()
{
    const ResolvedJammaBindings defaults = input::DefaultJammaBindings();
    return input::FormatJammaBinding(defaults, JammaInputKey::kP1UpLeft) ==
               "Q, Pad1_LeftShoulder" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kP1UpRight) ==
               "E, Pad1_RightShoulder" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kP1DownLeft) ==
               "Z, Pad1_DpadDown, Pad1_DpadLeft" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kP1Center) ==
               "S, Pad1_A, Pad1_Start" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kCoin1) ==
               "F5, Pad1_Back, Pad2_Back" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kTest) == "F1" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kService) ==
               "F2" &&
           input::FormatJammaBinding(defaults, JammaInputKey::kP2DownRight) ==
               "Keypad3, PageDown, Pad2_B, Pad2_DpadRight" &&
           defaults.Get(JammaInputKey::kP2DownRight).alias_count == 2;
}

bool ProbeMask()
{
    const ResolvedJammaBindings defaults = input::DefaultJammaBindings();
    HostPadState state;
    bool ok = input::ComputeJammaPadMask(defaults, state) == 0U;

    state.gamepad_buttons[0] = (1U << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) |
                               (1U << SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    state.gamepad_buttons[1] = (1U << SDL_GAMEPAD_BUTTON_SOUTH) |
                               (1U << SDL_GAMEPAD_BUTTON_EAST);
    ok = ok && input::ComputeJammaPadMask(defaults, state) ==
                   (JammaInputKeyMask(JammaInputKey::kP1UpRight) |
                    JammaInputKeyMask(JammaInputKey::kP1DownLeft) |
                    JammaInputKeyMask(JammaInputKey::kP2Center) |
                    JammaInputKeyMask(JammaInputKey::kP2DownRight));

    // A pad with no number bound (Pad3) changes nothing.
    HostPadState unbound;
    unbound.gamepad_buttons[2] = 0xFFFFFFFFU;
    ok = ok && input::ComputeJammaPadMask(defaults, unbound) == 0U;

    std::vector<std::string> warnings;
    const ResolvedJammaBindings custom = Apply(
        "[Input]\nP1_UP_LEFT = Joy1_Hat1Up\nP1_UP_RIGHT = Joy1_Hat1Right\n"
        "P1_CENTER = Joy2_Button5\nCOIN1 = Pad1_LeftTrigger\n",
        &warnings);
    HostPadState hats;
    hats.joystick_hats[0][0] = SDL_HAT_RIGHTUP;
    hats.joystick_buttons[1] = 1U << 4;
    hats.gamepad_triggers[0] = 0x01U;
    ok = ok && warnings.empty() &&
         input::ComputeJammaPadMask(custom, hats) ==
             (JammaInputKeyMask(JammaInputKey::kP1UpLeft) |
              JammaInputKeyMask(JammaInputKey::kP1UpRight) |
              JammaInputKeyMask(JammaInputKey::kP1Center) |
              JammaInputKeyMask(JammaInputKey::kCoin1));

    hats.ClearJoystick(0);
    ok = ok && input::ComputeJammaPadMask(custom, hats) ==
                   (JammaInputKeyMask(JammaInputKey::kP1Center) |
                    JammaInputKeyMask(JammaInputKey::kCoin1));
    return ok;
}

// Numbers follow connection order, survive another device leaving, and are
// reused lowest first.
bool ProbeSlots()
{
    input::HostPadSlotTable<2> table;
    bool ok = table.Assign(10) == 0 && table.Assign(20) == 1 &&
              table.Assign(10) == 0 && table.Assign(30) == -1 &&
              table.Assign(0) == -1;
    ok = ok && table.Release(10) == 0 && table.Find(20) == 1 &&
         table.Assign(30) == 0 && table.Release(99) == -1 &&
         table.IdAt(0) == 30;
    return ok;
}

// Issue #52. The exit chord counts only all four on one pad, and fires once
// after a second of unbroken holding.
bool ProbeExitChord()
{
    constexpr std::uint32_t kSticks =
        (1U << SDL_GAMEPAD_BUTTON_LEFT_STICK) |
        (1U << SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    input::HostPadState state;
    state.gamepad_buttons[1] = kSticks | (1U << SDL_GAMEPAD_BUTTON_SOUTH);
    state.gamepad_triggers[1] = 0x03U;
    bool ok = input::IsPadExitChordDown(state);
    // One trigger short.
    state.gamepad_triggers[1] = 0x01U;
    ok = ok && !input::IsPadExitChordDown(state);
    // Split across two pads: triggers on one, stick clicks on the other.
    state.gamepad_triggers[1] = 0x00U;
    state.gamepad_triggers[0] = 0x03U;
    ok = ok && !input::IsPadExitChordDown(state);
    // One stick click short.
    input::HostPadState missing;
    missing.gamepad_buttons[0] = 1U << SDL_GAMEPAD_BUTTON_LEFT_STICK;
    missing.gamepad_triggers[0] = 0x03U;
    ok = ok && !input::IsPadExitChordDown(missing);
    // The threshold the launcher shares with the game: past half.
    ok = ok && input::kHostPadTriggerThreshold == 16384;

    input::PadExitChordTimer timer;
    ok = ok && !timer.Update(true, 5000) && !timer.Update(true, 5999) &&
        timer.Update(true, 6000) && !timer.Update(true, 6001) &&
        !timer.Update(true, 9000);
    // Released, then held again: a fresh second.
    ok = ok && !timer.Update(false, 9100) && !timer.Update(true, 9200) &&
        !timer.Update(true, 10199) && timer.Update(true, 10200);
    // A break before the second restarts it.
    input::PadExitChordTimer broken;
    ok = ok && !broken.Update(true, 0) && !broken.Update(true, 900) &&
        !broken.Update(false, 950) && !broken.Update(true, 1000) &&
        !broken.Update(true, 1500) && broken.Update(true, 2000);
    return ok;
}

// Issue #55. LT+RT+Y on one pad, its edge, and the gate that keeps pad input
// from the game while the OSD is open and lets nothing held leak as it closes.
bool ProbeOsdChord()
{
    constexpr std::uint32_t kY = 1U << SDL_GAMEPAD_BUTTON_NORTH;
    input::HostPadState state;
    state.gamepad_buttons[0] = kY;
    state.gamepad_triggers[0] = 0x03U;
    bool ok = input::IsPadOsdChordDown(state);
    state.gamepad_triggers[0] = 0x02U;
    ok = ok && !input::IsPadOsdChordDown(state);
    // Split: triggers on one pad, Y on the other.
    state.gamepad_triggers[0] = 0x00U;
    state.gamepad_triggers[1] = 0x03U;
    ok = ok && !input::IsPadOsdChordDown(state);
    // The exit chord's buttons do not open it.
    input::HostPadState exit_chord;
    exit_chord.gamepad_buttons[0] = (1U << SDL_GAMEPAD_BUTTON_LEFT_STICK) |
        (1U << SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    exit_chord.gamepad_triggers[0] = 0x03U;
    ok = ok && !input::IsPadOsdChordDown(exit_chord) &&
        input::IsPadExitChordDown(exit_chord);

    input::PadChordEdge edge;
    ok = ok && edge.Update(true) && !edge.Update(true) && !edge.Update(false) &&
        edge.Update(true);

    // The gate: open passes everything; suppressed passes nothing; as it
    // reopens, a held bit stays hidden until released and pressed again.
    constexpr std::uint16_t kCenter = 0x0004U;
    constexpr std::uint16_t kDownRight = 0x0010U;
    input::PadGameGate gate;
    ok = ok && gate.Apply(kCenter) == kCenter && !gate.suppressed();
    gate.SetSuppressed(true, kCenter);
    ok = ok && gate.suppressed() && gate.Apply(kCenter | kDownRight) == 0U;
    gate.SetSuppressed(false, kDownRight);
    ok = ok && gate.Apply(kDownRight) == 0U &&
        gate.Apply(kDownRight | kCenter) == kCenter &&
        gate.Apply(kCenter) == kCenter && gate.Apply(kDownRight) == kDownRight;
    return ok;
}

}  // namespace

bool RunHostPadInputProbe()
{
    const bool shape = ProbeShape();
    const bool parse = ProbeParse();
    const bool rejects = ProbeRejects();
    const bool format = ProbeFormat();
    const bool mixed = ProbeMixedValue();
    const bool defaults = ProbeDefaults();
    const bool mask = ProbeMask();
    const bool slots = ProbeSlots();
    const bool exit_chord = ProbeExitChord();
    const bool osd_chord = ProbeOsdChord();
    const bool all = shape && parse && rejects && format && mixed &&
                     defaults && mask && slots && exit_chord && osd_chord;
    std::cout << "host_pad_input_shape=" << (shape ? "true" : "false")
              << "\nhost_pad_input_parse=" << (parse ? "true" : "false")
              << "\nhost_pad_input_rejects=" << (rejects ? "true" : "false")
              << "\nhost_pad_input_format=" << (format ? "true" : "false")
              << "\nhost_pad_input_mixed_value="
              << (mixed ? "true" : "false")
              << "\nhost_pad_input_defaults=" << (defaults ? "true" : "false")
              << "\nhost_pad_input_mask=" << (mask ? "true" : "false")
              << "\nhost_pad_input_slots=" << (slots ? "true" : "false")
              << "\nhost_pad_input_exit_chord="
              << (exit_chord ? "true" : "false")
              << "\nhost_pad_input_osd_chord="
              << (osd_chord ? "true" : "false")
              << "\nhost_pad_input_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
