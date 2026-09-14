#include "long_mode_compatibility_probe.h"

#include "repiu/runtime/aot_long_mode_compatibility.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <Zydis.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <utility>
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

// Task 555, rewritten by Task 564. The stack pointer, in all three roles.
//
// 555 wrote this to check a refusal that used to be an admission: `add esp,16`
// reached `kIdenticalBytes`, and in long mode writing `ESP` zero-extends into
// `RSP` -- the host's stack pointer.
//
// 564 re-encodes them instead, so what must hold is no longer "refused" but
// "never `kIdenticalBytes`, and named as the R15 rewrite". The danger these
// cases carry is unchanged; only the answer to it is. Asserting the refusal
// after the re-encoder existed would have been asserting the past, which is the
// third time this has come up -- see Task 562's `kReturn`.
//
// The last case keeps it honest: `mov eax,[ebx+8]` must still get the ordinary
// prefix lowering, or "fixed" would just mean a blanket refusal of memory
// operands.
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
             StackCase{"cmp_byte_esp_imm8", {0x80U, 0x3CU, 0x24U, 0x00U}},
         })
    {
        const LongModeCompatibilityResult result =
            ClassifyLongModeBytes(item.bytes.data(), item.bytes.size());
        const bool reencoded = result.compatibility ==
                LongModeByteCompatibility::kNeedsReencode &&
            result.divergence ==
                LongModeDivergence::kStackPointerRegister &&
            result.lowering ==
                repiu::runtime::LongModeLowering::kStackPointerToR15;
        if (!reencoded)
        {
            std::cout << "  long_mode_stack_reencoded_" << item.name
                      << "=false\n";
        }
        ok = ok && reencoded;
    }

    // The runtime frontier is a memory compare with an immediate. Keep the
    // immediate in the assertion: changing only the SIB base must not turn
    // `cmp byte [esp],0` into a different compare or lose its final byte.
    const std::uint8_t cmp_byte_esp[] = {
        0x80U, 0x3CU, 0x24U, 0x00U};
    const std::uint8_t cmp_byte_esp_expected[] = {
        0x41U, 0x80U, 0x3CU, 0x27U, 0x00U};
    std::uint8_t cmp_byte_esp_lowered[
        repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t cmp_byte_esp_count = 0U;
    const bool cmp_byte_esp_lowered_ok =
        repiu::runtime::LowerLongModeBytes(
            cmp_byte_esp, sizeof(cmp_byte_esp), cmp_byte_esp_lowered,
            &cmp_byte_esp_count, nullptr) &&
        cmp_byte_esp_count == sizeof(cmp_byte_esp_expected) &&
        std::memcmp(cmp_byte_esp_lowered, cmp_byte_esp_expected,
                    sizeof(cmp_byte_esp_expected)) == 0;
    std::cout << "long_mode_cmp_byte_esp_lowered="
              << (cmp_byte_esp_lowered_ok ? "true" : "false") << "\n";

    // The control: a base register the project has decided nothing against is
    // still lowered with the ordinary prefix, so the R15 rewrite above is
    // targeted rather than swallowing every memory operand.
    const std::uint8_t base_relative[] = {0x8BU, 0x43U, 0x08U};
    const LongModeCompatibilityResult control =
        ClassifyLongModeBytes(base_relative, sizeof(base_relative));
    const bool control_ok = control.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        control.lowering ==
            repiu::runtime::LongModeLowering::kAddressSizePrefix;

    // Task 674. The opcode-embedded `MOV ESP,imm32` form has no ModRM field,
    // so it needs its own lowering to the guest stack register R15D.
    const std::uint8_t mov_esp_immediate[] = {
        0xBCU, 0x00U, 0x20U, 0xFBU, 0x8DU};
    const std::uint8_t expected_mov_esp_immediate[] = {
        0x41U, 0xBFU, 0x00U, 0x20U, 0xFBU, 0x8DU};
    std::uint8_t mov_esp_immediate_lowered[
        repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t mov_esp_immediate_count = 0U;
    const LongModeCompatibilityResult mov_esp_immediate_verdict =
        ClassifyLongModeBytes(mov_esp_immediate,
                              sizeof(mov_esp_immediate));
    const bool mov_esp_immediate_ok =
        mov_esp_immediate_verdict.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        mov_esp_immediate_verdict.divergence ==
            LongModeDivergence::kStackPointerRegister &&
        mov_esp_immediate_verdict.lowering ==
            repiu::runtime::LongModeLowering::kStackPointerImmediateToR15 &&
        repiu::runtime::LowerLongModeBytes(
            mov_esp_immediate, sizeof(mov_esp_immediate),
            mov_esp_immediate_lowered, &mov_esp_immediate_count, nullptr) &&
        mov_esp_immediate_count == sizeof(expected_mov_esp_immediate) &&
        std::memcmp(mov_esp_immediate_lowered,
                    expected_mov_esp_immediate,
                    sizeof(expected_mov_esp_immediate)) == 0;
    // Task 676. `MOV AH,[ESP+0x2C]` cannot keep AH as the ModRM destination
    // once a REX prefix is needed for the guest stack base. The lowering uses
    // DL as a temporary byte and restores it around the R15-based load.
    const std::uint8_t high_byte_destination[] = {
        0x8AU, 0x64U, 0x24U, 0x2CU};
    const std::uint8_t expected_high_byte_destination[] = {
        0x44U, 0x88U, 0xF2U,
        0x41U, 0x8AU, 0x54U, 0x27U, 0x2CU,
        0x8AU, 0xE2U,
        0x44U, 0x88U, 0xF2U};
    std::uint8_t high_byte_destination_lowered[
        repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t high_byte_destination_count = 0U;
    const LongModeCompatibilityResult high_byte_destination_verdict =
        ClassifyLongModeBytes(high_byte_destination,
                              sizeof(high_byte_destination));
    const bool high_byte_destination_ok =
        high_byte_destination_verdict.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        high_byte_destination_verdict.divergence ==
            LongModeDivergence::kStackPointerRegister &&
        high_byte_destination_verdict.lowering ==
            repiu::runtime::LongModeLowering::
                kStackPointerHighByteDestinationToR15 &&
        repiu::runtime::LowerLongModeBytes(
            high_byte_destination, sizeof(high_byte_destination),
            high_byte_destination_lowered, &high_byte_destination_count,
            nullptr) &&
        high_byte_destination_count == sizeof(expected_high_byte_destination) &&
        std::memcmp(high_byte_destination_lowered,
                    expected_high_byte_destination,
                    sizeof(expected_high_byte_destination)) == 0;
    const std::uint8_t high_byte_read_write[] = {
        0x86U, 0x64U, 0x24U, 0x2CU};
    const bool high_byte_read_write_refused =
        ClassifyLongModeBytes(high_byte_read_write,
                              sizeof(high_byte_read_write)).compatibility ==
            LongModeByteCompatibility::kUnsupported;
    std::cout << "long_mode_stack_pointer_reencoded=" << (ok ? "true" : "false")
              << ",non_stack_base_still_lowered="
              << (control_ok ? "true" : "false")
              << ",mov_esp_immediate_lowered="
              << (mov_esp_immediate_ok ? "true" : "false")
              << ",high_byte_destination_lowered="
              << (high_byte_destination_ok ? "true" : "false")
              << ",high_byte_read_write_refused="
              << (high_byte_read_write_refused ? "true" : "false")
              << "\n";
    return ok && control_ok && cmp_byte_esp_lowered_ok &&
        mov_esp_immediate_ok && high_byte_destination_ok &&
        high_byte_read_write_refused;
}

// Task 557. INC/DEC r32 becomes the ModRM group form.
//
// The table in the design was copied by hand, so the central item here does not
// compare against another hand-written table -- it decodes the lowered bytes
// with a long-mode decoder and asks whether the mnemonic and the register came
// out the way the original meant. That is what catches a transcription slip.
// Task 559. The stack sequences, checked by decoding what they produce.
//
// Same method as Task 557's: the encoding table was written by hand, so the
// check is a long-mode decoder reading the result rather than a second table
// written by the same hand. Here it also asserts the instruction count, which
// is what the emitter's verification now compares against.
bool ProbeStackSequenceLowering()
{
    ZydisDecoder long_mode;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&long_mode, ZYDIS_MACHINE_MODE_LONG_64,
                                       ZYDIS_STACK_WIDTH_64)))
    {
        std::cout << "long_mode_stack_sequence_decoder=false" "\n";
        return false;
    }

    struct SeqCase
    {
        const char* name;
        std::vector<std::uint8_t> bytes;
        std::size_t instructions;
        // The mnemonic each emitted instruction must decode to, in order.
        std::vector<ZydisMnemonic> mnemonics;
    };
    const std::vector<SeqCase> cases = {
        {"push_eax", {0x50U}, 2, {ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"push_edi", {0x57U}, 2, {ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"pop_eax", {0x58U}, 2, {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA}},
        {"pop_edi", {0x5FU}, 2, {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA}},
        {"push_esp", {0x54U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"pop_esp", {0x5CU}, 1, {ZYDIS_MNEMONIC_MOV}},
        {"push_imm32", {0x68U, 0x78U, 0x56U, 0x34U, 0x12U}, 2,
         {ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"push_imm8", {0x6AU, 0xFFU}, 2,
         {ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        // Task 669. FF /6 must use the guest stack even when its source is a
        // memory operand. The source load precedes the guest ESP decrement.
        {"push_rm32_register", {0xFFU, 0xF6U}, 2,
         {ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"push_rm32_memory", {0xFFU, 0x75U, 0x18U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"push_rm32_esp_memory", {0xFFU, 0x74U, 0x24U, 0x04U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"push_rm32_absolute_memory",
         {0xFFU, 0x35U, 0x78U, 0x56U, 0x34U, 0x12U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"pushfd", {0x9CU}, 4,
         {ZYDIS_MNEMONIC_PUSHFQ, ZYDIS_MNEMONIC_POP, ZYDIS_MNEMONIC_LEA,
          ZYDIS_MNEMONIC_MOV}},
        {"popfd", {0x9DU}, 4,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_PUSH,
          ZYDIS_MNEMONIC_POPFQ}},
        {"leave", {0xC9U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA}},
        // Task 631. `POP DWORD PTR [EDI+0x14]` and the absolute form. Load the
        // stacked value, raise guest ESP, then store through the guest's own
        // memory operand.
        {"pop_mem_disp8", {0x8FU, 0x47U, 0x14U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        {"pop_mem_disp32", {0x8FU, 0x05U, 0x78U, 0x56U, 0x34U, 0x12U}, 3,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV}},
        // Task 634. The register-array forms. `PUSHAD` captures the entry ESP
        // first, adjusts once, and fills eight slots; `POPAD` restores seven
        // and adjusts last, because the stored ESP is discarded.
        {"pushad", {0x60U}, 10,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA, ZYDIS_MNEMONIC_MOV,
          ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV,
          ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV,
          ZYDIS_MNEMONIC_MOV}},
        {"popad", {0x61U}, 8,
         {ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV,
          ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_MOV,
          ZYDIS_MNEMONIC_MOV, ZYDIS_MNEMONIC_LEA}},
    };

    bool ok = true;
    for (const SeqCase& item : cases)
    {
        const LongModeCompatibilityResult verdict =
            ClassifyLongModeBytes(item.bytes.data(), item.bytes.size());
        std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
        std::size_t count = 0;
        std::size_t instructions = 0;
        bool good = verdict.lowering ==
                repiu::runtime::LongModeLowering::kStackSequence &&
            repiu::runtime::LowerLongModeBytes(item.bytes.data(),
                                               item.bytes.size(), lowered,
                                               &count, &instructions) &&
            instructions == item.instructions;
        // Decode every emitted instruction and compare it with what the
        // sequence table says it should be.
        std::size_t offset = 0;
        for (std::size_t index = 0; good && index < item.mnemonics.size();
             ++index)
        {
            ZydisDecodedInstruction decoded{};
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(
                    &long_mode, nullptr, lowered + offset, count - offset,
                    &decoded)) ||
                decoded.mnemonic != item.mnemonics[index])
            {
                good = false;
                break;
            }
            offset += decoded.length;
        }
        good = good && offset == count;
        if (!good)
        {
            std::cout << "  long_mode_stack_seq_" << item.name << "=false"
                      "\n";
        }
        ok = ok && good;
    }

    // Sign extension, called out on its own because getting it wrong puts a
    // different value on the guest's stack and raises nothing. `6A FF` is
    // `push -1`, so the emitted immediate must be 0xFFFFFFFF.
    const std::uint8_t push_minus_one[] = {0x6AU, 0xFFU};
    std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t count = 0;
    bool sign_ok = repiu::runtime::LowerLongModeBytes(
        push_minus_one, sizeof(push_minus_one), lowered, &count, nullptr) &&
        count >= 4U;
    if (sign_ok)
    {
        const std::uint8_t* immediate = lowered + count - 4U;
        sign_ok = immediate[0] == 0xFFU && immediate[1] == 0xFFU &&
            immediate[2] == 0xFFU && immediate[3] == 0xFFU;
    }

    // Task 631. The store's encoding, byte for byte, because the whole point of
    // the sequence is that the guest's memory operand survives it. `67` for
    // 32-bit addressing, `44` as the REX.R that makes ModRM `reg=110` name R14
    // rather than ESI, then `89` and the guest's own ModRM and displacement.
    const std::uint8_t pop_mem[] = {0x8FU, 0x47U, 0x14U};
    std::uint8_t pop_lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t pop_count = 0;
    const std::uint8_t expected_store[] = {0x67U, 0x44U, 0x89U, 0x77U, 0x14U};
    bool pop_bytes_ok = repiu::runtime::LowerLongModeBytes(
        pop_mem, sizeof(pop_mem), pop_lowered, &pop_count, nullptr) &&
        pop_count >= sizeof(expected_store);
    if (pop_bytes_ok)
    {
        const std::uint8_t* store = pop_lowered + pop_count -
            sizeof(expected_store);
        pop_bytes_ok = std::memcmp(store, expected_store,
                                   sizeof(expected_store)) == 0;
    }

    // Task 669. `PUSH [EBP+0x18]` first loads the source into R14D, then
    // adjusts guest ESP, and finally stores the dword through R15D. The
    // ESP-based form additionally rewrites the source SIB base to R15D.
    const std::uint8_t push_mem[] = {0xFFU, 0x75U, 0x18U};
    const std::uint8_t expected_push_load[] = {
        0x67U, 0x44U, 0x8BU, 0x75U, 0x18U};
    const std::uint8_t expected_push_store[] = {
        0x45U, 0x89U, 0x37U};
    std::uint8_t push_lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t push_count = 0;
    bool push_bytes_ok = repiu::runtime::LowerLongModeBytes(
        push_mem, sizeof(push_mem), push_lowered, &push_count, nullptr) &&
        push_count == 12U &&
        std::memcmp(push_lowered, expected_push_load,
                    sizeof(expected_push_load)) == 0 &&
        std::memcmp(push_lowered + push_count - sizeof(expected_push_store),
                    expected_push_store, sizeof(expected_push_store)) == 0;

    const std::uint8_t push_esp_mem[] = {
        0xFFU, 0x74U, 0x24U, 0x04U};
    const std::uint8_t expected_push_esp_load[] = {
        0x67U, 0x45U, 0x8BU, 0x74U, 0x27U, 0x04U};
    std::uint8_t push_esp_lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t push_esp_count = 0;
    const bool push_esp_bytes_ok = repiu::runtime::LowerLongModeBytes(
        push_esp_mem, sizeof(push_esp_mem), push_esp_lowered,
        &push_esp_count, nullptr) &&
        push_esp_count == 13U &&
        std::memcmp(push_esp_lowered, expected_push_esp_load,
                    sizeof(expected_push_esp_load)) == 0;

    // A 0x67 prefix alone still leaves ModRM rm=101 RIP-relative in long
    // mode. The absolute guest disp32 must therefore be rewritten to SIB.
    const std::uint8_t push_absolute_mem[] = {
        0xFFU, 0x35U, 0x78U, 0x56U, 0x34U, 0x12U};
    const std::uint8_t expected_push_absolute_load[] = {
        0x67U, 0x44U, 0x8BU, 0x34U, 0x25U, 0x78U, 0x56U, 0x34U, 0x12U};
    std::uint8_t push_absolute_lowered[
        repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t push_absolute_count = 0;
    const bool push_absolute_bytes_ok = repiu::runtime::LowerLongModeBytes(
        push_absolute_mem, sizeof(push_absolute_mem), push_absolute_lowered,
        &push_absolute_count, nullptr) &&
        push_absolute_count == 16U &&
        std::memcmp(push_absolute_lowered, expected_push_absolute_load,
                    sizeof(expected_push_absolute_load)) == 0 &&
        std::memcmp(push_absolute_lowered + push_absolute_count -
                        sizeof(expected_push_store),
                    expected_push_store, sizeof(expected_push_store)) == 0;

    // Task 631. The two forms deliberately left as boundaries: an ESP-based
    // destination, whose effective address the SDM computes after the
    // increment, and the operand-size-prefixed `POP m16`.
    const std::uint8_t pop_esp_mem[] = {0x8FU, 0x44U, 0x24U, 0x04U};
    const std::uint8_t pop_mem16[] = {0x66U, 0x8FU, 0x47U, 0x14U};
    const bool pop_refusals_kept =
        ClassifyLongModeBytes(pop_esp_mem, sizeof(pop_esp_mem)).lowering ==
            repiu::runtime::LongModeLowering::kNone &&
        ClassifyLongModeBytes(pop_mem16, sizeof(pop_mem16)).lowering ==
            repiu::runtime::LongModeLowering::kNone;

    // Task 634. `PUSHAD` opens with `mov r14d, r15d` and closes the `+12` slot
    // from that scratch, which is the entry ESP. Read after the `lea` it would
    // be thirty-two lower and the guest would find the wrong value there, and
    // nothing would raise -- so the two bytes that make it the scratch, and the
    // store that spends it, are checked rather than assumed.
    const std::uint8_t pushad_bytes[] = {0x60U};
    std::uint8_t pushad_lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t pushad_count = 0;
    const std::uint8_t expected_capture[] = {0x45U, 0x89U, 0xFEU};
    const std::uint8_t expected_entry_esp_store[] = {0x45U, 0x89U, 0x77U,
                                                     0x0CU};
    bool pushad_ok = repiu::runtime::LowerLongModeBytes(
        pushad_bytes, sizeof(pushad_bytes), pushad_lowered, &pushad_count,
        nullptr) &&
        pushad_count == 39U &&
        std::memcmp(pushad_lowered, expected_capture,
                    sizeof(expected_capture)) == 0;
    if (pushad_ok)
    {
        bool found = false;
        for (std::size_t index = 0;
             index + sizeof(expected_entry_esp_store) <= pushad_count; ++index)
        {
            if (std::memcmp(pushad_lowered + index, expected_entry_esp_store,
                            sizeof(expected_entry_esp_store)) == 0)
            {
                found = true;
                break;
            }
        }
        pushad_ok = found;
    }

    // Still refused, because they change EIP as well as the stack.
    const std::uint8_t ret_bytes[] = {0xC3U};
    const std::uint8_t call_bytes[] = {0xE8U, 0x00U, 0x00U, 0x00U, 0x00U};
    const bool control_still_refused =
        ClassifyLongModeBytes(ret_bytes, sizeof(ret_bytes)).lowering ==
            repiu::runtime::LongModeLowering::kNone &&
        ClassifyLongModeBytes(call_bytes, sizeof(call_bytes)).lowering ==
            repiu::runtime::LongModeLowering::kNone;

    std::cout << "long_mode_stack_sequences=" << (ok ? "true" : "false")
              << ",push_imm8_sign_extended=" << (sign_ok ? "true" : "false")
              << ",control_flow_still_refused="
              << (control_still_refused ? "true" : "false")
              << ",pop_memory_store_encoding="
              << (pop_bytes_ok ? "true" : "false")
              << ",push_rm32_memory_encoding="
              << (push_bytes_ok ? "true" : "false")
              << ",push_rm32_esp_encoding="
              << (push_esp_bytes_ok ? "true" : "false")
              << ",push_rm32_absolute_encoding="
              << (push_absolute_bytes_ok ? "true" : "false")
              << ",pop_memory_refusals_kept="
              << (pop_refusals_kept ? "true" : "false")
              << ",pushad_entry_esp=" << (pushad_ok ? "true" : "false")
              << "\n";
    return ok && sign_ok && control_still_refused && pop_bytes_ok &&
        push_bytes_ok && push_esp_bytes_ok && push_absolute_bytes_ok &&
        pop_refusals_kept && pushad_ok;
}

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

// Task 680. The same opcode has two different source instructions depending
// on the LE code object's default operand size. The mode-aware API must decode
// the three-byte word form and lower it without consuming the next instruction.
bool Probe16BitStackPointerImmediate()
{
    const std::uint8_t guest[] = {0xBCU, 0x00U, 0x20U};
    const std::uint8_t expected[] = {
        0x66U, 0x41U, 0xBFU, 0x00U, 0x20U};
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        guest, sizeof(guest),
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    const bool lowered_ok = repiu::runtime::LowerLongModeBytes(
        guest, sizeof(guest), lowered, &lowered_count, nullptr,
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    const bool ok =
        verdict.compatibility == LongModeByteCompatibility::kNeedsReencode &&
        verdict.divergence == LongModeDivergence::kStackPointerRegister &&
        verdict.lowering ==
            repiu::runtime::LongModeLowering::
                k16BitStackPointerImmediateToR15 &&
        lowered_ok && lowered_count == sizeof(expected) &&
        std::memcmp(lowered, expected, sizeof(expected)) == 0 &&
        ClassifyLongModeBytes(guest, sizeof(guest)).compatibility !=
            LongModeByteCompatibility::kIdenticalBytes;
    std::cout << "long_mode_16bit_stack_pointer_immediate="
              << (ok ? "true" : "false") << ",length="
              << (ok ? 3U : 0U) << ",lowered="
              << (ok ? lowered_count : 0U) << "\n";
    return ok;
}

// Task 681. In a 16-bit code object, explicit 67+66 LEA selects 32-bit
// addressing and a 32-bit destination. The mode-aware classifier must remove
// only the source operand-size override and remap guest ESP when lowering.
bool Probe16BitLea32()
{
    const std::uint8_t guest[] = {
        0x67U, 0x66U, 0x8DU, 0x8CU, 0x24U,
        0x00U, 0xE0U, 0xFFU, 0xFFU,
    };
    const std::uint8_t expected[] = {
        0x67U, 0x41U, 0x8DU, 0x8CU, 0x27U,
        0x00U, 0xE0U, 0xFFU, 0xFFU,
    };
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        guest, sizeof(guest),
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    const bool lowered_ok = repiu::runtime::LowerLongModeBytes(
        guest, sizeof(guest), lowered, &lowered_count, nullptr,
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    const bool ok =
        verdict.compatibility == LongModeByteCompatibility::kNeedsReencode &&
        verdict.divergence == LongModeDivergence::kAddressSize &&
        verdict.lowering ==
            repiu::runtime::LongModeLowering::k16BitLea32ToGuestGprs &&
        lowered_ok && lowered_count == sizeof(expected) &&
        std::memcmp(lowered, expected, sizeof(expected)) == 0;
    std::cout << "long_mode_16bit_lea32=" << (ok ? "true" : "false")
              << ",length=" << (ok ? 9U : 0U)
              << ",lowered=" << (ok ? lowered_count : 0U) << "\n";
    return ok;
}

// Task 682. The runtime frontier is a prefix-free mode16 LEA with a 16-bit
// address calculation. It must be admitted only through the dedicated
// scratch-register lowering, while a BP-based form remains refused.
bool Probe16BitLea16()
{
    const std::uint8_t guest[] = {0x8DU, 0x8CU, 0x24U, 0x00U};
    const std::uint8_t expected[] = {
        0x44U, 0x0FU, 0xB7U, 0xF6U,
        0x67U, 0x66U, 0x41U, 0x8DU, 0x8EU,
        0x24U, 0x00U, 0x00U, 0x00U,
    };
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        guest, sizeof(guest),
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    const bool lowered_ok = repiu::runtime::LowerLongModeBytes(
        guest, sizeof(guest), lowered, &lowered_count, nullptr,
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    const std::uint8_t bp_form[] = {0x8DU, 0x4EU, 0x00U};
    const bool ok =
        verdict.compatibility == LongModeByteCompatibility::kNeedsReencode &&
        verdict.divergence == LongModeDivergence::kAddressSize &&
        verdict.lowering ==
            repiu::runtime::LongModeLowering::k16BitLea16ToGuestGprs &&
        lowered_ok && lowered_count == sizeof(expected) &&
        std::memcmp(lowered, expected, sizeof(expected)) == 0 &&
        ClassifyLongModeBytes(
            bp_form, sizeof(bp_form),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).compatibility !=
            LongModeByteCompatibility::kIdenticalBytes;
    std::cout << "long_mode_16bit_lea16=" << (ok ? "true" : "false")
              << ",length=" << (ok ? 4U : 0U)
              << ",lowered=" << (ok ? lowered_count : 0U) << "\n";
    return ok;
}

// Task 685. A mode16 operand-size override makes TEST a 32-bit register
// operation; removing the override is safe only for the register/no-ESP form.
bool Probe16BitTest32()
{
    const std::uint8_t guest[] = {0x66U, 0x85U, 0xFFU};
    const std::uint8_t expected[] = {0x85U, 0xFFU};
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        guest, sizeof(guest),
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    std::size_t lowered_instructions = 0U;
    const bool lowered_ok = repiu::runtime::LowerLongModeBytes(
        guest, sizeof(guest), lowered, &lowered_count,
        &lowered_instructions,
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    const std::uint8_t no_override[] = {0x85U, 0xFFU};
    const std::uint8_t address_override[] = {
        0x67U, 0x66U, 0x85U, 0xFFU};
    const std::uint8_t memory_form[] = {0x66U, 0x85U, 0x07U};
    const std::uint8_t stack_form[] = {0x66U, 0x85U, 0xE4U};
    const bool unsupported_variants =
        ClassifyLongModeBytes(
            no_override, sizeof(no_override),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).lowering !=
            repiu::runtime::LongModeLowering::k16BitTest32ToGuestGprs &&
        ClassifyLongModeBytes(
            address_override, sizeof(address_override),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).lowering !=
            repiu::runtime::LongModeLowering::k16BitTest32ToGuestGprs &&
        ClassifyLongModeBytes(
            memory_form, sizeof(memory_form),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).lowering !=
            repiu::runtime::LongModeLowering::k16BitTest32ToGuestGprs &&
        ClassifyLongModeBytes(
            stack_form, sizeof(stack_form),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).lowering !=
            repiu::runtime::LongModeLowering::k16BitTest32ToGuestGprs;
    const bool ok =
        verdict.compatibility == LongModeByteCompatibility::kNeedsReencode &&
        verdict.divergence == LongModeDivergence::kOperandWidth &&
        verdict.lowering ==
            repiu::runtime::LongModeLowering::k16BitTest32ToGuestGprs &&
        lowered_ok && lowered_count == sizeof(expected) &&
        lowered_instructions == 1U &&
        std::memcmp(lowered, expected, sizeof(expected)) == 0 &&
        unsupported_variants;
    std::cout << "long_mode_16bit_test32=" << (ok ? "true" : "false")
              << ",length=" << (ok ? 3U : 0U)
              << ",lowered=" << (ok ? lowered_count : 0U)
              << ",unsupported_variants="
              << (unsupported_variants ? "true" : "false") << "\n";
    return ok;
}

// Task 683. LOOPNZ is a control-flow lowering rather than a byte-only
// lowering, because its direct target comes from the translation plan.
// Address-size and opcode variants remain refused until their counter
// semantics have separate proof.
bool Probe16BitLoopNz()
{
    const std::uint8_t guest[] = {0xE0U, 0xFFU};
    const LongModeCompatibilityResult verdict = ClassifyLongModeBytes(
        guest, sizeof(guest),
        repiu::runtime::GuestCodeDefaultOperandSize::k16);
    const bool target_free_lowerer_refuses = [&] {
        std::uint8_t lowered[repiu::runtime::kMaxLoweredBytes] = {};
        std::size_t lowered_count = 0U;
        return !repiu::runtime::LowerLongModeBytes(
            guest, sizeof(guest), lowered, &lowered_count, nullptr,
            repiu::runtime::GuestCodeDefaultOperandSize::k16);
    }();
    const std::uint8_t address_override[] = {0x67U, 0xE0U, 0xFFU};
    const std::uint8_t loopz[] = {0xE1U, 0xFFU};
    const bool unsupported_variants =
        ClassifyLongModeBytes(
            address_override, sizeof(address_override),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).lowering !=
            repiu::runtime::LongModeLowering::k16BitLoopNzToGuestCx &&
        ClassifyLongModeBytes(
            loopz, sizeof(loopz),
            repiu::runtime::GuestCodeDefaultOperandSize::k16).lowering !=
            repiu::runtime::LongModeLowering::k16BitLoopNzToGuestCx;
    constexpr std::uint32_t planner_base = 0x00220000U;
    repiu::runtime::RelocatedRuntimeImage runtime_image;
    runtime_image.valid = true;
    repiu::runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = planner_base;
    object.virtual_size = 16U;
    object.flags = repiu::runtime::kLeObjectExecutable;
    object.memory.assign(object.virtual_size, 0x90U);
    object.memory[0] = 0xE0U;
    object.memory[1] = 0x01U;
    runtime_image.objects.push_back(std::move(object));
    runtime_image.code_mode_ranges.push_back({
        planner_base, 16U, repiu::runtime::kLeObjectExecutable});
    repiu::runtime::AotTranslationPlan plan;
    const bool plan_built = repiu::runtime::BuildAotTranslationPlanFromEntry(
        runtime_image, planner_base, &plan);
    const bool planner_target_rebased = plan_built && !plan.blocks.empty() &&
        !plan.blocks.front().instructions.empty() &&
        plan.blocks.front().instructions.front().kind ==
            repiu::runtime::AotInstructionKind::kConditionalBranch &&
        plan.blocks.front().instructions.front().direct_target ==
            planner_base + 3U;
    const bool ok =
        verdict.compatibility == LongModeByteCompatibility::kNeedsReencode &&
        verdict.divergence == LongModeDivergence::kAddressSize &&
        verdict.lowering ==
            repiu::runtime::LongModeLowering::k16BitLoopNzToGuestCx &&
        target_free_lowerer_refuses && unsupported_variants &&
        planner_target_rebased;
    std::cout << "long_mode_16bit_loopnz=" << (ok ? "true" : "false")
              << ",length=" << (ok ? 2U : 0U)
              << ",target_free_lowerer_refused="
              << (target_free_lowerer_refuses ? "true" : "false")
              << ",planner_target_rebased="
              << (planner_target_rebased ? "true" : "false") << "\n";
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
    const bool stack_seq_ok = ProbeStackSequenceLowering();
    const bool subset_ok = ProbeAdmittedSubset();
    const bool refusals_ok = ProbeRefusals();
    const bool sixteen_bit_ok = Probe16BitStackPointerImmediate();
    const bool sixteen_bit_lea_ok = Probe16BitLea32();
    const bool sixteen_bit_lea16_ok = Probe16BitLea16();
    const bool sixteen_bit_test_ok = Probe16BitTest32();
    const bool sixteen_bit_loopnz_ok = Probe16BitLoopNz();

    const bool all = silent_ok && invalid_ok && width_ok && width_kind_ok &&
        reasons_ok && stack_ok && inc_dec_ok && stack_seq_ok && subset_ok &&
        refusals_ok && sixteen_bit_ok && sixteen_bit_lea_ok &&
        sixteen_bit_lea16_ok && sixteen_bit_test_ok &&
        sixteen_bit_loopnz_ok;
    std::cout << "long_mode_compatibility_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
