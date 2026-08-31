#include "long_mode_compatibility_probe.h"

#include "repiu/runtime/aot_long_mode_compatibility.h"

#include <Zydis.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::runtime::ClassifyLongModeBytes;
using repiu::runtime::LongModeByteCompatibility;
using repiu::runtime::LongModeCompatibilityResult;
using repiu::runtime::LongModeDivergence;

// Task 550. This probe is written to prove refusals rather than passes.
//
// The pass list is the small half and the easy half. What decides whether an
// x64 emitter is safe is the other one: the encodings that must never be
// answered `kIdenticalBytes`, because copying them produces a program that runs
// and is wrong. A probe that only checked that `xor eax, eax` is allowed would
// pass against a classifier that allowed everything.

struct Case
{
    const char* name;
    std::vector<std::uint8_t> bytes;
};

LongModeCompatibilityResult Classify(const Case& item)
{
    return ClassifyLongModeBytes(item.bytes.data(), item.bytes.size());
}

bool RefusesAll(const char* group,
                const std::initializer_list<Case>& cases,
                const bool report_each)
{
    bool ok = true;
    std::size_t refused = 0;
    for (const Case& item : cases)
    {
        const LongModeCompatibilityResult result = Classify(item);
        const bool refused_here = result.compatibility !=
            LongModeByteCompatibility::kIdenticalBytes;
        ok = ok && refused_here;
        refused += refused_here ? 1U : 0U;
        // The silent group is named case by case. These are the encodings whose
        // whole danger is that they pass quietly, so a single count is the one
        // shape of report they must not be allowed to hide in.
        if (report_each)
        {
            std::cout << "  long_mode_refused_" << item.name << "="
                      << (refused_here ? "true" : "false") << "\n";
        }
    }
    std::cout << group << "=" << (ok ? "true" : "false") << ",refused="
              << refused << "/" << cases.size() << "\n";
    return ok;
}

// A. Encodings long mode decodes as a different instruction, without raising.
bool ProbeSilentlyDifferent()
{
    return RefusesAll(
        "long_mode_silently_different",
        {
            // 40: `inc eax` in 32-bit mode, a REX prefix in long mode.
            // 4F: `dec edi`, likewise. Task 557 gave both a re-encoding, so
            // they are no longer *refused* -- but they must never become
            // `kIdenticalBytes`, which is exactly what this group asserts, and
            // that is why they stay here rather than moving out.
            {"inc_eax", {0x40U}},
            {"dec_edi", {0x4FU}},
            // 62: BOUND -> EVEX prefix.
            {"bound", {0x62U, 0x04U, 0x24U}},
            // 63: ARPL -> MOVSXD.
            {"arpl", {0x63U, 0xC0U}},
            // C4: LES -> three-byte VEX prefix.
            {"les", {0xC4U, 0x04U, 0x24U}},
            // C5: LDS -> two-byte VEX prefix.
            {"lds", {0xC5U, 0x04U, 0x24U}},
            // A1: `mov eax, [0x12345678]`. In long mode the immediate is eight
            // bytes, so the instruction's own length changes and the decode of
            // everything after it moves too.
            {"mov_eax_moffs", {0xA1U, 0x78U, 0x56U, 0x34U, 0x12U}},
            // 8B 05 disp32: absolute in 32-bit mode, RIP-relative in long mode.
            {"absolute_disp32",
             {0x8BU, 0x05U, 0x78U, 0x56U, 0x34U, 0x12U}},
        },
        true);
}

