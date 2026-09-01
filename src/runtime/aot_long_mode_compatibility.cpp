#include "repiu/runtime/aot_long_mode_compatibility.h"

#include <Zydis.h>

namespace repiu::runtime
{
namespace
{

using Result = LongModeCompatibilityResult;

Result Refuse(const LongModeDivergence divergence)
{
    return Result{LongModeByteCompatibility::kUnsupported, divergence,
                  LongModeLowering::kNone};
}

Result Reencode(const LongModeDivergence divergence,
                const LongModeLowering lowering = LongModeLowering::kNone)
{
    return Result{LongModeByteCompatibility::kNeedsReencode, divergence,
                  lowering};
}

// The one-byte opcodes long mode decodes as something else entirely. These are
// the dangerous ones: nothing raises, and the program runs on doing the wrong
// thing.
//
// `40`-`4F` matters most by volume. In 32-bit code they are `INC`/`DEC` of a
// register, among the most common single-byte instructions there is; in long
// mode every one of them is a REX prefix that modifies the *next* instruction.
// Copying one does not produce a wrong result so much as delete an instruction
// and rewrite its successor.
bool IsSilentlyDifferentOpcode(const std::uint8_t opcode)
{
    // Task 557 removed `40`-`4F` from this list. They are still the single most
    // dangerous range -- every one of them is a REX prefix in long mode -- but
    // they are no longer *refused* for it: `IsIncDecRegisterOpcode` below names
    // the re-encoding that keeps their meaning, and the classifier reaches that
    // before it reaches this list.
    switch (opcode)
    {
        // Task 565 removed `A0`-`A3` from this list, the same way Task 557
        // removed `40`-`4F`. They are still exactly as dangerous -- the offset
        // is four bytes here and eight in long mode, so the instruction's own
        // length changes and every decode after it moves -- but they are no
        // longer *refused* for it: `IsMoffsOpcode` names the re-encoding, and
        // the classifier reaches that first.
        case 0x62U:  // BOUND -> EVEX prefix
        case 0x63U:  // ARPL -> MOVSXD
        case 0xC4U:  // LES -> three-byte VEX prefix
        case 0xC5U:  // LDS -> two-byte VEX prefix
            return true;
        default:
            return false;
    }
}

// Task 557. `INC r32` (`40+r`) and `DEC r32` (`48+r`), the opcode-embedded
// register forms.
//
// These are the range long mode reuses for REX, so they cannot be copied. But
// the operation itself is unchanged in long mode under a different encoding,
// which makes them a rewrite rather than a refusal -- and by volume they are
// the largest thing this classifier was giving up on: 805 of the guest's
// instructions, measured in Task 556.
bool IsIncDecRegisterOpcode(const std::uint8_t opcode)
{
    return opcode >= 0x40U && opcode <= 0x4FU;
}

// Task 565. `MOV`'s moffs forms, whose absolute offset the opcode carries
// directly with no ModRM byte at all.
//
// 681 of the 682 encodings this classifier still called silently different are
// these, which is what put them next: Task 564 moved the obstruction twenty-one
// bytes and this is what it landed on.
bool IsMoffsOpcode(const std::uint8_t opcode)
{
    return opcode >= 0xA0U && opcode <= 0xA3U;
}

// The ModRM-form opcode that means the same thing. `A0`/`A2` move a byte and
// `A1`/`A3` a dword; the low bit of the moffs opcode is that width and the
// second bit is the direction, which is the same layout `88`-`8B` uses.
std::uint8_t MoffsModRmOpcode(const std::uint8_t opcode)
{
    switch (opcode)
    {
        case 0xA0U: return 0x8AU;  // mov al, [disp32]
        case 0xA1U: return 0x8BU;  // mov eax, [disp32]
        case 0xA2U: return 0x88U;  // mov [disp32], al
        case 0xA3U: return 0x89U;  // mov [disp32], eax
        default: return 0x00U;
    }
}

// Removed from long mode. These raise #UD rather than running, which makes them
// the safe half of the refusal list -- but they are still refused by name.
// Catching #UD in a fault handler is an execution strategy someone might choose
// later; it is not a reason to call these bytes compatible now.
bool IsInvalidInLongMode(const std::uint8_t opcode)
{
    switch (opcode)
    {
        case 0x06U: case 0x0EU: case 0x16U: case 0x1EU:  // PUSH ES/CS/SS/DS
        case 0x07U: case 0x17U: case 0x1FU:              // POP ES/SS/DS
        case 0x27U: case 0x2FU:                          // DAA, DAS
        case 0x37U: case 0x3FU:                          // AAA, AAS
        case 0x60U: case 0x61U:                          // PUSHAD, POPAD
        case 0x9AU:                                      // CALL far ptr16:32
        case 0xCEU:                                      // INTO
        case 0xD4U: case 0xD5U:                          // AAM, AAD
        case 0xD6U:                                      // SALC
        case 0xEAU:                                      // JMP far ptr16:32
            return true;
        default:
            return false;
    }
}

// The stack instructions. Long mode gives every one of them a 64-bit operand
// size and offers no way back down: a `66` prefix asks for 16 bits, not 32.
// The meaning survives, so these are re-encodable rather than refused outright
// -- but the x64 emitter has to do the lowering, and until it exists these
// cannot be copied.
//
// `E9`, a `JMP rel32`, is deliberately absent: it touches no stack, and its
// encoding and meaning are the same in both modes.
bool NeedsWidthReencode(const std::uint8_t opcode,
                        const ZydisDecodedInstruction& instruction)
{
    if (opcode >= 0x50U && opcode <= 0x5FU)
    {
        return true;  // PUSH/POP r32 -> r64
    }
    switch (opcode)
    {
        case 0x68U: case 0x6AU:  // PUSH imm32 / imm8
        case 0x8FU:              // POP r/m
        case 0x9CU: case 0x9DU:  // PUSHFD / POPFD
        case 0xC2U: case 0xC3U:  // RET imm16 / RET
        case 0xC9U:              // LEAVE
        case 0xE8U:              // CALL rel32, which pushes eight bytes
            return true;
        case 0xFFU:
            // Only the CALL and JMP extensions of the group; `/0` INC and `/1`
            // DEC through this encoding are ordinary and stay eligible.
            return instruction.raw.modrm.reg == 2U ||
                instruction.raw.modrm.reg == 3U;
        default:
            return false;
    }
}

// Task 559. The stack instructions this unit knows how to rewrite.
//
// Not the whole of `NeedsWidthReencode`: `CALL`, `RET` and the `FF` group also
// change EIP, so they belong with the dispatch resolver rather than here, and
// `PUSH r/m` reaches a second memory operand that may name ESP itself -- the
// general re-encoder's problem.
//
// Prefix-free forms only. `66 50` is `push ax`, a two-byte push with different
// semantics, and admitting it here would lower it as though it moved four.
bool HasStackSequenceLowering(const std::uint8_t opcode,
                              const ZydisDecodedInstruction& instruction)
{
    const std::size_t length = instruction.length;
    if (opcode >= 0x50U && opcode <= 0x5FU)
    {
        return length == 1U;  // PUSH/POP r32, ESP included as a special case
    }
    switch (opcode)
    {
        case 0x68U:
            return length == 5U;  // PUSH imm32
        case 0x6AU:
            return length == 2U;  // PUSH imm8, sign-extended
        case 0x8FU:
            // Only the register form, which is `58+r` spelled differently. The
            // memory form is refused above.
            return length == 2U && instruction.raw.modrm.mod == 3U;
        case 0x9CU:
        case 0x9DU:
        case 0xC9U:
            return length == 1U;  // PUSHFD, POPFD, LEAVE
        default:
            return false;
    }
}

// PUSH/POP FS and GS, the two-byte forms. Long mode keeps them and widens them
// to eight bytes, so they belong with the width group rather than the invalid
// one.
bool NeedsWidthReencodeTwoByte(const std::uint8_t opcode)
{
    switch (opcode)
    {
        case 0xA0U: case 0xA8U:  // PUSH FS / PUSH GS
        case 0xA1U: case 0xA9U:  // POP FS / POP GS
            return true;
        default:
            return false;
    }
}

bool TouchesSegmentRegister(const ZydisDecodedInstruction& instruction,
                            const ZydisDecodedOperand* operands)
{
    for (std::uint8_t index = 0; index < instruction.operand_count; ++index)
    {
        if (operands[index].type != ZYDIS_OPERAND_TYPE_REGISTER)
        {
            continue;
        }
        switch (operands[index].reg.value)
        {
            case ZYDIS_REGISTER_ES:
            case ZYDIS_REGISTER_CS:
            case ZYDIS_REGISTER_SS:
            case ZYDIS_REGISTER_DS:
            case ZYDIS_REGISTER_FS:
            case ZYDIS_REGISTER_GS:
                return true;
            default:
                break;
        }
    }
    return false;
}

bool IsStackPointerRegister(const ZydisRegister reg)
{
    switch (reg)
    {
        case ZYDIS_REGISTER_RSP:
        case ZYDIS_REGISTER_ESP:
        case ZYDIS_REGISTER_SP:
        case ZYDIS_REGISTER_SPL:
            return true;
        default:
            return false;
    }
}

// Task 555. Whether the instruction names the stack pointer in any role.
//
// Task 546's decision 3 keeps host RSP as the SysV stack and holds guest ESP as
// state, so in long mode `ESP` is the low half of the host's stack pointer.
// Copying or prefixing anything that names it does not address the guest's
// stack -- it reaches the host's.
//
// Both roles are checked because the two failures differ in kind.
// `mov eax,[esp+8]` has a memory operand and would have been lowered with a
// prefix, yielding wrong data. `add esp,16` has none, so it never reaches the
// memory path at all and passed as `kIdenticalBytes` -- and writing `ESP` in
// long mode zero-extends into `RSP` and destroys the host stack pointer. The
// second is why this check sits ahead of the memory-operand judgement rather
// than inside it.
//
// Every operand is scanned, hidden ones included: the implicit stack operands
// belong to instructions the opcode lists already refuse, but a check that
// depended on that ordering would be one edit away from being wrong.
// Task 564. Which encoding fields name `ESP`, and whether all of them are ones
// this unit can rewrite.
//
// `ESP` can appear in three places, and Zydis's `operand.encoding` says which:
// ModRM `reg`, ModRM `rm`, and the SIB base. All three become `R15D` the same
// way -- the field goes to `111` and the matching `REX` bit is set -- so the
// work is deciding which bits, not how.
//
// What is deliberately not admitted is a register embedded in the opcode
// (`push esp`, `inc esp`): those are Task 559's and Task 557's, and a second
// path to them here would be a second place to get them wrong.
struct StackPointerFields
{
    bool rex_r = false;
    bool rex_b = false;
    bool supported = false;
};

StackPointerFields ClassifyStackPointerFields(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands)
{
    StackPointerFields fields;
    if ((instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) == 0U)
    {
        return fields;
    }
    for (std::uint8_t index = 0; index < instruction.operand_count; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            IsStackPointerRegister(operand.reg.value))
        {
            if (operand.encoding == ZYDIS_OPERAND_ENCODING_MODRM_REG)
            {
                fields.rex_r = true;
                continue;
            }
            if (operand.encoding == ZYDIS_OPERAND_ENCODING_MODRM_RM &&
                instruction.raw.modrm.mod == 3U)
            {
                fields.rex_b = true;
                continue;
            }
            // Embedded in the opcode, or an implicit operand. Not this unit's.
            return StackPointerFields{};
        }
        if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY)
        {
            continue;
        }
        if (IsStackPointerRegister(operand.mem.index))
        {
            // Unreachable by encoding -- `index=100` is "no index" -- and
            // refused rather than assumed away.
            return StackPointerFields{};
        }
        if (!IsStackPointerRegister(operand.mem.base))
        {
            continue;
        }
        // A base of `ESP` is only expressible through a SIB byte, so one has to
        // be there for this to be the shape it looks like.
        if ((instruction.attributes & ZYDIS_ATTRIB_HAS_SIB) == 0U ||
            instruction.raw.modrm.rm != 4U)
        {
            return StackPointerFields{};
        }
        fields.rex_b = true;
    }
    fields.supported = fields.rex_r || fields.rex_b;
    return fields;
}

