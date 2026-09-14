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

// Task 674. `MOV r32, imm32` embeds the destination register in the opcode,
// so `MOV ESP, imm32` has no ModRM field for the ordinary stack-pointer
// re-encoder to rewrite. This is the exact bare form emitted by the legacy
// guest path; prefixed forms remain subject to the normal fail-closed rules.
bool IsMovStackPointerImmediate(const ZydisDecodedInstruction& instruction)
{
    return instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        instruction.opcode == 0xBCU && instruction.length == 5U &&
        instruction.operand_width == 32U && instruction.raw.prefix_count == 0U;
}

// Task 680. In a 16-bit code object the same opcode embeds a word immediate
// and names SP rather than ESP. Keep the form narrow until the other 16-bit
// operand and stack semantics have their own proven lowerings.
bool IsMovStackPointerImmediate16(
    const ZydisDecodedInstruction& instruction)
{
    return instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        instruction.opcode == 0xBCU && instruction.length == 3U &&
        instruction.operand_width == 16U && instruction.raw.prefix_count == 0U;
}

// Task 687. In a mode16 code object, prefix-free `B8+r iw` writes a 16-bit
// GPR. Exclude `BC iw` because opcode register 4 is guest SP and has its own
// R15W lowering that must not write the host stack pointer.
bool IsMode16MovImmediate(const ZydisDecodedInstruction& instruction)
{
    return instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        instruction.opcode >= 0xB8U && instruction.opcode <= 0xBFU &&
        instruction.mnemonic == ZYDIS_MNEMONIC_MOV &&
        instruction.length == 3U && instruction.operand_width == 16U &&
        instruction.address_width == 16U &&
        instruction.raw.prefix_count == 0U &&
        instruction.opcode != 0xBCU;
}

// Task 688. Prefix-free mode16 register-only `89/8B /r` moves 16-bit guest
// GPRs. Reject ModRM register 4 because it names guest SP, whose host mapping
// is R15 rather than RSP.
bool IsMode16MovRegister(const ZydisDecodedInstruction& instruction)
{
    return instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        (instruction.opcode == 0x89U || instruction.opcode == 0x8BU) &&
        instruction.mnemonic == ZYDIS_MNEMONIC_MOV &&
        instruction.length == 2U && instruction.operand_width == 16U &&
        instruction.address_width == 16U &&
        instruction.raw.prefix_count == 0U &&
        (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) != 0U &&
        instruction.raw.modrm.offset == 1U &&
        instruction.raw.modrm.mod == 3U &&
        instruction.raw.modrm.reg != 4U &&
        instruction.raw.modrm.rm != 4U;
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
            // `/2` and `/3` are CALL/JMP and `/6` is PUSH. The other group
            // extensions are ordinary operations and stay eligible.
            return instruction.raw.modrm.reg == 2U ||
                instruction.raw.modrm.reg == 3U ||
                instruction.raw.modrm.reg == 6U;
        default:
            return false;
    }
}

// Task 631. The `POP r/m32` memory form this unit will rewrite.
//
// The register form is `58+r` spelled differently and was always here. The
// memory form was refused, which made it an HLE boundary, and the boundary path
// single-steps the guest instruction at its guest address -- executing a
// 32-bit `POP m32` as a 64-bit one, off the host stack, leaving guest ESP
// untouched. The guest's own `POP DWORD PTR [EDI+0x14]` is where that was
// found.
//
// Three forms stay refused rather than guessed:
//
//  - Anything with a prefix. `66` is `POP m16` with a different stack width,
//    and a segment override needs the guest's segment HLE, not this.
//  - A destination addressed through ESP. The SDM computes that effective
//    address *after* the increment, so its ordering differs from the sequence
//    written below. An operand that does not name ESP has an address that does
//    not depend on it, which is what makes load-then-raise-then-store safe.
//  - `mod == 3`, which is the register form and belongs to the case above.
bool PopMemoryFormLowerable(const ZydisDecodedInstruction& instruction)
{
    if (instruction.raw.prefix_count != 0U ||
        instruction.raw.modrm.mod == 3U)
    {
        return false;
    }
    // `rm == 100` is the SIB escape; every other `rm` names a base register
    // directly, and ESP cannot be spelled without a SIB.
    if (instruction.raw.modrm.rm == 4U)
    {
        return instruction.raw.sib.base != 4U;
    }
    return true;
}

