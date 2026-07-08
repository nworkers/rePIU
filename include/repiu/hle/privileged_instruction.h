#ifndef REPIU_HLE_PRIVILEGED_INSTRUCTION_H_
#define REPIU_HLE_PRIVILEGED_INSTRUCTION_H_

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::hle
{

enum class PrivilegedInstructionClass
{
    kUnknown,
    kHleTrapCandidate,
    kCpuStateInitializationCandidate,
};

struct PrivilegedInstructionClassification
{
    bool valid = false;
    std::uint8_t opcode = 0;
    std::uint32_t length = 0;
    std::string mnemonic;
    PrivilegedInstructionClass instruction_class =
        PrivilegedInstructionClass::kUnknown;
    bool hle_trap_candidate = false;
    bool cpu_state_initialization_candidate = false;
    std::string message;
};

bool ClassifyPrivilegedInstruction(
    const std::vector<std::uint8_t>& bytes,
    std::uint32_t focus_offset,
    PrivilegedInstructionClassification* classification);

const char* PrivilegedInstructionClassName(
    PrivilegedInstructionClass instruction_class);

}  // namespace repiu::hle

#endif  // REPIU_HLE_PRIVILEGED_INSTRUCTION_H_