bool CanReencodeStackPointer(const ZydisDecodedInstruction& instruction,
                             const ZydisDecodedOperand* operands)
{
    return ClassifyStackPointerFields(instruction, operands).supported;
}

bool TouchesStackPointer(const ZydisDecodedInstruction& instruction,
                         const ZydisDecodedOperand* operands)
{
    for (std::uint8_t index = 0; index < instruction.operand_count; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            IsStackPointerRegister(operand.reg.value))
        {
            return true;
        }
        if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY)
        {
            continue;
        }
        // `ESP` cannot be a SIB index -- that encoding is what "no index"
        // means -- so the index arm completes the check rather than catching a
        // case that occurs.
        if (IsStackPointerRegister(operand.mem.base) ||
            IsStackPointerRegister(operand.mem.index))
        {
            return true;
        }
    }
    return false;
}

bool HasMemoryOperand(const ZydisDecodedInstruction& instruction,
                      const ZydisDecodedOperand* operands)
{
    for (std::uint8_t index = 0; index < instruction.operand_count; ++index)
    {
        if (operands[index].type == ZYDIS_OPERAND_TYPE_MEMORY)
        {
            return true;
        }
    }
    return false;
}

// ModRM mod=00, rm=101. In 32-bit mode that is an absolute `disp32` with no
// base register; in long mode the same bits mean RIP-relative.
//
// It is checked apart from the opcode lists because it is a property of the
// addressing form, so no list of opcodes catches it -- and apart from the plain
// memory-operand check because of what it costs. Reading a global by absolute
// address is what compiled 32-bit code does constantly, so this single
// divergence is spread through the whole program rather than confined to a few
// instructions.
bool IsAbsoluteDisplacementForm(const ZydisDecodedInstruction& instruction)
{
    return (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) != 0U &&
        instruction.raw.modrm.mod == 0U && instruction.raw.modrm.rm == 5U;
}

}  // namespace