// Task 559. The stack instructions this unit knows how to rewrite.
//
// Not the whole of `NeedsWidthReencode`: `CALL`, `RET` and the control-flow
// members of the `FF` group also change EIP, so they belong with the dispatch
// resolver rather than here. `PUSH r/m32` is handled below as a stack sequence;
// its source memory operand is loaded before guest ESP is adjusted.
//
// Task 606 also admits a single operand-size prefix on register PUSH/POP.
// Its two-byte stack effect is emitted explicitly below.
bool HasStackSequenceLowering(const std::uint8_t opcode,
                              const ZydisDecodedInstruction& instruction)
{
    const std::size_t length = instruction.length;
    if (opcode >= 0x50U && opcode <= 0x5FU)
    {
        return length == 1U ||
            (length == 2U && instruction.operand_width == 16U &&
             instruction.raw.prefix_count == 1U &&
             instruction.raw.prefixes[0].value == 0x66U);
    }
    switch (opcode)
    {
        case 0x60U:
        case 0x61U:
            // The 32-bit forms only. A `0x66` prefix makes them `PUSHA` and
            // `POPA` over sixteen-bit halves, which is a different stack
            // layout and not a form the measured stops ask for.
            return length == 1U;
        case 0x68U:
            return length == 5U;  // PUSH imm32
        case 0x6AU:
            return length == 2U;  // PUSH imm8, sign-extended
        case 0x8FU:
            // The register form is `58+r` spelled differently. Task 631 added
            // the memory form, under the conditions the helper above names.
            return (length == 2U && instruction.raw.modrm.mod == 3U) ||
                PopMemoryFormLowerable(instruction);
        case 0xFFU:
            // Task 669. Only the bare 32-bit PUSH member of the FF group. The
            // CALL/JMP members are resolved as control flow, and prefixed PUSH
            // forms remain boundaries until their width/address semantics are
            // separately proven.
            return length >= 2U && instruction.operand_width == 32U &&
                instruction.raw.prefix_count == 0U &&
                instruction.raw.modrm.reg == 6U;
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

bool IsHighByteRegister(const ZydisRegister reg)
{
    switch (reg)
    {
        case ZYDIS_REGISTER_AH:
        case ZYDIS_REGISTER_CH:
        case ZYDIS_REGISTER_DH:
        case ZYDIS_REGISTER_BH:
            return true;
        default:
            return false;
    }
}

std::uint8_t HighByteSourceGpr(const ZydisRegister reg)
{
    switch (reg)
    {
        case ZYDIS_REGISTER_AH: return 0U;  // EAX
        case ZYDIS_REGISTER_CH: return 1U;  // ECX
        case ZYDIS_REGISTER_DH: return 2U;  // EDX
        case ZYDIS_REGISTER_BH: return 3U;  // EBX
        default: return 0xFFU;
    }
}

// A REX prefix changes the meaning of the four legacy high-byte register
// encodings. Keep this separate from the ESP field classification because the
// two transforms have different output shapes: the high-byte source needs a
// scratch materialisation before the instruction, while ESP only changes the
// REX and ModRM/SIB fields.
struct HighByteFields
{
    bool present = false;
    bool source_only = false;
    std::uint8_t source_gpr = 0xFFU;
};

HighByteFields ClassifyHighByteFields(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands)
{
    HighByteFields fields;
    for (std::uint8_t index = 0; index < instruction.operand_count; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if (operand.type != ZYDIS_OPERAND_TYPE_REGISTER ||
            !IsHighByteRegister(operand.reg.value))
        {
            continue;
        }
        if (fields.present)
        {
            fields.source_only = false;
            fields.source_gpr = 0xFFU;
            return fields;
        }
        fields.present = true;
        const bool read =
            (operand.actions & ZYDIS_OPERAND_ACTION_MASK_READ) != 0U;
        const bool write =
            (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0U;
        const std::uint8_t source_gpr = HighByteSourceGpr(operand.reg.value);
        fields.source_gpr = source_gpr;
        if (!read || write ||
            operand.encoding != ZYDIS_OPERAND_ENCODING_MODRM_REG ||
            source_gpr == 0xFFU)
        {
            return fields;
        }
        fields.source_only = true;
    }
    return fields;
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

bool HasStackPointerMemoryBase(const ZydisDecodedInstruction& instruction,
                              const ZydisDecodedOperand* operands)
{
    for (std::uint8_t index = 0; index < instruction.operand_count; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY &&
            IsStackPointerRegister(operand.mem.base))
        {
            return true;
        }
    }
    return false;
}

bool IsHighByteMemoryDestination(const ZydisDecodedInstruction& instruction,
                                 const ZydisDecodedOperand* operands,
                                 const HighByteFields& high_byte_fields,
                                 const StackPointerFields& stack_fields)
{
    return instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        instruction.opcode == 0x8AU && instruction.raw.prefix_count == 0U &&
        instruction.raw.modrm.mod != 3U &&
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) == 0U &&
        high_byte_fields.present && !high_byte_fields.source_only &&
        high_byte_fields.source_gpr < 4U && stack_fields.rex_b &&
        !stack_fields.rex_r && HasStackPointerMemoryBase(instruction, operands);
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

// Task 681. A 16-bit code object can opt into both 32-bit address and operand
// sizes for LEA. Long mode keeps the 32-bit address-size prefix, but its
// default operand size is already 32 bits, so the guest's 66 prefix must be
// removed. The only guest register mapping that changes is ESP -> R15.
bool IsMode16Lea32(const std::uint8_t* const bytes,
                   const ZydisDecodedInstruction& instruction,
                   const ZydisDecodedOperand* const operands)
{
    if (bytes == nullptr || operands == nullptr ||
        instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT ||
        instruction.opcode != 0x8DU ||
        instruction.mnemonic != ZYDIS_MNEMONIC_LEA ||
        instruction.operand_width != 32U ||
        instruction.address_width != 32U ||
        instruction.raw.prefix_count == 0U ||
        !HasMemoryOperand(instruction, operands) ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) != 0U ||
        instruction.raw.modrm.mod == 3U)
    {
        return false;
    }

    bool has_operand_size = false;
    bool has_address_size = false;
    for (std::size_t index = 0U;
         index < instruction.raw.prefix_count; ++index)
    {
        if (bytes[index] == 0x66U)
        {
            has_operand_size = true;
        }
        else if (bytes[index] == 0x67U)
        {
            has_address_size = true;
        }
        else
        {
            return false;
        }
    }
    if (!has_operand_size || !has_address_size)
    {
        return false;
    }

    // A valid ESP base is remappable. Any other stack-pointer shape is kept
    // out of this lowering rather than relying on an incomplete field rewrite.
    return !TouchesStackPointer(instruction, operands) ||
        ClassifyStackPointerFields(instruction, operands).supported;
}

// Task 685. In mode16, `66 85 /r` selects a 32-bit register TEST. Long mode
// already defaults to 32-bit operands, so this narrow register-only form can
// remove its operand-size prefix without remapping the guest stack register.
bool IsMode16Test32(const std::uint8_t* const bytes,
                    const ZydisDecodedInstruction& instruction,
                    const ZydisDecodedOperand* const operands)
{
    return bytes != nullptr && operands != nullptr &&
        instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        instruction.opcode == 0x85U &&
        instruction.mnemonic == ZYDIS_MNEMONIC_TEST &&
        instruction.operand_width == 32U &&
        instruction.address_width == 16U && instruction.length == 3U &&
        instruction.raw.prefix_count == 1U &&
        instruction.raw.prefixes[0].value == 0x66U &&
        (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) != 0U &&
        instruction.raw.modrm.offset == 2U &&
        instruction.raw.modrm.mod == 3U &&
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) == 0U &&
        !TouchesStackPointer(instruction, operands);
}

struct Mode16Lea16AddressFields
{
    std::uint8_t first_register = 0xFFU;
    std::uint8_t second_register = 0xFFU;
    std::int32_t displacement = 0;
};

bool IsMode16Lea16(const std::uint8_t* const bytes,
                   const ZydisDecodedInstruction& instruction,
                   const ZydisDecodedOperand* const operands,
                   Mode16Lea16AddressFields* const address_fields)
{
    if (bytes == nullptr || operands == nullptr ||
        instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT ||
        instruction.opcode != 0x8DU ||
        instruction.mnemonic != ZYDIS_MNEMONIC_LEA ||
        instruction.operand_width != 16U ||
        instruction.address_width != 16U ||
        instruction.raw.prefix_count != 0U ||
        !HasMemoryOperand(instruction, operands) ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) != 0U ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) == 0U ||
        instruction.raw.modrm.mod == 3U ||
        instruction.raw.modrm.offset != 1U)
    {
        return false;
    }

    Mode16Lea16AddressFields fields;
    const std::uint8_t mod = instruction.raw.modrm.mod;
    const std::uint8_t rm = instruction.raw.modrm.rm;
    switch (rm)
    {
        case 0U: fields.first_register = 3U; fields.second_register = 6U; break;
        case 1U: fields.first_register = 3U; fields.second_register = 7U; break;
        case 4U: fields.first_register = 6U; break;
        case 5U: fields.first_register = 7U; break;
        case 6U:
            if (mod != 0U)
            {
                // BP defaults to SS in 16-bit addressing. The current x64
                // lowering has not proven the default-SS base contract.
                return false;
            }
            break;
        case 7U: fields.first_register = 3U; break;
        default: return false;
    }

    std::size_t displacement_bytes = 0U;
    if (mod == 1U)
    {
        displacement_bytes = 1U;
        fields.displacement = static_cast<std::int8_t>(
            bytes[instruction.raw.disp.offset]);
    }
    else if (mod == 2U || (mod == 0U && rm == 6U))
    {
        displacement_bytes = 2U;
        fields.displacement = static_cast<std::int32_t>(
            static_cast<std::uint16_t>(bytes[instruction.raw.disp.offset]) |
            (static_cast<std::uint16_t>(
                 bytes[instruction.raw.disp.offset + 1U]) << 8U));
    }
    if (instruction.raw.disp.offset + displacement_bytes !=
            instruction.length ||
        instruction.raw.disp.size != displacement_bytes * 8U)
    {
        return false;
    }

    if (TouchesStackPointer(instruction, operands) &&
        !ClassifyStackPointerFields(instruction, operands).supported)
    {
        return false;
    }
    if (address_fields != nullptr)
    {
        *address_fields = fields;
    }
    return true;
}

