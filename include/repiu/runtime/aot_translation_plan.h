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
    kFarReturn,
    kHleBoundary,
    kIndirectExit,
    kJumpTable,
    // A segment-override memory access (e.g. GS:[mem]) translated natively with
    // a self-correcting guard and the segment base folded into the displacement
    // (Task 264 Phase 3a). segment_override_register holds the overridden
    // segment (0=ES,2=SS,3=DS,4=FS,5=GS). Falls back to a boundary at emit time
    // for any form the re-encoder cannot verify.
    kSegmentOverrideMem,
    // MOV r16/r32,Sreg reads a guest selector natively only while physical and
    // shadow selectors agree; divergence falls back to HLE (Task 383).
    kGuardedSegmentRead,
    // MOV Sreg,r16 is skipped only while source, physical, and shadow
    // selectors already agree; mismatch falls back to HLE (Task 389).
    kGuardedSegmentLoad,
    // A plain POP ES/DS/FS/GS whose cache slot advances the stack only when
    // physical, shadow, and stack selectors are already identical. Any
    // mismatch reaches the existing HLE boundary (Task 291).
    kGuardedSegmentPop,
    // Port I/O (IN/OUT DX) handled without #DB exception traps (Task 311).
    kPortIo,
};

struct AotInstructionRecord
{
    std::uint32_t guest_address = 0;
    std::uint32_t direct_target = 0;
    std::uint32_t fallthrough_target = 0;
    AotInstructionKind kind = AotInstructionKind::kCopy;
    std::uint8_t length = 0;
    std::uint8_t table_index_register = 0xFFU;
    std::uint8_t segment_override_register = 0xFFU;
    std::uint8_t segment_register = 0xFFU;
    std::uint8_t gpr_register = 0xFFU;
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
    std::uint32_t far_return_count = 0;
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

// Task 330: optional attribution of one plan build, in the units of
// `runtime::ReadCycleCounter`. Observation only — a null profile leaves the
// builder reading no timestamps and behaving exactly as before.
//
// The stages partition the build: `decode` covers operand-array initialization
// and `ZydisDecoderDecodeFull`, `record_build` the instruction record and its
// byte copy, `classify` boundary/branch classification and jump-table reads,
// `walk` the pending/visited structures and range lookups, and `sweep` the
// jump-table reclassification passes. `total_cycles` is measured independently,
// so a derived residual shows whether the partition was complete.
struct AotPlanBuildProfile
{
    bool enabled = false;
    std::uint64_t decoder_init_cycles = 0;
    std::uint64_t decode_cycles = 0;
    std::uint64_t record_build_cycles = 0;
    std::uint64_t classify_cycles = 0;
    std::uint64_t walk_cycles = 0;
    std::uint64_t sweep_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint32_t decode_count = 0;
    std::uint32_t record_count = 0;
    // Answers how often the sweep re-walks the whole plan, which no measurement
    // has covered before.
    std::uint32_t sweep_pass_count = 0;
    std::uint32_t sweep_record_visit_count = 0;
    std::uint32_t clamped_sample_count = 0;
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
    AotTranslationPlan* plan,
    AotPlanBuildProfile* profile = nullptr);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_TRANSLATION_PLAN_H_