LongModeCompatibilityResult ClassifyLongModeBytes(
    const std::uint8_t* const bytes, const std::size_t byte_count)
{
    if (bytes == nullptr || byte_count == 0U)
    {
        return Refuse(LongModeDivergence::kNone);
    }

    // The guest's ISA is the source, so the decode stays LEGACY_32. This asks
    // what these bytes mean where they came from; what they would mean in long
    // mode is the judgement below, and decoding them as 64-bit would answer a
    // different question.
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                                       ZYDIS_STACK_WIDTH_32)))
    {
        return Refuse(LongModeDivergence::kNone);
    }

    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, bytes, byte_count,
                                             &instruction, operands)))
    {
        // Not an instruction is an answer, and the answer is no.
        return Refuse(LongModeDivergence::kNone);
    }

    const std::uint8_t opcode = instruction.opcode;

    if (instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT)
    {
        if (IsIncDecRegisterOpcode(opcode))
        {
            // `inc esp` lowers to `FF C4`, which writes the host's stack
            // pointer -- the hole Task 555 closed. This judgement returns from
            // inside the opcode-map block and so never reaches the
            // stack-pointer check further down, which is why the check is
            // called here rather than duplicated as `rm == 4`.
            if (TouchesStackPointer(instruction, operands))
            {
                return Refuse(LongModeDivergence::kStackPointerRegister);
            }
            // Only the bare byte. A prefixed form (`66 40` is `inc ax`) could
            // be lowered the same way, but Task 557 keeps what it proves to the
            // smallest thing, and the census measures whether the restriction
            // costs anything.
            if (instruction.length != 1U)
            {
                return Refuse(LongModeDivergence::kSilentlyDifferent);
            }
            return Reencode(LongModeDivergence::kSilentlyDifferent,
                            LongModeLowering::kIncDecToModRm);
        }
        // Task 565. Ahead of the refusal list because these were on it.
        //
        // Two lengths: the bare five-byte form, and six bytes when an
        // operand-size `66` makes it `mov ax, moffs`. The first attempt admitted
        // only the bare one, on Task 557's policy of proving the smallest thing
        // and letting the census say what the restriction costs -- and the
        // census answered at once. The instruction still blocking the entry
        // chain was `66 A3 disp32`, one of the 216 the restriction had left
        // behind, so the restriction cost exactly the case that mattered.
        //
        // Anything else stays refused. `67 A1` is a different instruction --
        // the address-size prefix makes the offset sixteen bits.
        if (IsMoffsOpcode(opcode))
        {
            const bool bare = instruction.length == 5U;
            const bool operand_size = instruction.length == 6U &&
                bytes[0] == 0x66U;
            if (!bare && !operand_size)
            {
                return Refuse(LongModeDivergence::kSilentlyDifferent);
            }
            return Reencode(LongModeDivergence::kSilentlyDifferent,
                            LongModeLowering::kMoffsToSib);
        }
        if (IsSilentlyDifferentOpcode(opcode))
        {
            return Refuse(LongModeDivergence::kSilentlyDifferent);
        }
        if (IsInvalidInLongMode(opcode))
        {
            return Refuse(LongModeDivergence::kInvalidInLongMode);
        }
        if (NeedsWidthReencode(opcode, instruction))
        {
            return Reencode(LongModeDivergence::kOperandWidth,
                            HasStackSequenceLowering(opcode, instruction)
                                ? LongModeLowering::kStackSequence
                                : LongModeLowering::kNone);
        }
    }
    else if (instruction.opcode_map == ZYDIS_OPCODE_MAP_0F)
    {
        if (NeedsWidthReencodeTwoByte(opcode))
        {
            return Reencode(LongModeDivergence::kOperandWidth);
        }
    }
    else
    {
        // Every remaining map arrives through a byte long mode reads as a VEX,
        // EVEX, or XOP prefix, so the encoding itself is in question before its
        // meaning is.
        return Refuse(LongModeDivergence::kSilentlyDifferent);
    }

    // Anything that still writes the instruction pointer is control flow, and
    // control flow on x64 belongs to the dispatch frame rather than to copied
    // bytes.
    if (instruction.meta.category == ZYDIS_CATEGORY_CALL ||
        instruction.meta.category == ZYDIS_CATEGORY_RET ||
        instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR ||
        instruction.meta.category == ZYDIS_CATEGORY_COND_BR ||
        instruction.meta.category == ZYDIS_CATEGORY_INTERRUPT ||
        instruction.meta.category == ZYDIS_CATEGORY_SYSCALL ||
        instruction.meta.category == ZYDIS_CATEGORY_SYSRET)
    {
        return Reencode(LongModeDivergence::kOperandWidth);
    }

    if (TouchesSegmentRegister(instruction, operands))
    {
        return Refuse(LongModeDivergence::kSegmentRegister);
    }

    // Task 555, and Task 564 for what happens after it. Ahead of the
    // memory-operand judgement below, because the worse of the two shapes has
    // no memory operand: `add esp,16` was reaching the `kIdenticalBytes` return
    // at the bottom of this function.
    //
    // The refusal was right; what was missing was the re-encoding. Guest ESP is
    // R15D, so an instruction naming ESP in ModRM or SIB can name R15D instead,
    // and Task 563 measured that these are what hold execution to one block.
    if (TouchesStackPointer(instruction, operands))
    {
        if (CanReencodeStackPointer(instruction, operands))
        {
            return Reencode(LongModeDivergence::kStackPointerRegister,
                            LongModeLowering::kStackPointerToR15);
        }
        return Refuse(LongModeDivergence::kStackPointerRegister);
    }

    // Task 552. A memory operand is now a rewrite rather than a refusal,
    // because Task 546's decision 4 is settled: guest memory is placed below
    // 4 GiB, so a 32-bit address computation zero-extended to 64 bits names the
    // address the guest meant. Task 551 measured that placement rather than
    // assuming it.
    //
    // A segment override is not covered by that and stays refused. `FS` and
    // `GS` have real bases in long mode but host TLS is using `FS`, and
    // decision 5 says raw guest segments are never installed into host `FS` or
    // `GS`; the other overrides are ignored in long mode. Neither is a problem
    // an address-size prefix moves.
    if (const bool has_memory = HasMemoryOperand(instruction, operands);
        has_memory)
    {
        if ((instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) != 0U)
        {
            return Refuse(LongModeDivergence::kSegmentRegister);
        }
        // Checked apart from the ordinary case, and lowered differently: a
        // prefix does not turn RIP-relative off, only truncate it, so this form
        // needs its ModRM rewritten into the SIB absolute encoding as well.
        if (IsAbsoluteDisplacementForm(instruction))
        {
            return Reencode(LongModeDivergence::kRipRelativeDisplacement,
                            LongModeLowering::kAbsoluteToSib);
        }
        return Reencode(LongModeDivergence::kAddressSize,
                        LongModeLowering::kAddressSizePrefix);
    }

    // No memory operand, so the addressing form cannot be the absolute one; a
    // bare `mod=00 rm=101` without a memory operand is not a form this reaches.
    if (IsAbsoluteDisplacementForm(instruction))
    {
        return Refuse(LongModeDivergence::kRipRelativeDisplacement);
    }

    // A privileged instruction is refused rather than judged. Whether it means
    // the same thing is not the interesting question about it.
    if ((instruction.attributes & ZYDIS_ATTRIB_IS_PRIVILEGED) != 0U)
    {
        return Refuse(LongModeDivergence::kNone);
    }

    // What is left is register-only work at 8, 16, or 32 bits: the ordinary
    // GPR and flags subset Task 546 named, and the only thing this unit is
    // willing to call identical.
    if (instruction.operand_width != 8U && instruction.operand_width != 16U &&
        instruction.operand_width != 32U)
    {
        return Refuse(LongModeDivergence::kOperandWidth);
    }

    return Result{LongModeByteCompatibility::kIdenticalBytes,
                  LongModeDivergence::kNone, LongModeLowering::kNone};
}