// B. Encodings long mode does not have. They raise #UD rather than running,
// which is the safe half of the refusal list -- and still refused.
bool ProbeInvalidInLongMode()
{
    return RefusesAll(
        "long_mode_invalid",
        {
            {"push_es", {0x06U}},
            {"pop_ds", {0x1FU}},
            {"daa", {0x27U}},
            {"aas", {0x3FU}},
            {"pushad", {0x60U}},
            {"popad", {0x61U}},
            {"call_far", {0x9AU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},
            {"into", {0xCEU}},
            {"aam", {0xD4U, 0x0AU}},
            {"jmp_far", {0xEAU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},
        },
        false);
}

// C. Encodings whose meaning survives at a different width. Long mode gives
// every stack instruction a 64-bit operand size and no way back to 32.
bool ProbeWidthReencode()
{
    return RefusesAll(
        "long_mode_width",
        {
            {"push_eax", {0x50U}},
            {"pop_edi", {0x5FU}},
            {"push_imm32", {0x68U, 0x78U, 0x56U, 0x34U, 0x12U}},
            {"push_imm8", {0x6AU, 0x10U}},
            {"pushfd", {0x9CU}},
            {"popfd", {0x9DU}},
            {"ret", {0xC3U}},
            {"ret_imm16", {0xC2U, 0x08U, 0x00U}},
            {"leave", {0xC9U}},
            {"call_rel32", {0xE8U, 0x00U, 0x00U, 0x00U, 0x00U}},
            {"call_rm32", {0xFFU, 0xD0U}},
            {"push_fs", {0x0FU, 0xA0U}},
            {"pop_gs", {0x0FU, 0xA9U}},
        },
        false);
}

// The width group must also be told apart from the refusals. An emitter that
// treated `push eax` as unsupported rather than re-encodable would be safe and
// useless, and this is the assertion that keeps the difference real.
bool ProbeWidthIsReencodeRatherThanRefusal()
{
    const Case push_eax{"push_eax", {0x50U}};
    const Case ret{"ret", {0xC3U}};
    const bool ok =
        Classify(push_eax).compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        Classify(ret).compatibility ==
            LongModeByteCompatibility::kNeedsReencode;
    std::cout << "long_mode_width_is_reencode=" << (ok ? "true" : "false")
              << "\n";
    return ok;
}

// Task 555. The stack pointer, in both roles.
//
// Written to check a refusal that used to be an admission: `add esp,16` reached
// `kIdenticalBytes`, and in long mode writing `ESP` zero-extends into `RSP` --
// the host's stack pointer. `mov eax,[esp+8]` is the milder half, lowered with
// a prefix and reading the host stack.
//
// The last case is the one that keeps this honest. `mov eax,[ebx+8]` must still
// be lowered, or the "fix" would just be a blanket refusal of memory operands,
// which is what Task 552's measurement opened.
bool ProbeStackPointerRefusal()
{
    struct StackCase
    {
        const char* name;
        std::vector<std::uint8_t> bytes;
    };
    bool ok = true;
    for (const StackCase& item : {
             StackCase{"add_esp_imm8", {0x83U, 0xC4U, 0x10U}},
             StackCase{"sub_esp_imm32",
                       {0x81U, 0xECU, 0x20U, 0x00U, 0x00U, 0x00U}},
             StackCase{"mov_eax_esp_disp", {0x8BU, 0x44U, 0x24U, 0x08U}},
             StackCase{"mov_esp_disp_ecx", {0x89U, 0x4CU, 0x24U, 0x04U}},
             StackCase{"mov_esp_eax", {0x89U, 0xC4U}},
             StackCase{"lea_eax_esp_disp", {0x8DU, 0x44U, 0x24U, 0x08U}},
         })
    {
        const LongModeCompatibilityResult result =
            ClassifyLongModeBytes(item.bytes.data(), item.bytes.size());
        const bool refused = result.compatibility ==
                LongModeByteCompatibility::kUnsupported &&
            result.divergence ==
                LongModeDivergence::kStackPointerRegister &&
            result.lowering == repiu::runtime::LongModeLowering::kNone;
        if (!refused)
        {
            std::cout << "  long_mode_stack_refused_" << item.name
                      << "=false\n";
        }
        ok = ok && refused;
    }

    // The control: a base register the project has decided nothing against is
    // still lowered, so the refusal above is targeted rather than a blanket.
    const std::uint8_t base_relative[] = {0x8BU, 0x43U, 0x08U};
    const LongModeCompatibilityResult control =
        ClassifyLongModeBytes(base_relative, sizeof(base_relative));
    const bool control_ok = control.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        control.lowering ==
            repiu::runtime::LongModeLowering::kAddressSizePrefix;
    std::cout << "long_mode_stack_pointer_refused=" << (ok ? "true" : "false")
              << ",non_stack_base_still_lowered="
              << (control_ok ? "true" : "false") << "\n";
    return ok && control_ok;
}

// Task 557. INC/DEC r32 becomes the ModRM group form.
//
// The table in the design was copied by hand, so the central item here does not
// compare against another hand-written table -- it decodes the lowered bytes
// with a long-mode decoder and asks whether the mnemonic and the register came
// out the way the original meant. That is what catches a transcription slip.
bool ProbeIncDecLowering()
{
    ZydisDecoder legacy;
    ZydisDecoder long_mode;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&legacy, ZYDIS_MACHINE_MODE_LEGACY_32,
                                       ZYDIS_STACK_WIDTH_32)) ||
        !ZYAN_SUCCESS(ZydisDecoderInit(&long_mode, ZYDIS_MACHINE_MODE_LONG_64,
                                       ZYDIS_STACK_WIDTH_64)))
    {
        std::cout << "long_mode_inc_dec_decoder=false" "\n";
        return false;
    }

    bool ok = true;
    std::size_t lowered_count_total = 0;
    for (std::uint32_t opcode = 0x40U; opcode <= 0x4FU; ++opcode)
    {
        const std::uint8_t original[] = {static_cast<std::uint8_t>(opcode)};
        const LongModeCompatibilityResult verdict =
            ClassifyLongModeBytes(original, sizeof(original));

        // 44 is `inc esp` and 4C is `dec esp`. Lowered they would write the
        // host's stack pointer, so they stay refused -- and for the stack
        // pointer's reason, not a vague one.
        if (opcode == 0x44U || opcode == 0x4CU)
        {
            const bool refused = verdict.compatibility ==
                    LongModeByteCompatibility::kUnsupported &&
                verdict.divergence ==
                    LongModeDivergence::kStackPointerRegister;
            if (!refused)
            {
                std::cout << "  long_mode_inc_dec_esp_refused_" << std::hex
                          << opcode << std::dec << "=false" "\n";
            }
            ok = ok && refused;
            continue;
        }

        std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
        std::size_t count = 0;
        const bool produced = verdict.lowering ==
                repiu::runtime::LongModeLowering::kIncDecToModRm &&
            repiu::runtime::LowerLongModeBytes(original, sizeof(original),
                                               lowered, &count) &&
            count == 2U && lowered[0] == 0xFFU;
        if (!produced)
        {
            std::cout << "  long_mode_inc_dec_lowered_" << std::hex << opcode
                      << std::dec << "=false" "\n";
            ok = false;
            continue;
        }
        lowered_count_total += 1U;

        // What the original meant, read where it came from.
        ZydisDecodedInstruction source{};
        ZydisDecodedOperand source_operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        // What the rewrite means, read where it will run.
        ZydisDecodedInstruction target{};
        ZydisDecodedOperand target_operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        const bool decoded =
            ZYAN_SUCCESS(ZydisDecoderDecodeFull(&legacy, original,
                                                sizeof(original), &source,
                                                source_operands)) &&
            ZYAN_SUCCESS(ZydisDecoderDecodeFull(&long_mode, lowered, count,
                                                &target, target_operands));
        const bool same = decoded && source.length == 1U &&
            target.length == 2U &&
            source.mnemonic == target.mnemonic &&
            source_operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            target_operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            source_operands[0].reg.value == target_operands[0].reg.value;
        if (!same)
        {
            std::cout << "  long_mode_inc_dec_means_same_" << std::hex
                      << opcode << std::dec << "=false" "\n";
        }
        ok = ok && same;
    }

    // A prefixed form stays refused: this unit lowers the bare byte only.
    const std::uint8_t prefixed[] = {0x66U, 0x40U};
    const LongModeCompatibilityResult prefixed_verdict =
        ClassifyLongModeBytes(prefixed, sizeof(prefixed));
    const bool prefixed_refused = prefixed_verdict.compatibility ==
        LongModeByteCompatibility::kUnsupported;

    std::cout << "long_mode_inc_dec_lowering=" << (ok ? "true" : "false")
              << ",lowered=" << lowered_count_total << "/14"
              << ",prefixed_refused="
              << (prefixed_refused ? "true" : "false") << "\n";
    return ok && prefixed_refused;
}