// Task 683. LOOPNZ selects its counter width from the address-size attribute.
// The prefix-free mode16 form therefore names CX, while long mode has no
// 16-bit address-size encoding for the copied opcode to retain.
bool IsMode16LoopNz(const ZydisDecodedInstruction& instruction)
{
    return instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT &&
        instruction.opcode == 0xE0U &&
        instruction.mnemonic == ZYDIS_MNEMONIC_LOOPNE &&
        instruction.meta.category == ZYDIS_CATEGORY_COND_BR &&
        instruction.length == 2U && instruction.address_width == 16U &&
        instruction.raw.prefix_count == 0U;
}

}  // namespace

LongModeCompatibilityResult ClassifyLongModeBytes(
    const std::uint8_t* const bytes,
    const std::size_t byte_count,
    const GuestCodeDefaultOperandSize guest_code_default_operand_size)
{
    if (bytes == nullptr || byte_count == 0U)
    {
        return Refuse(LongModeDivergence::kNone);
    }

    // The guest's ISA is the source, so the decode stays in its legacy mode.
    // This asks what these bytes mean where they came from; what they would
    // mean in long mode is the judgement below, and decoding them as 64-bit
    // would answer a different question. Unknown mode keeps the historical
    // legacy-32 default used by existing callers.
    const bool guest_is_16_bit = guest_code_default_operand_size ==
        GuestCodeDefaultOperandSize::k16;
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder,
            guest_is_16_bit ? ZYDIS_MACHINE_MODE_LEGACY_16
                            : ZYDIS_MACHINE_MODE_LEGACY_32,
            guest_is_16_bit ? ZYDIS_STACK_WIDTH_16 : ZYDIS_STACK_WIDTH_32)))
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

    // Task 680. Do not send a 16-bit decode through the 32-bit classifier. A
    // word stack-pointer write is the one proven form; every other 16-bit
    // instruction remains a boundary until its operand and stack semantics are
    // separately established.
    if (guest_is_16_bit)
    {
        if (IsMovStackPointerImmediate16(instruction))
        {
            return Reencode(
                LongModeDivergence::kStackPointerRegister,
                LongModeLowering::k16BitStackPointerImmediateToR15);
        }
        if (IsMode16MovImmediate(instruction))
        {
            return Reencode(LongModeDivergence::kOperandWidth,
                            LongModeLowering::k16BitMovImmediateToGuestGprs);
        }
        if (IsMode16MovRegister(instruction))
        {
            return Reencode(LongModeDivergence::kOperandWidth,
                            LongModeLowering::k16BitMovRegisterToGuestGprs);
        }
        if (IsMode16Test32(bytes, instruction, operands))
        {
            return Reencode(LongModeDivergence::kOperandWidth,
                            LongModeLowering::k16BitTest32ToGuestGprs);
        }
        if (IsMode16Lea32(bytes, instruction, operands))
        {
            return Reencode(LongModeDivergence::kAddressSize,
                            LongModeLowering::k16BitLea32ToGuestGprs);
        }
        if (IsMode16Lea16(bytes, instruction, operands, nullptr))
        {
            return Reencode(LongModeDivergence::kAddressSize,
                            LongModeLowering::k16BitLea16ToGuestGprs);
        }
        if (IsMode16LoopNz(instruction))
        {
            return Reencode(LongModeDivergence::kAddressSize,
                            LongModeLowering::k16BitLoopNzToGuestCx);
        }
        return Refuse(LongModeDivergence::kOperandWidth);
    }

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
        if (IsMovStackPointerImmediate(instruction))
        {
            return Reencode(LongModeDivergence::kStackPointerRegister,
                            LongModeLowering::kStackPointerImmediateToR15);
        }
        if (IsSilentlyDifferentOpcode(opcode))
        {
            return Refuse(LongModeDivergence::kSilentlyDifferent);
        }
        if (IsInvalidInLongMode(opcode))
        {
            // Task 634. Invalid is not the same as unbuildable. `PUSHAD` and
            // `POPAD` have no long-mode encoding at all, which is why the
            // divergence stays `kInvalidInLongMode` -- but their effect is
            // eight moves and one stack adjustment, and the sequence writer can
            // spell that. Every other opcode here keeps `kNone` and stays
            // refused, `PUSH ES` among them: it is serviced by the guest
            // segment HLE rather than rewritten.
            const LongModeLowering lowering =
                HasStackSequenceLowering(opcode, instruction)
                    ? LongModeLowering::kStackSequence
                    : LongModeLowering::kNone;
            if (lowering == LongModeLowering::kNone)
            {
                return Refuse(LongModeDivergence::kInvalidInLongMode);
            }
            return Reencode(LongModeDivergence::kInvalidInLongMode, lowering);
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
        const StackPointerFields stack_fields =
            ClassifyStackPointerFields(instruction, operands);
        const HighByteFields high_byte_fields =
            ClassifyHighByteFields(instruction, operands);
        if (!stack_fields.supported)
        {
            return Refuse(LongModeDivergence::kStackPointerRegister);
        }
        if (high_byte_fields.present)
        {
            // The high-byte form is admitted only when it is a source-only
            // ModRM.reg operand and ESP is the base of a memory operand. A
            // destination or read/write form needs a result merge back into
            // AH/CH/DH/BH, which this lowering does not claim to implement.
            if (high_byte_fields.source_only &&
                !stack_fields.rex_r && stack_fields.rex_b &&
                HasStackPointerMemoryBase(instruction, operands))
            {
                return Reencode(
                    LongModeDivergence::kStackPointerRegister,
                    LongModeLowering::kStackPointerHighByteToR15);
            }
            if (IsHighByteMemoryDestination(
                    instruction, operands, high_byte_fields, stack_fields))
            {
                return Reencode(
                    LongModeDivergence::kStackPointerRegister,
                    LongModeLowering::kStackPointerHighByteDestinationToR15);
            }
            return Refuse(LongModeDivergence::kStackPointerRegister);
        }
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

    // Task 634. The same two moves with a displacement, which is all the
    // register-array forms need beyond what is already here.
    // mov [r15 + disp8], r32
    void StoreGuestRegisterAt(const std::uint8_t reg,
                              const std::int8_t displacement)
    {
        Byte(0x41U);  // REX.B for r15 as the base
        Byte(0x89U);
        Byte(static_cast<std::uint8_t>(0x47U | (reg << 3U)));
        Byte(static_cast<std::uint8_t>(displacement));
        ++instructions;
    }

    // mov r32, [r15 + disp8]
    void LoadGuestRegisterAt(const std::uint8_t reg,
                             const std::int8_t displacement)
    {
        Byte(0x41U);
        Byte(0x8BU);
        Byte(static_cast<std::uint8_t>(0x47U | (reg << 3U)));
        Byte(static_cast<std::uint8_t>(displacement));
        ++instructions;
    }

    // mov r14d, [guest memory operand]. Keep the source addressing bytes except
    // for ModRM.reg, which must name the R14D scratch, an ESP base, which must
    // name the guest ESP register held in R15D, and an absolute disp32 form,
    // which must become the long-mode SIB absolute encoding. A 0x67 prefix
    // narrows the address calculation but does not turn long-mode RIP-relative
    // ModRM rm=101 back into a legacy absolute address.
    void LoadScratchFromMemory(const std::uint8_t* const bytes,
                               const ZydisDecodedInstruction& instruction,
                               const bool guest_esp_base)
    {
        const std::size_t modrm_offset = instruction.raw.modrm.offset;
        const std::uint8_t modrm = bytes[modrm_offset];
        const bool absolute_disp32 = (modrm & 0xC7U) == 0x05U;
        Byte(0x67U);
        Byte(guest_esp_base ? 0x45U : 0x44U);
        Byte(0x8BU);
        if (absolute_disp32)
        {
            // mod=00, reg=110 (R14D), rm=100 (SIB follows).
            Byte(static_cast<std::uint8_t>((modrm & 0xF8U) | 0x04U));
            Byte(0x25U);  // scale=0, index=none, base=disp32.
        }
        else
        {
            Byte(static_cast<std::uint8_t>((modrm & 0xC7U) | 0x30U));
        }

        std::size_t tail_offset = modrm_offset + 1U;
        if (!absolute_disp32 && (modrm & 0x07U) == 4U)
        {
            std::uint8_t sib = bytes[tail_offset++];
            if (guest_esp_base)
            {
                sib = static_cast<std::uint8_t>((sib & 0xF8U) | 0x07U);
            }
            Byte(sib);
        }
        while (tail_offset < instruction.length)
        {
            Byte(bytes[tail_offset++]);
        }
        ++instructions;
    }

    // mov [r15 + disp8], r14d. REX.R names r14 and REX.B names r15.
    void StoreScratchAt(const std::int8_t displacement)
    {
        Byte(0x45U);
        Byte(0x89U);
        Byte(0x77U);  // mod=01, reg=110 (r14), rm=111 (r15)
        Byte(static_cast<std::uint8_t>(displacement));
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
        if (instruction.operand_width == 16U)
        {
            if (reg == 4U)
            {
                if (is_pop)
                {
                    writer->Byte(0x66U);
                    writer->ExtendedMove(0x45U, 0x8BU, 0x37U); // mov r14w,[r15]
                    writer->AdjustGuestEsp(2);
                    writer->Byte(0x66U);
                    writer->ExtendedMove(0x45U, 0x89U, 0xF7U); // mov r15w,r14w
                    return true;
                }
                writer->ExtendedMove(0x45U, 0x89U, 0xFEU); // mov r14d,r15d
                writer->AdjustGuestEsp(-2);
                writer->Byte(0x66U);
                writer->ExtendedMove(0x45U, 0x89U, 0x37U); // mov [r15],r14w
                return true;
            }
            if (is_pop)
            {
                writer->Byte(0x66U);
                writer->LoadGuestRegister(reg);
                writer->AdjustGuestEsp(2);
                return true;
            }
            writer->AdjustGuestEsp(-2);
            writer->Byte(0x66U);
            writer->StoreGuestRegister(reg);
            return true;
        }
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
        case 0xFFU:
        {
            if (instruction.operand_width != 32U ||
                instruction.raw.prefix_count != 0U ||
                instruction.raw.modrm.reg != 6U)
            {
                return false;
            }
            if (instruction.raw.modrm.mod == 3U)
            {
                const std::uint8_t source_register =
                    instruction.raw.modrm.rm;
                if (source_register == 4U)
                {
                    // PUSH ESP reads the pre-decrement value.
                    writer->ExtendedMove(0x45U, 0x89U, 0xFEU);
                    writer->AdjustGuestEsp(-4);
                    writer->ExtendedMove(0x45U, 0x89U, 0x37U);
                    return true;
                }
                writer->AdjustGuestEsp(-4);
                writer->StoreGuestRegister(source_register);
                return true;
            }

            const std::uint8_t modrm =
                bytes[instruction.raw.modrm.offset];
            bool guest_esp_base = false;
            if ((modrm & 0x07U) == 4U)
            {
                const std::uint8_t sib =
                    bytes[instruction.raw.modrm.offset + 1U];
                // In mod=00, SIB base=101 means no base and is not ESP.
                guest_esp_base = (sib & 0x07U) == 4U;
            }
            writer->LoadScratchFromMemory(bytes, instruction,
                                          guest_esp_base);
            writer->AdjustGuestEsp(-4);
            writer->ExtendedMove(0x45U, 0x89U, 0x37U);
            return true;
        }
        case 0x60U:
        {
            // PUSHAD. The entry ESP is captured before the adjustment, because
            // the SDM pushes the value ESP held on entry and reading it after
            // the `lea` would store one thirty-two lower. `PUSH ESP` above
            // takes the same shape for the same reason.
            //
            // The slots, in the order the SDM leaves them: EDI at +0 and EAX
            // at +28, with the captured ESP at +12.
            writer->ExtendedMove(0x45U, 0x89U, 0xFEU);  // mov r14d, r15d
            writer->AdjustGuestEsp(-32);
            writer->StoreGuestRegisterAt(7U, 0);    // edi
            writer->StoreGuestRegisterAt(6U, 4);    // esi
            writer->StoreGuestRegisterAt(5U, 8);    // ebp
            writer->StoreScratchAt(12);             // the entry ESP
            writer->StoreGuestRegisterAt(3U, 16);   // ebx
            writer->StoreGuestRegisterAt(2U, 20);   // edx
            writer->StoreGuestRegisterAt(1U, 24);   // ecx
            writer->StoreGuestRegisterAt(0U, 28);   // eax
            return true;
        }
        case 0x61U:
        {
            // POPAD. The +12 slot is read by nobody: the SDM discards the
            // stored ESP rather than restoring it, and the final `lea` is what
            // moves the stack pointer instead.
            writer->LoadGuestRegisterAt(7U, 0);     // edi
            writer->LoadGuestRegisterAt(6U, 4);     // esi
            writer->LoadGuestRegisterAt(5U, 8);     // ebp
            writer->LoadGuestRegisterAt(3U, 16);    // ebx
            writer->LoadGuestRegisterAt(2U, 20);    // edx
            writer->LoadGuestRegisterAt(1U, 24);    // ecx
            writer->LoadGuestRegisterAt(0U, 28);    // eax
            writer->AdjustGuestEsp(32);
            return true;
        }
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
            if (instruction.raw.modrm.mod != 3U)
            {
                // Load, raise, then store. The guest's ModRM, SIB, and
                // displacement are carried over byte for byte; only the `reg`
                // field changes, to `110`, so the store's source is the scratch
                // the load filled. `67` restores 32-bit addressing and `44` is
                // the REX.R that makes `110` mean R14 rather than ESI.
                writer->ExtendedMove(0x45U, 0x8BU, 0x37U);  // mov r14d, [r15]
                writer->AdjustGuestEsp(4);
                writer->Byte(0x67U);
                writer->Byte(0x44U);
                writer->Byte(0x89U);
                writer->Byte(static_cast<std::uint8_t>(
                    (bytes[1] & 0xC7U) | (6U << 3U)));
                for (std::size_t index = 2U; index < instruction.length;
                     ++index)
                {
                    writer->Byte(bytes[index]);
                }
                ++writer->instructions;
                return true;
            }
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
                        std::size_t* const instruction_count,
                        const GuestCodeDefaultOperandSize
                            guest_code_default_operand_size)
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
        ClassifyLongModeBytes(bytes, byte_count,
                              guest_code_default_operand_size);
    if (verdict.lowering == LongModeLowering::kNone)
    {
        return false;
    }

    // Decoded a second time rather than threaded out of the classifier. The
    // cost is one decode on a path that emits, and what it buys is that the
    // classifier's answer stays a judgement about bytes rather than a carrier
    // for a decode nobody else can check.
    const bool guest_is_16_bit = guest_code_default_operand_size ==
        GuestCodeDefaultOperandSize::k16;
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder,
            guest_is_16_bit ? ZYDIS_MACHINE_MODE_LEGACY_16
                            : ZYDIS_MACHINE_MODE_LEGACY_32,
            guest_is_16_bit ? ZYDIS_STACK_WIDTH_16 : ZYDIS_STACK_WIDTH_32)))
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

    // Task 680. `BC iw` in a 16-bit code object writes only SP. R15W is the
    // low word of the guest ESP state, while the host stack remains in RSP.
    if (verdict.lowering ==
        LongModeLowering::k16BitStackPointerImmediateToR15)
    {
        if (!guest_is_16_bit || !IsMovStackPointerImmediate16(instruction) ||
            length != 3U || length + 2U > kMaxLoweredBytes)
        {
            return false;
        }
        lowered[0] = 0x66U;  // Select R15W rather than R15D.
        lowered[1] = 0x41U;  // REX.B selects R15.
        lowered[2] = 0xBFU;  // MOV r16, imm16 with register field 111.
        lowered[3] = bytes[1];
        lowered[4] = bytes[2];
        *lowered_count = 5U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 687. A mode16 `B8+r iw` writes a guest GPR low word. Prefixing the
    // original opcode and immediate with 66 selects the same operand width in
    // long mode without changing the encoded destination register.
    if (verdict.lowering == LongModeLowering::k16BitMovImmediateToGuestGprs)
    {
        if (!guest_is_16_bit || !IsMode16MovImmediate(instruction) ||
            length != 3U || length + 1U > kMaxLoweredBytes)
        {
            return false;
        }
        lowered[0] = 0x66U;
        lowered[1] = bytes[0U];
        lowered[2] = bytes[1U];
        lowered[3] = bytes[2U];
        *lowered_count = 4U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 688. A mode16 register-register word MOV needs only the x64
    // operand-size prefix; the original opcode and ModRM mapping are safe to
    // reuse for the proven no-guest-SP register subset.
    if (verdict.lowering == LongModeLowering::k16BitMovRegisterToGuestGprs)
    {
        if (!guest_is_16_bit || !IsMode16MovRegister(instruction) ||
            length != 2U || length + 1U > kMaxLoweredBytes)
        {
            return false;
        }
        lowered[0] = 0x66U;
        lowered[1] = bytes[0U];
        lowered[2] = bytes[1U];
        *lowered_count = 3U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 685. The mode16 operand-size override selects the guest's 32-bit
    // TEST. Long mode's default is already 32 bits, so copy the opcode and
    // register ModRM after dropping only the `66` prefix.
    if (verdict.lowering == LongModeLowering::k16BitTest32ToGuestGprs)
    {
        if (!guest_is_16_bit || !IsMode16Test32(bytes, instruction, operands) ||
            length != 3U || length - 1U > kMaxLoweredBytes)
        {
            return false;
        }
        lowered[0] = bytes[1U];
        lowered[1] = bytes[2U];
        *lowered_count = 2U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 681. Remove the source-only operand-size override from a 32-bit
    // LEA in a 16-bit code object. Keep the address-size override and rewrite
    // only the ModRM/SIB fields that name guest ESP. All other guest GPRs keep
    // their established host-register mapping.
    if (verdict.lowering == LongModeLowering::k16BitLea32ToGuestGprs)
    {
        if (!guest_is_16_bit ||
            !IsMode16Lea32(bytes, instruction, operands) ||
            instruction.raw.modrm.offset != instruction.raw.prefix_count + 1U)
        {
            return false;
        }
        const StackPointerFields fields =
            ClassifyStackPointerFields(instruction, operands);
        const bool needs_rex = fields.rex_r || fields.rex_b;
        if (needs_rex && length + 1U > kMaxLoweredBytes)
        {
            return false;
        }

        std::size_t out = 0U;
        for (std::size_t index = 0U;
             index < instruction.raw.prefix_count; ++index)
        {
            if (bytes[index] != 0x66U)
            {
                lowered[out++] = bytes[index];
            }
        }
        if (needs_rex)
        {
            std::uint8_t rex = 0x40U;
            rex |= fields.rex_r ? 0x04U : 0x00U;
            rex |= fields.rex_b ? 0x01U : 0x00U;
            lowered[out++] = rex;
        }
        const std::size_t output_opcode_offset = out;
        for (std::size_t index = instruction.raw.prefix_count;
             index < length; ++index)
        {
            lowered[out++] = bytes[index];
        }

        const std::size_t modrm_out = output_opcode_offset + 1U;
        if (modrm_out >= out)
        {
            return false;
        }
        if (fields.rex_r)
        {
            lowered[modrm_out] = static_cast<std::uint8_t>(
                (lowered[modrm_out] & 0xC7U) | 0x38U);
        }
        if (fields.rex_b)
        {
            if ((instruction.attributes & ZYDIS_ATTRIB_HAS_SIB) == 0U ||
                instruction.raw.modrm.rm != 4U ||
                modrm_out + 1U >= out)
            {
                return false;
            }
            lowered[modrm_out + 1U] = static_cast<std::uint8_t>(
                (lowered[modrm_out + 1U] & 0xF8U) | 0x07U);
        }
        *lowered_count = out;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 682. x64 has no 16-bit address-size mode. Materialize the proven
    // 16-bit address form in scratch registers, then use a 32-bit address-size
    // word LEA. MOVZX and LEA preserve flags, and the final word write keeps
    // the guest's low-word destination semantics.
    if (verdict.lowering == LongModeLowering::k16BitLea16ToGuestGprs)
    {
        Mode16Lea16AddressFields address_fields;
        if (!guest_is_16_bit ||
            !IsMode16Lea16(bytes, instruction, operands, &address_fields) ||
            instruction.raw.modrm.offset != 1U)
        {
            return false;
        }

        const StackPointerFields stack_fields =
            ClassifyStackPointerFields(instruction, operands);
        const std::uint8_t destination = instruction.raw.modrm.reg;
        const bool destination_is_stack_pointer = stack_fields.rex_r;
        const std::size_t final_length = 2U + 1U + 1U + 1U + 4U;
        const std::size_t first_load_length =
            address_fields.first_register != 0xFFU ? 4U : 0U;
        const std::size_t second_load_length =
            address_fields.second_register != 0xFFU ? 4U : 0U;
        const std::size_t sum_length =
            address_fields.second_register != 0xFFU ? 5U : 0U;
        const std::size_t zero_length =
            address_fields.first_register == 0xFFU ? 6U : 0U;
        if (destination > 7U ||
            (!destination_is_stack_pointer && destination == 4U) ||
            first_load_length + second_load_length + sum_length +
                    zero_length + final_length > kMaxLoweredBytes)
        {
            return false;
        }

        std::size_t out = 0U;
        if (address_fields.first_register != 0xFFU)
        {
            lowered[out++] = 0x44U;  // REX.R selects R14D.
            lowered[out++] = 0x0FU;
            lowered[out++] = 0xB7U;
            lowered[out++] = static_cast<std::uint8_t>(
                0xF0U | address_fields.first_register);
        }
        else
        {
            lowered[out++] = 0x41U;  // REX.B selects R14D.
            lowered[out++] = 0xBEU;  // MOV R14D,0.
            lowered[out++] = 0x00U;
            lowered[out++] = 0x00U;
            lowered[out++] = 0x00U;
            lowered[out++] = 0x00U;
        }
        if (address_fields.second_register != 0xFFU)
        {
            lowered[out++] = 0x44U;  // REX.R selects R10D.
            lowered[out++] = 0x0FU;
            lowered[out++] = 0xB7U;
            lowered[out++] = static_cast<std::uint8_t>(
                0xD0U | address_fields.second_register);
            lowered[out++] = 0x67U;
            lowered[out++] = 0x47U;  // R14D destination/base, R10D index.
            lowered[out++] = 0x8DU;
            lowered[out++] = 0x34U;
            lowered[out++] = 0x16U;
        }

        lowered[out++] = 0x67U;  // Select 32-bit address calculation.
        lowered[out++] = 0x66U;  // Select the guest word destination.
        lowered[out++] = destination_is_stack_pointer ? 0x45U : 0x41U;
        lowered[out++] = 0x8DU;
        lowered[out++] = static_cast<std::uint8_t>(
            0x80U | ((destination_is_stack_pointer ? 7U : destination) << 3U) |
            6U);  // [R14D + disp32], with REX.B.
        const std::uint32_t displacement = static_cast<std::uint32_t>(
            address_fields.displacement);
        for (std::size_t index = 0U; index < 4U; ++index)
        {
            lowered[out++] = static_cast<std::uint8_t>(
                (displacement >> (index * 8U)) & 0xFFU);
        }
        *lowered_count = out;
        if (instruction_count != nullptr)
        {
            *instruction_count = address_fields.second_register != 0xFFU
                ? 4U : 2U;
        }
        return true;
    }

    // Task 683. The LOOPNZ sequence needs direct-target and fallthrough
    // addresses from an AOT instruction record. The cache emitter owns that
    // control-flow lowering, so the byte-only API deliberately does not emit
    // a target-free pseudo-branch.
    if (verdict.lowering == LongModeLowering::k16BitLoopNzToGuestCx)
    {
        return false;
    }

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

    // Task 614. A REX changes ModRM reg=100..111 from AH/CH/DH/BH to
    // SPL/BPL/SIL/DIL. Materialise the source high byte in R14B without
    // changing flags: exchange the source low and high bytes using legacy
    // encodings, copy the exposed low byte to R14B, and exchange them back.
    //
    // The original legacy prefixes stay immediately before the opcode. The
    // scratch sequence is placed before those prefixes, and the transformed
    // instruction then gets one combined REX.R (R14B) + REX.B (R15 base).
    if (verdict.lowering == LongModeLowering::kStackPointerHighByteToR15)
    {
        const StackPointerFields stack_fields =
            ClassifyStackPointerFields(instruction, operands);
        const HighByteFields high_byte_fields =
            ClassifyHighByteFields(instruction, operands);
        const std::size_t opcode_offset = instruction.raw.prefix_count;
        const std::size_t scratch_bytes = 7U;
        const bool unsupported_opcode_map =
            instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT &&
            instruction.opcode_map != ZYDIS_OPCODE_MAP_0F;
        const std::size_t expected_modrm_offset =
            instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT
                ? opcode_offset + 1U
                : opcode_offset + 2U;
        if (!stack_fields.supported || !stack_fields.rex_b ||
            stack_fields.rex_r || !high_byte_fields.present ||
            !high_byte_fields.source_only ||
            high_byte_fields.source_gpr > 3U ||
            !HasStackPointerMemoryBase(instruction, operands) ||
            instruction.raw.modrm.offset == 0U ||
            opcode_offset >= length ||
            instruction.raw.modrm.offset <= opcode_offset ||
            unsupported_opcode_map ||
            instruction.raw.modrm.offset != expected_modrm_offset ||
            instruction.raw.modrm.mod == 3U ||
            length + scratch_bytes + 1U > kMaxLoweredBytes)
        {
            return false;
        }

        std::size_t out = 0U;
        // xchg low,high: 86 /r with low in ModRM.reg and high in rm. No REX
        // is used here, so the legacy high-byte register names remain valid.
        const std::uint8_t low_high_exchange = static_cast<std::uint8_t>(
            0xC4U + high_byte_fields.source_gpr * 9U);
        lowered[out++] = 0x86U;
        lowered[out++] = low_high_exchange;
        // mov r14b, low: 41 88 /r, mod=11, rm=R14.
        lowered[out++] = 0x41U;
        lowered[out++] = 0x88U;
        lowered[out++] = static_cast<std::uint8_t>(
            0xC6U | (high_byte_fields.source_gpr << 3U));
        // Restore the source GPR before running the original operation.
        lowered[out++] = 0x86U;
        lowered[out++] = low_high_exchange;

        for (std::size_t index = 0; index < opcode_offset; ++index)
        {
            lowered[out++] = bytes[index];
        }
        lowered[out++] = 0x45U;  // REX.R for R14B and REX.B for R15.
        for (std::size_t index = opcode_offset; index < length; ++index)
        {
            lowered[out++] = bytes[index];
        }

        const std::size_t modrm_out =
            scratch_bytes + static_cast<std::size_t>(
                                instruction.raw.modrm.offset) + 1U;
        // ModRM reg=110 plus REX.R names R14B instead of the original high
        // byte register. The classifier only admits a source in this field.
        lowered[modrm_out] = static_cast<std::uint8_t>(
            (lowered[modrm_out] & 0xC7U) | 0x30U);
        const std::size_t sib_out = modrm_out + 1U;
        if (sib_out >= out)
        {
            return false;
        }
        lowered[sib_out] = static_cast<std::uint8_t>(
            (lowered[sib_out] & 0xF8U) | 0x07U);

        *lowered_count = out;
        if (instruction_count != nullptr)
        {
            // xchg, mov, xchg, and the re-encoded guest instruction.
            *instruction_count = 4U;
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
        // Task 574. Everything before the opcode is a legacy prefix and is
        // copied as it is, so where the prefixes end is where the opcode
        // begins. This used to be derived as `modrm.offset - 1`, which is true
        // only of a one-byte opcode -- and the comment defending that read
        // "a two-byte opcode map never reaches here, because `ESP` in ModRM or
        // SIB is a one-byte-opcode shape".
        //
        // That was false. `movzx esi, byte ptr [esp+8]` is `0F B6` with `ESP`
        // as the SIB base: a two-byte opcode in exactly the shape this unit
        // admits. `movzx` has no other encoding, so every one the census
        // counted was refused right here.
        //
        // `prefix_count` is the opcode's start in both maps, and it puts the
        // REX after any mandatory `66`/`F2`/`F3`, which is where the encoding
        // rules require it.
        const std::size_t opcode_offset = instruction.raw.prefix_count;
        if (!fields.supported || length + 1U > kMaxLoweredBytes ||
            instruction.raw.modrm.offset == 0U ||
            opcode_offset >= length ||
            instruction.raw.modrm.offset <= opcode_offset)
        {
            return false;
        }
        // And the layout is asserted rather than assumed. Moving the opcode's
        // start is not on its own a reason to believe ModRM is where this code
        // then writes to: a REX placed between `0F` and its second opcode byte
        // encodes a different instruction and raises nothing.
        //
        // `0F38` and `0F3A` never arrive -- the classifier refuses them -- but
        // a function that writes a layout checks the layout it writes.
        const std::size_t expected_modrm_offset =
            instruction.opcode_map == ZYDIS_OPCODE_MAP_DEFAULT
                ? opcode_offset + 1U
                : opcode_offset + 2U;
        if ((instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT &&
             instruction.opcode_map != ZYDIS_OPCODE_MAP_0F) ||
            instruction.raw.modrm.offset != expected_modrm_offset)
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

    // Task 674. The opcode-embedded `ESP` destination has no ModRM field.
    // Re-encode `BC imm32` as `41 BF imm32`, which writes guest ESP's host
    // register R15D and leaves the SysV host stack pointer in RSP untouched.
    if (verdict.lowering == LongModeLowering::kStackPointerImmediateToR15)
    {
        if (!IsMovStackPointerImmediate(instruction) ||
            length + 1U > kMaxLoweredBytes)
        {
            return false;
        }
        lowered[0] = 0x41U;  // REX.B selects R15D.
        lowered[1] = 0xBFU;  // MOV r32, imm32 with register field 111.
        for (std::size_t index = 1U; index < 5U; ++index)
        {
            lowered[index + 1U] = bytes[index];
        }
        *lowered_count = 6U;
        if (instruction_count != nullptr)
        {
            *instruction_count = 1U;
        }
        return true;
    }

    // Task 676. A REX prefix cannot preserve an AH/CH/DH/BH destination. Save
    // DL in the non-guest scratch R14B, load through the guest stack pointer
    // into DL, copy DL into the legacy high byte without a REX prefix, and
    // restore DL. The admitted form is deliberately restricted to MOV r8,
    // r/m8 with a SIB ESP base and no segment override.
    if (verdict.lowering ==
        LongModeLowering::kStackPointerHighByteDestinationToR15)
    {
        const StackPointerFields stack_fields =
            ClassifyStackPointerFields(instruction, operands);
        const HighByteFields high_byte_fields =
            ClassifyHighByteFields(instruction, operands);
        const std::size_t opcode_offset = instruction.raw.prefix_count;
        const std::size_t modrm_offset = instruction.raw.modrm.offset;
        const std::size_t expected_modrm_offset = opcode_offset + 1U;
        if (!IsHighByteMemoryDestination(
                instruction, operands, high_byte_fields, stack_fields) ||
            modrm_offset != expected_modrm_offset ||
            modrm_offset + 1U >= length ||
            (bytes[modrm_offset] & 0x07U) != 4U ||
            (bytes[modrm_offset + 1U] & 0x07U) != 4U ||
            length + 7U > kMaxLoweredBytes)
        {
            return false;
        }

        std::size_t out = 0U;
        // mov r14b, dl
        lowered[out++] = 0x44U;
        lowered[out++] = 0x88U;
        lowered[out++] = 0xF2U;

        // mov dl, [r15 + displacement]
        lowered[out++] = 0x41U;
        for (std::size_t index = opcode_offset; index < length; ++index)
        {
            lowered[out++] = bytes[index];
        }
        const std::size_t modrm_out =
            4U + modrm_offset - opcode_offset;
        lowered[modrm_out] = static_cast<std::uint8_t>(
            (lowered[modrm_out] & 0xC7U) | 0x10U);
        const std::size_t sib_out = modrm_out + 1U;
        lowered[sib_out] = static_cast<std::uint8_t>(
            (lowered[sib_out] & 0xF8U) | 0x07U);

        // mov AH/CH/DH/BH, dl, with no REX so the legacy high-byte name stays.
        lowered[out++] = 0x8AU;
        lowered[out++] = static_cast<std::uint8_t>(
            0xE2U | (high_byte_fields.source_gpr << 3U));

        // mov dl, r14b
        lowered[out++] = 0x44U;
        lowered[out++] = 0x88U;
        lowered[out++] = 0xF2U;

        *lowered_count = out;
        if (instruction_count != nullptr)
        {
            *instruction_count = 4U;
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
    // Task 572. Four displacement bytes follow the ModRM byte in this form, and
    // nothing may sit between them -- but something may sit *after* them.
    //
    // This used to read `modrm_offset + 1 + 4 != length`, which additionally
    // demanded that the displacement be the instruction's last field. That
    // demand refused every absolute form carrying an immediate, and those were
    // 865 of the 1,609 instructions the census still refused: `cmp
    // [abs32],imm8`, `test [abs32],imm32`, `mov [abs32],imm32`, and their
    // group-1 siblings.
    //
    // An immediate may be carried through because **its value does not depend
    // on where it sits**. Inserting the SIB byte pushes it one byte later and no
    // field refers to that offset. The one field whose position does carry
    // meaning is the RIP-relative displacement, which is precisely what this
    // lowering is removing.
    //
    // The single arithmetic identity above asserted both "the disp32 is here"
    // and "nothing follows it". Only the first is wanted, so the field
    // positions are now asked of the decoder rather than inferred from length.
    if (instruction.raw.disp.size != 32U ||
        instruction.raw.disp.offset != modrm_offset + 1U)
    {
        // `IsAbsoluteDisplacementForm` reads only ModRM's `mod` and `rm`. Under
        // an existing 0x67 prefix the guest is addressing in 16 bits, where
        // `mod=00 rm=101` is `[DI]` and not absolute at all; that form carries
        // no displacement and is refused here. The replaced arithmetic used to
        // reject it as a side effect, so the protection is carried across
        // explicitly rather than dropped.
        return false;
    }
    const std::size_t displacement_end = instruction.raw.disp.offset + 4U;
    if (displacement_end > length)
    {
        return false;
    }
    // Every byte between the displacement and the end of the instruction has to
    // be accounted for as an immediate. In legacy 32-bit decoding nothing else
    // follows ModRM/SIB/displacement, so this holds for the forms meant here --
    // and where it does not, the encoding is one this unit did not understand
    // and is refused rather than guessed at.
    std::size_t immediate_bytes = 0U;
    for (const auto& immediate : instruction.raw.imm)
    {
        if (immediate.size == 0U)
        {
            continue;
        }
        // A relative immediate is a branch displacement, and its value *does*
        // depend on the instruction's length -- the one thing that breaks this
        // lowering's premise. Control flow is refused by the classifier long
        // before here, so this is the premise written down rather than a case
        // expected to occur.
        if (immediate.is_relative)
        {
            return false;
        }
        immediate_bytes += immediate.size / 8U;
    }
    if (displacement_end + immediate_bytes != length)
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
    // The immediate, byte for byte. It moved two bytes later in the encoding
    // and its value is unchanged, which is the whole of decision 1.
    for (std::size_t index = displacement_end; index < length; ++index)
    {
        lowered[out++] = bytes[index];
    }
    *lowered_count = out;
    if (instruction_count != nullptr)
    {
        *instruction_count = 1U;
    }
    return true;
}

}  // namespace repiu::runtime