namespace
{

// Task 559. The pieces every stack sequence is built from.
//
// Guest ESP is R15D and the emitter's scratch is R14D (Task 558, and Task 559
// for the scratch). Each helper appends one instruction and is named for what
// it emits, so the sequences below read as the assembly they are.
struct SequenceWriter
{
    std::uint8_t* out = nullptr;
    std::size_t written = 0;
    std::size_t instructions = 0;

    void Byte(const std::uint8_t value)
    {
        out[written++] = value;
    }

    // lea r15d, [r15 + disp8]. A LEA and never an ADD or SUB: guest PUSH and
    // POP change no flags, and the guest's next branch reads them.
    void AdjustGuestEsp(const std::int8_t displacement)
    {
        Byte(0x45U);  // REX.R (r15 as reg) + REX.B (r15 as base)
        Byte(0x8DU);
        Byte(0x7FU);  // mod=01, reg=111 (r15), rm=111 (r15)
        Byte(static_cast<std::uint8_t>(displacement));
        ++instructions;
    }

    // mov [r15], r32
    void StoreGuestRegister(const std::uint8_t reg)
    {
        Byte(0x41U);  // REX.B for r15 as the base
        Byte(0x89U);
        Byte(static_cast<std::uint8_t>(0x07U | (reg << 3U)));
        ++instructions;
    }

