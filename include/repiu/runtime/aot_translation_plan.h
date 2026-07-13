#ifndef REPIU_RUNTIME_AOT_TRANSLATION_PLAN_H_
#define REPIU_RUNTIME_AOT_TRANSLATION_PLAN_H_

#include "repiu/runtime/runtime_memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

enum class AotInstructionKind
{
    kCopy,
    kDirectCall,
    kDirectJump,
    kConditionalBranch,
    kReturn,
    kHleBoundary,
    kIndirectExit,
    kJumpTable,
};

struct AotInstructionRecord
{
    std::uint32_t guest_address = 0;
    std::uint32_t direct_target = 0;
    std::uint32_t fallthrough_target = 0;
    AotInstructionKind kind = AotInstructionKind::kCopy;
    std::uint8_t length = 0;
    std::uint8_t table_index_register = 0xFFU;
    std::uint16_t mnemonic = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint32_t> table_targets;
};

struct AotBasicBlock
{
    std::uint32_t guest_address = 0;
    std::vector<AotInstructionRecord> instructions;
};

struct AotExcludedGuestRange
{
    std::uint32_t guest_address = 0;
    std::uint32_t byte_count = 0;
};

struct AotTranslationPlan
{
    bool valid = false;
    std::uint32_t entry_address = 0;
    std::uint32_t block_count = 0;
    std::uint32_t instruction_count = 0;
    std::uint64_t source_code_bytes = 0;
    std::uint64_t estimated_emitted_bytes = 0;
    std::uint32_t copy_instruction_count = 0;
    std::uint32_t direct_call_count = 0;
    std::uint32_t direct_jump_count = 0;
    std::uint32_t conditional_branch_count = 0;
    std::uint32_t return_count = 0;
    std::uint32_t hle_boundary_count = 0;
    std::uint32_t indirect_exit_count = 0;
    std::uint32_t jump_table_count = 0;
    std::uint32_t jump_table_target_count = 0;
    std::uint32_t outside_image_target_count = 0;
    std::uint32_t decode_failure_count = 0;
    std::uint32_t analysis_limit_count = 0;
    std::uint64_t elapsed_microseconds = 0;
    std::vector<AotBasicBlock> blocks;
    std::string message;
};

bool BuildAotTranslationPlan(const RelocatedRuntimeImage& image,
                             AotTranslationPlan* plan);
bool BuildAotTranslationPlanFromEntry(const RelocatedRuntimeImage& image,
                                      std::uint32_t entry_address,
                                      AotTranslationPlan* plan);
bool BuildAotTranslationPlanFromEntry(
    const RelocatedRuntimeImage& image,
    std::uint32_t entry_address,
    const std::vector<AotExcludedGuestRange>& excluded_ranges,
    AotTranslationPlan* plan);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_TRANSLATION_PLAN_H_