// The refusal groups must report *why*, not merely that they were refused. The
// reason is what tells a later reader whether an instruction is waiting for an
// emitter or must never be emitted at all.
bool ProbeDivergenceReasons()
{
    // Task 557 moved `40` from refused to lowered, so the silently-different
    // reason is now demonstrated by one that has no re-encoding: `A1` is
    // `mov eax, moffs32`, which long mode reads as a 64-bit offset and whose
    // length changes with it.
    const Case moffs{"mov_eax_moffs32",
                     {0xA1U, 0x78U, 0x56U, 0x34U, 0x12U}};
    const Case absolute{"absolute_disp32",
                        {0x8BU, 0x05U, 0x78U, 0x56U, 0x34U, 0x12U}};
    const Case register_memory{"mov_eax_mem", {0x8BU, 0x03U}};
    const Case pushad{"pushad", {0x60U}};
    const bool ok =
        Classify(moffs).divergence ==
            LongModeDivergence::kSilentlyDifferent &&
        Classify(absolute).divergence ==
            LongModeDivergence::kRipRelativeDisplacement &&
        Classify(register_memory).divergence ==
            LongModeDivergence::kAddressSize &&
        Classify(pushad).divergence ==
            LongModeDivergence::kInvalidInLongMode;
    std::cout << "long_mode_divergence_reasons=" << (ok ? "true" : "false")
              << "\n";
    return ok;
}