    // mov r32, [r15]
    void LoadGuestRegister(const std::uint8_t reg)
    {
        Byte(0x41U);
        Byte(0x8BU);
        Byte(static_cast<std::uint8_t>(0x07U | (reg << 3U)));
        ++instructions;
    }

    // mov dword ptr [r15], imm32
    void StoreImmediate(const std::uint32_t value)
    {
        Byte(0x41U);
        Byte(0xC7U);
        Byte(0x07U);
        Byte(static_cast<std::uint8_t>(value));
        Byte(static_cast<std::uint8_t>(value >> 8U));
        Byte(static_cast<std::uint8_t>(value >> 16U));
        Byte(static_cast<std::uint8_t>(value >> 24U));
        ++instructions;
    }

    // One of the fixed extended-register moves, given its ModRM byte.
    void ExtendedMove(const std::uint8_t rex, const std::uint8_t opcode,
                      const std::uint8_t modrm)
    {
        Byte(rex);
        Byte(opcode);
        Byte(modrm);
        ++instructions;
    }

    void Single(const std::uint8_t opcode)
    {
        Byte(opcode);
        ++instructions;
    }

    void Pair(const std::uint8_t first, const std::uint8_t second)
    {
        Byte(first);
        Byte(second);
        ++instructions;
    }
};

// The stack sequences. Returns false for anything the classifier named but this
// does not build, which would be a disagreement between the two rather than an
// ordinary refusal -- so it is written to be impossible rather than handled.
bool WriteStackSequence(const std::uint8_t* const bytes,
                        const ZydisDecodedInstruction& instruction,
                        SequenceWriter* const writer)
{
    const std::uint8_t opcode = instruction.opcode;
    if (opcode >= 0x50U && opcode <= 0x5FU)
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(opcode & 0x07U);
        const bool is_pop = (opcode & 0x08U) != 0U;
        if (reg == 4U)
        {
            // ESP is not in a host GPR, so these two cannot go through the
            // general path.
            if (is_pop)
            {
                // pop esp: the loaded value overrides the increment, so the
                // whole instruction is one load. mov r15d, [r15]
                writer->ExtendedMove(0x45U, 0x8BU, 0x3FU);
                return true;
            }
            // push esp pushes ESP *as it was before the decrement*, so the
            // value has to be kept before adjusting. mov r14d, r15d
            writer->ExtendedMove(0x45U, 0x89U, 0xFEU);
            writer->AdjustGuestEsp(-4);
            writer->ExtendedMove(0x45U, 0x89U, 0x37U);  // mov [r15], r14d
            return true;
        }
        if (is_pop)
        {
            writer->LoadGuestRegister(reg);
            writer->AdjustGuestEsp(4);
            return true;
        }
        writer->AdjustGuestEsp(-4);
        writer->StoreGuestRegister(reg);
        return true;
    }

