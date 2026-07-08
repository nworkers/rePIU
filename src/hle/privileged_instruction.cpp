#include "repiu/hle/privileged_instruction.h"

#include <cstddef>

namespace repiu::hle
{
namespace
{

std::uint8_t ModRmReg(std::uint8_t modrm)
{
    return static_cast<std::uint8_t>((modrm >> 3) & 0x07);
}

void SetClassification(
    std::uint8_t opcode,
    std::uint32_t length,
    const char* mnemonic,
    PrivilegedInstructionClass instruction_class,
    const char* message,
    PrivilegedInstructionClassification* classification)
{
    classification->valid = true;
    classification->opcode = opcode;
    classification->length = length;
    classification->mnemonic = mnemonic;
    classification->instruction_class = instruction_class;
    classification->hle_trap_candidate =
        instruction_class == PrivilegedInstructionClass::kHleTrapCandidate;
    classification->cpu_state_initialization_candidate =
        instruction_class ==
        PrivilegedInstructionClass::kCpuStateInitializationCandidate;
    classification->message = message;
}

}  // namespace

bool ClassifyPrivilegedInstruction(
    const std::vector<std::uint8_t>& bytes,
    std::uint32_t focus_offset,
    PrivilegedInstructionClassification* classification)
{
    if (classification == nullptr)
    {
        return false;
    }

    *classification = PrivilegedInstructionClassification{};
    if (focus_offset >= bytes.size())
    {
        classification->message = "focus offset is outside the byte window";
        return false;
    }

    const std::size_t offset = static_cast<std::size_t>(focus_offset);
    const std::uint8_t opcode = bytes[offset];
    switch (opcode)
    {
        case 0xFA:
            SetClassification(
                opcode,
                1,
                "CLI",
                PrivilegedInstructionClass::kHleTrapCandidate,
                "clear-interrupt-flag should be handled as a DOS/DPMI trap boundary",
                classification);
            return true;
        case 0xFB:
            SetClassification(
                opcode,
                1,
                "STI",
                PrivilegedInstructionClass::kHleTrapCandidate,
                "set-interrupt-flag should be handled as a DOS/DPMI trap boundary",
                classification);
            return true;
        case 0xF4:
            SetClassification(
                opcode,
                1,
                "HLT",
                PrivilegedInstructionClass::kHleTrapCandidate,
                "halt cannot run in user mode and should become an HLE wait/exit trap",
                classification);
            return true;
        case 0xCD:
            if (offset + 1 < bytes.size())
            {
                SetClassification(
                    opcode,
                    2,
                    "INT imm8",
                    PrivilegedInstructionClass::kHleTrapCandidate,
                    "software interrupt should be dispatched through DOS/DPMI HLE",
                    classification);
                return true;
            }
            break;
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
            SetClassification(
                opcode,
                2,
                "port I/O",
                PrivilegedInstructionClass::kHleTrapCandidate,
                "port I/O should be routed to a hardware or DOS extender HLE service",
                classification);
            return true;
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
            SetClassification(
                opcode,
                1,
                "port I/O",
                PrivilegedInstructionClass::kHleTrapCandidate,
                "port I/O should be routed to a hardware or DOS extender HLE service",
                classification);
            return true;
        case 0x0F:
            if (offset + 1 >= bytes.size())
            {
                break;
            }

            if (bytes[offset + 1] == 0x20 || bytes[offset + 1] == 0x22)
            {
                if (offset + 2 >= bytes.size())
                {
                    break;
                }

                SetClassification(
                    opcode,
                    3,
                    bytes[offset + 1] == 0x20 ? "MOV from CRn"
                                               : "MOV to CRn",
                    PrivilegedInstructionClass::kCpuStateInitializationCandidate,
                    "control-register access points to missing CPU/DPMI state emulation",
                    classification);
                return true;
            }

            if (offset + 2 >= bytes.size())
            {
                break;
            }

            if (bytes[offset + 1] == 0x00)
            {
                const std::uint8_t reg = ModRmReg(bytes[offset + 2]);
                if (reg == 2 || reg == 3)
                {
                    SetClassification(
                        opcode,
                        3,
                        reg == 2 ? "LLDT" : "LTR",
                        PrivilegedInstructionClass::
                            kCpuStateInitializationCandidate,
                        "task or descriptor-register load requires DPMI descriptor modeling",
                        classification);
                    return true;
                }
            }

            if (bytes[offset + 1] == 0x01)
            {
                const std::uint8_t reg = ModRmReg(bytes[offset + 2]);
                if (reg == 2 || reg == 3 || reg == 6 || reg == 7)
                {
                    const char* mnemonic = "descriptor/control operation";
                    if (reg == 2)
                    {
                        mnemonic = "LGDT";
                    }
                    else if (reg == 3)
                    {
                        mnemonic = "LIDT";
                    }
                    else if (reg == 6)
                    {
                        mnemonic = "LMSW";
                    }
                    else if (reg == 7)
                    {
                        mnemonic = "INVLPG";
                    }

                    SetClassification(
                        opcode,
                        3,
                        mnemonic,
                        PrivilegedInstructionClass::
                            kCpuStateInitializationCandidate,
                        "descriptor or control-state operation requires CPU/DPMI state modeling",
                        classification);
                    return true;
                }
            }
            break;
        default:
            break;
    }

    classification->opcode = opcode;
    classification->message =
        "opcode is not recognized by the initial privileged instruction classifier";
    return false;
}

const char* PrivilegedInstructionClassName(
    PrivilegedInstructionClass instruction_class)
{
    switch (instruction_class)
    {
        case PrivilegedInstructionClass::kHleTrapCandidate:
            return "HLE trap candidate";
        case PrivilegedInstructionClass::kCpuStateInitializationCandidate:
            return "CPU/DPMI state initialization candidate";
        case PrivilegedInstructionClass::kUnknown:
        default:
            return "unknown";
    }
}

}  // namespace repiu::hle