// The subset that is allowed. Small on purpose: register-only work at 8, 16, or
// 32 bits, which is what remains once everything above is taken out.
bool ProbeAdmittedSubset()
{
    bool ok = true;
    for (const Case& item : {
             Case{"xor_eax_eax", {0x31U, 0xC0U}},
             Case{"add_eax_ebx", {0x01U, 0xD8U}},
             Case{"mov_ecx_edx", {0x89U, 0xD1U}},
             Case{"cmp_eax_imm32", {0x3DU, 0x78U, 0x56U, 0x34U, 0x12U}},
             Case{"shl_eax_1", {0xD1U, 0xE0U}},
             Case{"movzx_eax_bl", {0x0FU, 0xB6U, 0xC3U}},
             Case{"test_al_imm8", {0xA8U, 0x01U}},
             Case{"inc_eax_ff_form", {0xFFU, 0xC0U}},
         })
    {
        const bool admitted = Classify(item).compatibility ==
            LongModeByteCompatibility::kIdenticalBytes;
        if (!admitted)
        {
            std::cout << "  long_mode_admitted_" << item.name << "=false\n";
        }
        ok = ok && admitted;
    }
    std::cout << "long_mode_admits_gpr_subset=" << (ok ? "true" : "false")
              << "\n";
    return ok;
}

// Bytes that are not an instruction, and no bytes at all. Both are questions
// with the same answer, and a classifier that fell through to its optimistic
// case on either would be worse than one that refused everything.
bool ProbeRefusals()
{
    const std::uint8_t truncated[] = {0x8BU};
    const bool ok =
        ClassifyLongModeBytes(nullptr, 4U).compatibility ==
            LongModeByteCompatibility::kUnsupported &&
        ClassifyLongModeBytes(truncated, 0U).compatibility ==
            LongModeByteCompatibility::kUnsupported &&
        ClassifyLongModeBytes(truncated, sizeof(truncated)).compatibility ==
            LongModeByteCompatibility::kUnsupported;
    std::cout << "long_mode_refusals=" << (ok ? "true" : "false") << "\n";
    return ok;
}

}  // namespace

bool RunLongModeCompatibilityProbe()
{
    const bool silent_ok = ProbeSilentlyDifferent();
    const bool invalid_ok = ProbeInvalidInLongMode();
    const bool width_ok = ProbeWidthReencode();
    const bool width_kind_ok = ProbeWidthIsReencodeRatherThanRefusal();
    const bool reasons_ok = ProbeDivergenceReasons();
    const bool stack_ok = ProbeStackPointerRefusal();
    const bool inc_dec_ok = ProbeIncDecLowering();
    const bool subset_ok = ProbeAdmittedSubset();
    const bool refusals_ok = ProbeRefusals();

    const bool all = silent_ok && invalid_ok && width_ok && width_kind_ok &&
        reasons_ok && stack_ok && inc_dec_ok && subset_ok && refusals_ok;
    std::cout << "long_mode_compatibility_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