    switch (opcode)
    {
        case 0x68U:
        {
            std::uint32_t immediate = 0U;
            for (std::size_t index = 0; index < 4U; ++index)
            {
                immediate |= static_cast<std::uint32_t>(bytes[1U + index])
                    << (8U * index);
            }
            writer->AdjustGuestEsp(-4);
            writer->StoreImmediate(immediate);
            return true;
        }
        case 0x6AU:
        {
            // Sign-extended, which is the whole content of this case. Pushing
            // `-1` as 0x000000FF would put a different value on the guest's
            // stack and raise nothing.
            const auto immediate = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(
                    static_cast<std::int8_t>(bytes[1])));
            writer->AdjustGuestEsp(-4);
            writer->StoreImmediate(immediate);
            return true;
        }
        case 0x8FU:
        {
            const std::uint8_t reg = instruction.raw.modrm.rm;
            if (reg == 4U)
            {
                writer->ExtendedMove(0x45U, 0x8BU, 0x3FU);
                return true;
            }
            writer->LoadGuestRegister(reg);
            writer->AdjustGuestEsp(4);
            return true;
        }
        case 0x9CU:
            // Long mode has no 32-bit PUSHFD and no way to move flags to a
            // register, so the host stack is a balanced temporary.
            writer->Single(0x9CU);                      // pushfq
            writer->Pair(0x41U, 0x5EU);                 // pop r14
            writer->AdjustGuestEsp(-4);
            writer->ExtendedMove(0x45U, 0x89U, 0x37U);  // mov [r15], r14d
            return true;
        case 0x9DU:
            writer->ExtendedMove(0x45U, 0x8BU, 0x37U);  // mov r14d, [r15]
            writer->AdjustGuestEsp(4);
            writer->Pair(0x41U, 0x56U);                 // push r14
            writer->Single(0x9DU);                      // popfq
            return true;
        case 0xC9U:
            // leave is `mov esp, ebp` then `pop ebp`.
            writer->ExtendedMove(0x41U, 0x89U, 0xEFU);  // mov r15d, ebp
            writer->ExtendedMove(0x41U, 0x8BU, 0x2FU);  // mov ebp, [r15]
            writer->AdjustGuestEsp(4);
            return true;
        default:
            return false;
    }
}

}  // namespace

bool LowerLongModeBytes(const std::uint8_t* const bytes,
                        const std::size_t byte_count,
                        std::uint8_t* const lowered,
                        std::size_t* const lowered_count,
                        std::size_t* const instruction_count)
{
    if (bytes == nullptr || lowered == nullptr || lowered_count == nullptr)
    {
        return false;
    }
    *lowered_count = 0U;
    if (instruction_count != nullptr)
    {
        *instruction_count = 0U;
    }

    const LongModeCompatibilityResult verdict =
        ClassifyLongModeBytes(bytes, byte_count);
    if (verdict.lowering == LongModeLowering::kNone)
    {
        return false;
    }

    // Decoded a second time rather than threaded out of the classifier. The
    // cost is one decode on a path that emits, and what it buys is that the
    // classifier's answer stays a judgement about bytes rather than a carrier
    // for a decode nobody else can check.
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                                       ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, bytes, byte_count,
                                             &instruction, operands)))
    {
        return false;
    }

    const std::size_t length = instruction.length;
    if (verdict.lowering == LongModeLowering::kAddressSizePrefix)
    {
        if (length + 1U > kMaxLoweredBytes)
        {
            return false;
        }
        lowered[0] = 0x67U;
        for (std::size_t index = 0; index < length; ++index)
        {
            lowered[index + 1U] = bytes[index];
        }
        *lowered_count = length + 1U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 565. The moffs form rewritten into the SIB absolute form.
    //
    // The destination is the one `kAbsoluteToSib` already produces; only the
    // starting encoding differs, since moffs carries its offset straight after
    // the opcode with no ModRM. So the opcode is exchanged for the ModRM-form
    // one and the displacement is copied unchanged -- it is already the
    // absolute guest address, and the `0x67` in front is what keeps it
    // zero-extended rather than sign-extended.
    if (verdict.lowering == LongModeLowering::kMoffsToSib)
    {
        const std::uint8_t modrm_opcode = MoffsModRmOpcode(instruction.opcode);
        // The offset is always the last four bytes and the opcode the one
        // before them, which locates both without assuming how many prefixes
        // came first or in what order.
        if (modrm_opcode == 0U || (length != 5U && length != 6U) ||
            length + 3U > kMaxLoweredBytes)
        {
            return false;
        }
        const std::size_t opcode_index = length - 5U;
        std::size_t out = 0U;
        if (opcode_index == 1U)
        {
            if (bytes[0] != 0x66U)
            {
                return false;
            }
            // The operand-size prefix is kept: it is what makes this a 16-bit
            // move, and the rewrite changes the addressing rather than the
            // width.
            lowered[out++] = 0x66U;
        }
        lowered[out++] = 0x67U;
        lowered[out++] = modrm_opcode;
        lowered[out++] = 0x04U;  // mod=00, reg=000 (AL/AX/EAX), rm=100 (SIB)
        lowered[out++] = 0x25U;  // scale=0, index=100 (none), base=101 (disp32)
        for (std::size_t index = 0; index < 4U; ++index)
        {
            lowered[out++] = bytes[length - 4U + index];
        }
        *lowered_count = out;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 564. The same instruction with `R15D` where `ESP` was.
    //
    // A `REX` is inserted between the legacy prefixes and the opcode -- guest
    // code is 32-bit so it can never already carry one -- and the field naming
    // `ESP` goes to `111`. Nothing else moves: same opcode, same displacement,
    // same immediate, one byte longer.
    if (verdict.lowering == LongModeLowering::kStackPointerToR15)
    {
        const StackPointerFields fields =
            ClassifyStackPointerFields(instruction, operands);
        const std::size_t opcode_offset = instruction.raw.modrm.offset > 0U
            ? static_cast<std::size_t>(instruction.raw.modrm.offset) - 1U
            : 0U;
        if (!fields.supported || length + 1U > kMaxLoweredBytes ||
            instruction.raw.modrm.offset == 0U ||
            opcode_offset >= length)
        {
            return false;
        }
        // Everything before the opcode is a legacy prefix and is copied as it
        // is. Zydis reports the ModRM byte's offset, and in every form this
        // unit admits the opcode is the byte before it -- a two-byte opcode map
        // never reaches here, because `ESP` in ModRM or SIB is a one-byte-opcode
        // shape and anything else was refused above.
        if (instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT)
        {
            return false;
        }
        std::size_t out = 0U;
        for (std::size_t index = 0; index < opcode_offset; ++index)
        {
            lowered[out++] = bytes[index];
        }
        std::uint8_t rex = 0x40U;
        rex |= fields.rex_r ? 0x04U : 0x00U;
        rex |= fields.rex_b ? 0x01U : 0x00U;
        lowered[out++] = rex;
        for (std::size_t index = opcode_offset; index < length; ++index)
        {
            lowered[out++] = bytes[index];
        }

        // The ModRM and SIB bytes moved by one when the `REX` went in.
        const std::size_t modrm_out =
            static_cast<std::size_t>(instruction.raw.modrm.offset) + 1U;
        if (fields.rex_r)
        {
            // reg = 111
            lowered[modrm_out] = static_cast<std::uint8_t>(
                (lowered[modrm_out] & 0xC7U) | 0x38U);
        }
        if (fields.rex_b)
        {
            if (instruction.raw.modrm.mod == 3U)
            {
                // rm = 111, the register form.
                lowered[modrm_out] = static_cast<std::uint8_t>(
                    (lowered[modrm_out] & 0xF8U) | 0x07U);
            }
            else
            {
                // SIB base = 111. The SIB byte follows ModRM.
                const std::size_t sib_out = modrm_out + 1U;
                if (sib_out >= out)
                {
                    return false;
                }
                lowered[sib_out] = static_cast<std::uint8_t>(
                    (lowered[sib_out] & 0xF8U) | 0x07U);
            }
        }
        *lowered_count = out;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 559. Several instructions rather than one, which is why the count is
    // reported at all.
    if (verdict.lowering == LongModeLowering::kStackSequence)
    {
        SequenceWriter writer{lowered, 0U, 0U};
        if (!WriteStackSequence(bytes, instruction, &writer) ||
            writer.written > kMaxLoweredBytes)
        {
            return false;
        }
        *lowered_count = writer.written;
        if (instruction_count != nullptr)
        {
            *instruction_count = writer.instructions;
        }
        return true;
    }

    // Task 557. `40+r` and `48+r` carry their register in the low three bits of
    // the opcode. The group form puts it in ModRM's `rm` with `mod=11`
    // (register direct) and uses `reg` as the extension that picks the
    // operation: `/0` is INC, `/1` is DEC. The classifier admits only the bare
    // byte, so there is nothing before or after the opcode to carry over.
    if (verdict.lowering == LongModeLowering::kIncDecToModRm)
    {
        if (length != 1U || kMaxLoweredBytes < 2U)
        {
            return false;
        }
        const std::uint8_t opcode = bytes[0];
        const std::uint8_t reg = static_cast<std::uint8_t>(opcode & 0x07U);
        const std::uint8_t extension =
            static_cast<std::uint8_t>((opcode & 0x08U) != 0U ? 1U : 0U);
        lowered[0] = 0xFFU;
        lowered[1] = static_cast<std::uint8_t>(
            0xC0U | (extension << 3U) | reg);
        *lowered_count = 2U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // kAbsoluteToSib. The ModRM byte keeps its `reg` field and its `mod`, and
    // its `rm` becomes 100 to say "a SIB byte follows"; the SIB then says
    // base=101 with index=100, which is the encoding for a bare disp32 with no
    // base and no index. The displacement itself is copied unchanged -- it is
    // already the absolute guest address, and the 0x67 in front is what keeps
    // it zero-extended rather than sign-extended.
    const std::size_t modrm_offset = instruction.raw.modrm.offset;
    if (modrm_offset >= length || length + 2U > kMaxLoweredBytes)
    {
        return false;
    }
    // Four displacement bytes follow the ModRM byte in this form, and nothing
    // may sit between them.
    if (modrm_offset + 1U + 4U != length)
    {
        return false;
    }

    std::size_t out = 0U;
    lowered[out++] = 0x67U;
    for (std::size_t index = 0; index < modrm_offset; ++index)
    {
        lowered[out++] = bytes[index];
    }
    const std::uint8_t modrm = bytes[modrm_offset];
    lowered[out++] = static_cast<std::uint8_t>((modrm & 0xF8U) | 0x04U);
    lowered[out++] = 0x25U;  // SIB: scale=0, index=100, base=101
    for (std::size_t index = 0; index < 4U; ++index)
    {
        lowered[out++] = bytes[modrm_offset + 1U + index];
    }
    *lowered_count = out;
    if (instruction_count != nullptr)
    {
        *instruction_count = 1U;
    }
    return true;
}

}  // namespace repiu::runtime
