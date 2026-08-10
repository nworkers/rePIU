#include "repiu/runtime/aot_translation_plan.h"

#include "repiu/runtime/cycle_clock.h"

#include <Zydis.h>

#include <chrono>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace repiu::runtime
{
namespace
{

const std::uint8_t* FindBytes(const RelocatedRuntimeImage& image,
                              std::uint32_t address,
                              std::uint32_t bytes)
{
    for (const RelocatedRuntimeObject& object : image.objects)
    {
        if (address < object.relocated_base_address)
        {
            continue;
        }
        const std::uint64_t offset =
            static_cast<std::uint64_t>(address) - object.relocated_base_address;
        // Task 329: an object may own its bytes or view live guest memory. The
        // range rule is identical either way, so the visible plan is too.
        if (offset + bytes <= RelocatedRuntimeObjectByteCount(object))
        {
            return RelocatedRuntimeObjectBytes(object) + offset;
        }
    }
    return nullptr;
}

bool ReadDirectTarget(const ZydisDecodedInstruction& instruction,
                      const ZydisDecodedOperand* operands,
                      std::uint32_t address,
                      std::uint32_t* target)
{
    if (operands == nullptr || target == nullptr ||
        instruction.operand_count_visible == 0U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE ||
        !operands[0].imm.is_relative ||
        instruction.meta.branch_type == ZYDIS_BRANCH_TYPE_FAR)
    {
        return false;
    }
    ZyanU64 absolute = 0;
    if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
            &instruction, &operands[0], address, &absolute)) ||
        absolute > UINT32_MAX)
    {
        return false;
    }
    *target = static_cast<std::uint32_t>(absolute);
    return true;
}

// Task 264 Phase 3a: a segment-override memory access that the emitter may
// translate natively (guard + base-folded displacement). Returns the overridden
// segment index (0=ES,2=SS,3=DS,4=FS,5=GS). The emitter re-verifies the exact
// form and falls back to a boundary for anything it cannot encode, so this stays
// permissive; it only screens out privileged/CS-override cases up front.
bool IsTranslatableSegmentOverrideMem(const ZydisDecodedInstruction& instruction,
                                      const ZydisDecodedOperand* operands,
                                      std::uint8_t* segment_register)
{
    if (operands == nullptr || segment_register == nullptr ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) == 0U ||
        (instruction.attributes & ZYDIS_ATTRIB_IS_PRIVILEGED) != 0U ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) == 0U)
    {
        return false;
    }
    for (std::uint8_t index = 0;
         index < instruction.operand_count_visible; ++index)
    {
        if (operands[index].type != ZYDIS_OPERAND_TYPE_MEMORY)
        {
            continue;
        }
        switch (operands[index].mem.segment)
        {
            case ZYDIS_REGISTER_ES: *segment_register = 0U; return true;
            case ZYDIS_REGISTER_SS: *segment_register = 2U; return true;
            case ZYDIS_REGISTER_DS: *segment_register = 3U; return true;
            case ZYDIS_REGISTER_FS: *segment_register = 4U; return true;
            case ZYDIS_REGISTER_GS: *segment_register = 5U; return true;
            default: return false; // CS override or none
        }
    }
    return false;
}

bool ReadGuardedSegmentPopRegister(
    const ZydisDecodedInstruction& instruction,
    const std::uint8_t* bytes,
    std::uint8_t* segment_register)
{
    if (bytes == nullptr || segment_register == nullptr)
    {
        return false;
    }
    if (instruction.length == 1U && bytes[0] == 0x07U)
    {
        *segment_register = 0U;
        return true;
    }
    if (instruction.length == 1U && bytes[0] == 0x1FU)
    {
        *segment_register = 3U;
        return true;
    }
    if (instruction.length == 2U && bytes[0] == 0x0FU &&
        bytes[1] == 0xA1U)
    {
        *segment_register = 4U;
        return true;
    }
    if (instruction.length == 2U && bytes[0] == 0x0FU &&
        bytes[1] == 0xA9U)
    {
        *segment_register = 5U;
        return true;
    }
    return false;
}

bool ReadGuardedSegmentLoadRegisters(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    std::uint8_t* segment_register,
    std::uint8_t* gpr_register)
{
    if (operands == nullptr || segment_register == nullptr ||
        gpr_register == nullptr || instruction.mnemonic != ZYDIS_MNEMONIC_MOV ||
        instruction.operand_count_visible != 2U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        ZydisRegisterGetClass(operands[0].reg.value) !=
            ZYDIS_REGCLASS_SEGMENT ||
        ZydisRegisterGetClass(operands[1].reg.value) !=
            ZYDIS_REGCLASS_GPR16)
    {
        return false;
    }
    switch (operands[0].reg.value)
    {
        case ZYDIS_REGISTER_ES: *segment_register = 0U; break;
        case ZYDIS_REGISTER_DS: *segment_register = 3U; break;
        case ZYDIS_REGISTER_FS: *segment_register = 4U; break;
        case ZYDIS_REGISTER_GS: *segment_register = 5U; break;
        default: return false;
    }
    const std::int8_t gpr = ZydisRegisterGetId(operands[1].reg.value);
    if (gpr < 0 || gpr > 7 || gpr == 4)
    {
        return false;
    }
    *gpr_register = static_cast<std::uint8_t>(gpr);
    return true;
}

bool ReadGuardedSegmentReadRegisters(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    std::uint8_t* segment_register,
    std::uint8_t* gpr_register)
{
    if (operands == nullptr || segment_register == nullptr ||
        gpr_register == nullptr || instruction.mnemonic != ZYDIS_MNEMONIC_MOV ||
        instruction.operand_count_visible != 2U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        ZydisRegisterGetClass(operands[0].reg.value) !=
            ZYDIS_REGCLASS_GPR32 ||
        ZydisRegisterGetClass(operands[1].reg.value) !=
            ZYDIS_REGCLASS_SEGMENT)
    {
        return false;
    }
    const std::int8_t gpr = ZydisRegisterGetId(operands[0].reg.value);
    if (gpr < 0 || gpr > 7)
    {
        return false;
    }
    switch (operands[1].reg.value)
    {
        case ZYDIS_REGISTER_ES: *segment_register = 0U; break;
        case ZYDIS_REGISTER_SS: *segment_register = 2U; break;
        case ZYDIS_REGISTER_DS: *segment_register = 3U; break;
        case ZYDIS_REGISTER_FS: *segment_register = 4U; break;
        case ZYDIS_REGISTER_GS: *segment_register = 5U; break;
        default: return false;
    }
    *gpr_register = static_cast<std::uint8_t>(gpr);
    return true;
}
bool ReadGuardedPortIoInstruction(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands)
{
    if (operands == nullptr)
    {
        return false;
    }
    if (instruction.mnemonic == ZYDIS_MNEMONIC_IN || instruction.mnemonic == ZYDIS_MNEMONIC_OUT)
    {
        for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
        {
            if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                operands[i].reg.value == ZYDIS_REGISTER_DX)
            {
                return true;
            }
        }
    }
    return false;
}

bool IsHleBoundary(const ZydisDecodedInstruction& instruction,
                   const ZydisDecodedOperand* operands)
{
    // A push of a segment register only reads the selector and writes it to the
    // (flat) stack -- it needs no HLE. The single-step boundary already executes
    // it natively, pushing the host selector, so translating it (kCopy) is
    // behavior-identical and removes the exception round-trip (Task 264 Phase 1).
    // The Task 264 probe confirmed the host segment register is what the
    // single-step path pushes; matching it exactly avoids any behavior change.
    if (instruction.mnemonic == ZYDIS_MNEMONIC_PUSH && operands != nullptr)
    {
        for (std::uint8_t index = 0;
             index < instruction.operand_count_visible; ++index)
        {
            if (operands[index].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                ZydisRegisterGetClass(operands[index].reg.value) ==
                    ZYDIS_REGCLASS_SEGMENT)
            {
                return false;
            }
        }
    }
    if ((instruction.attributes &
         (ZYDIS_ATTRIB_IS_PRIVILEGED | ZYDIS_ATTRIB_HAS_SEGMENT)) != 0U)
    {
        return true;
    }
    // NOTE (Task 264 Phase 2, reverted): translating `mov r32,Sreg` natively
    // (kCopy) regressed -- the guest stalled at the fatal-breakpoint idiom
    // 0x030F3438 with no rendering. Unlike `push seg` (proven native/host-value
    // by probe), the single-step path for a segment-register store returns the
    // *shadow* selector, so a native store of the host selector (0x2b) diverges.
    // Segment-register stores therefore stay HLE boundaries.
    if (operands != nullptr)
    {
        for (std::uint8_t index = 0;
             index < instruction.operand_count_visible; ++index)
        {
            if (operands[index].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                ZydisRegisterGetClass(operands[index].reg.value) ==
                    ZYDIS_REGCLASS_SEGMENT)
            {
                return true;
            }
        }
    }
    switch (instruction.meta.category)
    {
        case ZYDIS_CATEGORY_INTERRUPT:
        case ZYDIS_CATEGORY_IO:
        case ZYDIS_CATEGORY_IOSTRINGOP:
        case ZYDIS_CATEGORY_RDWRFSGS:
        case ZYDIS_CATEGORY_SEGOP:
        case ZYDIS_CATEGORY_SYSCALL:
        case ZYDIS_CATEGORY_SYSRET:
        case ZYDIS_CATEGORY_SYSTEM:
        case ZYDIS_CATEGORY_UINTR:
            return true;
        default:
            return false;
    }
}

// Watcom emits switch statements as `cmp reg, imm` + `ja default` followed
// by `jmp dword ptr cs:[reg*4 + table]`. The guard records the fallthrough
// address and bound so the table branch can be translated natively.
struct JumpTableGuard
{
    std::uint8_t index_register = 0xFFU;
    std::uint32_t entry_count = 0;
    bool requires_low_byte_normalization = false;
};

// The emitted slot is `jmp [reg*4+disp32]` + INT3 + N dword entries and the
// address map stores its emitted length in one byte, so N is capped at 61.
constexpr std::uint32_t kMaximumJumpTableEntries = 61U;

bool ReadGuardRegisterId(ZydisRegister reg, std::uint8_t* register_id)
{
    if (register_id == nullptr ||
        ZydisRegisterGetClass(reg) != ZYDIS_REGCLASS_GPR32)
    {
        return false;
    }
    const std::int8_t id = ZydisRegisterGetId(reg);
    if (id < 0 || id > 7 || id == 4)
    {
        return false;
    }
    *register_id = static_cast<std::uint8_t>(id);
    return true;
}

bool ReadLowByteParentRegisterId(ZydisRegister reg,
                                 std::uint8_t* register_id)
{
    if (register_id == nullptr)
    {
        return false;
    }
    switch (reg)
    {
        case ZYDIS_REGISTER_AL: *register_id = 0U; return true;
        case ZYDIS_REGISTER_CL: *register_id = 1U; return true;
        case ZYDIS_REGISTER_DL: *register_id = 2U; return true;
        case ZYDIS_REGISTER_BL: *register_id = 3U; return true;
        default: return false;
    }
}

bool ReadJumpTableGuard(const ZydisDecoder& decoder,
                        const AotInstructionRecord& compare,
                        JumpTableGuard* guard)
{
    if (guard == nullptr || compare.kind != AotInstructionKind::kCopy ||
        static_cast<ZydisMnemonic>(compare.mnemonic) != ZYDIS_MNEMONIC_CMP)
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, compare.bytes.data(), compare.bytes.size(),
            &instruction, operands)) ||
        instruction.operand_count_visible < 2U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
    {
        return false;
    }
    const std::int64_t bound = operands[1].imm.is_signed
        ? operands[1].imm.value.s
        : static_cast<std::int64_t>(operands[1].imm.value.u);
    if (bound < 0 ||
        bound + 1 > static_cast<std::int64_t>(kMaximumJumpTableEntries))
    {
        return false;
    }
    if (ReadGuardRegisterId(
            operands[0].reg.value, &guard->index_register))
    {
        guard->requires_low_byte_normalization = false;
    }
    else if (ReadLowByteParentRegisterId(
                 operands[0].reg.value, &guard->index_register))
    {
        guard->requires_low_byte_normalization = true;
    }
    else
    {
        return false;
    }
    guard->entry_count = static_cast<std::uint32_t>(bound) + 1U;
    return true;
}

bool MatchLowByteJumpTableNormalization(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    std::uint8_t expected_register)
{
    if (operands == nullptr || instruction.mnemonic != ZYDIS_MNEMONIC_AND ||
        instruction.operand_count_visible != 2U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
    {
        return false;
    }
    std::uint8_t destination_register = 0xFFU;
    if (!ReadGuardRegisterId(
            operands[0].reg.value, &destination_register) ||
        destination_register != expected_register)
    {
        return false;
    }
    const std::int64_t mask = operands[1].imm.is_signed
        ? operands[1].imm.value.s
        : static_cast<std::int64_t>(operands[1].imm.value.u);
    return mask == 0xFF;
}

bool PropagateLowByteJumpTableGuard(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    std::uint32_t next,
    const JumpTableGuard& guard,
    std::unordered_map<std::uint32_t, JumpTableGuard>* guards)
{
    if (guards == nullptr || !guard.requires_low_byte_normalization ||
        !MatchLowByteJumpTableNormalization(
            instruction, operands, guard.index_register))
    {
        return false;
    }
    JumpTableGuard normalized = guard;
    normalized.requires_low_byte_normalization = false;
    return guards->emplace(next, normalized).second;
}

bool MatchJumpTableBranch(const ZydisDecodedInstruction& instruction,
                          const ZydisDecodedOperand* operands,
                          std::uint8_t* register_id,
                          std::uint32_t* table_address)
{
    if (operands == nullptr || register_id == nullptr ||
        table_address == nullptr ||
        instruction.mnemonic != ZYDIS_MNEMONIC_JMP ||
        instruction.meta.branch_type == ZYDIS_BRANCH_TYPE_FAR ||
        instruction.operand_count_visible == 0U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY)
    {
        return false;
    }
    const auto& memory = operands[0].mem;
    if (memory.base != ZYDIS_REGISTER_NONE || memory.scale != 4U ||
        !memory.disp.has_displacement || memory.disp.value <= 0 ||
        memory.disp.value > static_cast<std::int64_t>(UINT32_MAX))
    {
        return false;
    }
    if (memory.segment != ZYDIS_REGISTER_CS &&
        memory.segment != ZYDIS_REGISTER_DS)
    {
        return false;
    }
    if (!ReadGuardRegisterId(memory.index, register_id))
    {
        return false;
    }
    *table_address = static_cast<std::uint32_t>(memory.disp.value);
    return true;
}

bool ReadJumpTableTargets(const RelocatedRuntimeImage& image,
                          std::uint32_t table_address,
                          std::uint32_t entry_count,
                          std::vector<std::uint32_t>* targets)
{
    if (targets == nullptr || entry_count == 0U ||
        entry_count > kMaximumJumpTableEntries ||
        static_cast<std::uint64_t>(table_address) + entry_count * 4ULL >
            UINT32_MAX)
    {
        return false;
    }
    targets->clear();
    targets->reserve(entry_count);
    for (std::uint32_t index = 0; index < entry_count; ++index)
    {
        const std::uint8_t* entry_bytes =
            FindBytes(image, table_address + index * 4U, 4U);
        if (entry_bytes == nullptr)
        {
            return false;
        }
        std::uint32_t target = 0;
        std::memcpy(&target, entry_bytes, sizeof(target));
        if (FindBytes(image, target, 1U) == nullptr)
        {
            return false;
        }
        targets->push_back(target);
    }
    return true;
}

// A guarded table branch may be visited before its cmp/ja guard when the
// walk reaches the fallthrough first (e.g. the plan entry is the branch).
// The sweep re-decodes such records once the guard is known.
bool TryReclassifyJumpTable(
    const RelocatedRuntimeImage& image,
    const ZydisDecoder& decoder,
    const std::unordered_map<std::uint32_t, JumpTableGuard>& guards,
    AotInstructionRecord* record)
{
    if (record == nullptr ||
        (record->kind != AotInstructionKind::kHleBoundary &&
         record->kind != AotInstructionKind::kIndirectExit) ||
        record->bytes.empty())
    {
        return false;
    }
    const auto guard = guards.find(record->guest_address);
    if (guard == guards.end() ||
        guard->second.requires_low_byte_normalization)
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, record->bytes.data(), record->bytes.size(),
            &instruction, operands)))
    {
        return false;
    }
    std::uint8_t branch_register = 0xFFU;
    std::uint32_t table_address = 0;
    if (!MatchJumpTableBranch(instruction, operands,
                              &branch_register, &table_address) ||
        branch_register != guard->second.index_register ||
        !ReadJumpTableTargets(image, table_address,
                              guard->second.entry_count,
                              &record->table_targets))
    {
        return false;
    }
    record->table_index_register = branch_register;
    return true;
}

bool TryPropagateLowByteJumpTableGuard(
    const ZydisDecoder& decoder,
    const std::unordered_map<std::uint32_t, JumpTableGuard>& guards,
    const AotInstructionRecord& record,
    std::unordered_map<std::uint32_t, JumpTableGuard>* updated_guards)
{
    if (record.kind != AotInstructionKind::kCopy || record.bytes.empty() ||
        updated_guards == nullptr)
    {
        return false;
    }
    const auto guard = guards.find(record.guest_address);
    if (guard == guards.end() ||
        !guard->second.requires_low_byte_normalization)
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, record.bytes.data(), record.bytes.size(),
            &instruction, operands)))
    {
        return false;
    }
    return PropagateLowByteJumpTableGuard(
        instruction, operands, record.guest_address + record.length,
        guard->second, updated_guards);
}

bool IsExcludedGuestAddress(
    const std::vector<AotExcludedGuestRange>& ranges,
    std::uint32_t address)
{
    for (const AotExcludedGuestRange& range : ranges)
    {
        const std::uint64_t end =
            static_cast<std::uint64_t>(range.guest_address) +
            range.byte_count;
        if (range.byte_count != 0U &&
            address >= range.guest_address && address < end)
        {
            return true;
        }
    }
    return false;
}

// Task 330: one stage of a plan build. Closing is idempotent and happens in the
// destructor too, so the many `break` exits inside the walk cannot leak a stage.
// A null profile makes every operation a no-op and reads no timestamp.
enum class PlanBuildPhase
{
    kDecoderInit,
    kDecode,
    kRecordBuild,
    kClassify,
    kWalk,
    kSweep,
    kTotal,
};

std::uint64_t* PlanBuildPhaseBucket(AotPlanBuildProfile* profile,
                                    PlanBuildPhase phase)
{
    if (profile == nullptr)
    {
        return nullptr;
    }
    switch (phase)
    {
        case PlanBuildPhase::kDecoderInit:
            return &profile->decoder_init_cycles;
        case PlanBuildPhase::kDecode:
            return &profile->decode_cycles;
        case PlanBuildPhase::kRecordBuild:
            return &profile->record_build_cycles;
        case PlanBuildPhase::kClassify:
            return &profile->classify_cycles;
        case PlanBuildPhase::kWalk:
            return &profile->walk_cycles;
        case PlanBuildPhase::kSweep:
            return &profile->sweep_cycles;
        case PlanBuildPhase::kTotal:
            return &profile->total_cycles;
    }
    return nullptr;
}

class PlanBuildPhaseTimer
{
public:
    PlanBuildPhaseTimer(AotPlanBuildProfile* profile, PlanBuildPhase phase)
        : profile_(profile),
          bucket_(PlanBuildPhaseBucket(profile, phase)),
          start_(bucket_ != nullptr ? ReadCycleCounter() : 0U)
    {
    }
    PlanBuildPhaseTimer(const PlanBuildPhaseTimer&) = delete;
    PlanBuildPhaseTimer& operator=(const PlanBuildPhaseTimer&) = delete;
    ~PlanBuildPhaseTimer() { Close(); }

    // Returns the closing timestamp so the next stage starts from it instead of
    // reading the counter twice at one boundary.
    std::uint64_t Close()
    {
        if (bucket_ == nullptr)
        {
            return 0U;
        }
        const std::uint64_t end = ReadCycleCounter();
        *bucket_ += CycleDelta(start_, end, &profile_->clamped_sample_count);
        bucket_ = nullptr;
        return end;
    }

private:
    AotPlanBuildProfile* profile_;
    std::uint64_t* bucket_;
    std::uint64_t start_;
};

void AppendExcludedBoundary(std::uint32_t address,
                            AotTranslationPlan* plan,
                            AotBasicBlock* block)
{
    AotInstructionRecord record;
    record.guest_address = address;
    record.kind = AotInstructionKind::kHleBoundary;
    record.length = 1U;
    block->instructions.push_back(std::move(record));
    ++plan->instruction_count;
    ++plan->hle_boundary_count;
    ++plan->source_code_bytes;
    ++plan->estimated_emitted_bytes;
}

}  // namespace

bool BuildAotTranslationPlanFromEntry(const RelocatedRuntimeImage& image,
                                      std::uint32_t entry_address,
                                      const std::vector<
                                          AotExcludedGuestRange>&
                                          excluded_ranges,
                                      AotTranslationPlan* plan,
                                      AotPlanBuildProfile* profile)
{
    constexpr std::uint32_t kMaximumInstructions = 2'000'000U;
    constexpr std::uint32_t kMaximumBlockInstructions = 65'536U;
    if (plan == nullptr || !image.valid)
    {
        return false;
    }
    *plan = AotTranslationPlan{};
    plan->entry_address = entry_address;
    if (profile != nullptr)
    {
        *profile = AotPlanBuildProfile{};
        profile->enabled = true;
    }
    // Task 330: measured independently of the stages, so the derived residual
    // shows whether the partition covered the build.
    PlanBuildPhaseTimer total_timer(profile, PlanBuildPhase::kTotal);
    const auto started = std::chrono::steady_clock::now();
    ZydisDecoder decoder;
    {
        PlanBuildPhaseTimer decoder_init_timer(
            profile, PlanBuildPhase::kDecoderInit);
        if (!ZYAN_SUCCESS(ZydisDecoderInit(
                &decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                ZYDIS_STACK_WIDTH_32)))
        {
            decoder_init_timer.Close();
            plan->message = "failed to initialize Zydis legacy-32 decoder";
            return false;
        }
    }
    std::vector<std::uint32_t> pending{plan->entry_address};
    std::unordered_set<std::uint32_t> visited_blocks;
    std::unordered_set<std::uint32_t> visited_instructions;
    std::unordered_map<std::uint32_t, JumpTableGuard> jump_table_guards;
    bool sweep_jump_table_guards = true;
    while (sweep_jump_table_guards)
    {
        while (!pending.empty() && plan->instruction_count < kMaximumInstructions)
        {
            // Task 330: block-entry bookkeeping belongs to the same walk stage
            // as the per-instruction bookkeeping below.
            PlanBuildPhaseTimer block_walk_timer(profile,
                                                 PlanBuildPhase::kWalk);
            const std::uint32_t block_entry = pending.back();
            pending.pop_back();
            if (!visited_blocks.insert(block_entry).second)
            {
                continue;
            }
            if (IsExcludedGuestAddress(excluded_ranges, block_entry))
            {
                ++plan->block_count;
                plan->blocks.push_back(AotBasicBlock{});
                AotBasicBlock& block = plan->blocks.back();
                block.guest_address = block_entry;
                block_walk_timer.Close();
                PlanBuildPhaseTimer excluded_timer(
                    profile, PlanBuildPhase::kRecordBuild);
                AppendExcludedBoundary(block_entry, plan, &block);
                if (profile != nullptr)
                {
                    ++profile->record_count;
                }
                continue;
            }
            if (FindBytes(image, block_entry, 1U) == nullptr)
            {
                ++plan->outside_image_target_count;
                continue;
            }
            ++plan->block_count;
            plan->blocks.push_back(AotBasicBlock{});
            AotBasicBlock& block = plan->blocks.back();
            block.guest_address = block_entry;
            std::uint32_t address = block_entry;
            block_walk_timer.Close();
            for (std::uint32_t block_instruction = 0;
                 block_instruction < kMaximumBlockInstructions;
                 ++block_instruction)
            {
                // Task 330 stage 1: pending/visited structures, the excluded
                // range scan, and the object lookup.
                PlanBuildPhaseTimer walk_timer(profile, PlanBuildPhase::kWalk);
                if (IsExcludedGuestAddress(excluded_ranges, address))
                {
                    walk_timer.Close();
                    PlanBuildPhaseTimer excluded_timer(
                        profile, PlanBuildPhase::kRecordBuild);
                    AppendExcludedBoundary(address, plan, &block);
                    if (profile != nullptr)
                    {
                        ++profile->record_count;
                    }
                    break;
                }
                if (!visited_instructions.insert(address).second)
                {
                    break;
                }
                const std::uint8_t* bytes = FindBytes(
                    image, address, ZYDIS_MAX_INSTRUCTION_LENGTH);
                if (bytes == nullptr)
                {
                    ++plan->outside_image_target_count;
                    break;
                }
                walk_timer.Close();
                // Task 330 stage 2: operand-array initialization and the decode
                // itself, which is what "32us is large for Zydis" assumed.
                PlanBuildPhaseTimer decode_timer(profile,
                                                 PlanBuildPhase::kDecode);
                ZydisDecodedInstruction instruction{};
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
                const bool decoded = ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                        &decoder, bytes, ZYDIS_MAX_INSTRUCTION_LENGTH,
                        &instruction, operands)) && instruction.length != 0U;
                decode_timer.Close();
                if (profile != nullptr)
                {
                    ++profile->decode_count;
                }
                if (!decoded)
                {
                    ++plan->decode_failure_count;
                    break;
                }
                // Task 330 stage 3: the record and its per-instruction byte
                // copy, which is the only guaranteed heap allocation here.
                // Attribution caveat, fixed before measuring: the record's
                // `push_back` into the block happens inside each classification
                // branch, so vector growth is counted in `classify`, not here.
                PlanBuildPhaseTimer record_timer(profile,
                                                 PlanBuildPhase::kRecordBuild);
                ++plan->instruction_count;
                plan->source_code_bytes += instruction.length;
                plan->estimated_emitted_bytes += instruction.length;
                const std::uint32_t next = address + instruction.length;
                AotInstructionRecord record;
                record.guest_address = address;
                record.length = instruction.length;
                record.mnemonic = static_cast<std::uint16_t>(instruction.mnemonic);
                record.bytes.assign(bytes, bytes + instruction.length);
                record_timer.Close();
                if (profile != nullptr)
                {
                    ++profile->record_count;
                }
                // Task 330 stage 4: classification runs to the end of this
                // iteration on every path, so the timer closes in its
                // destructor rather than at each `break`.
                PlanBuildPhaseTimer classify_timer(profile,
                                                   PlanBuildPhase::kClassify);
                const auto guard = jump_table_guards.find(address);
                if (guard != jump_table_guards.end())
                {
                    const JumpTableGuard active_guard = guard->second;
                    if (active_guard.requires_low_byte_normalization)
                    {
                        PropagateLowByteJumpTableGuard(
                            instruction, operands, next, active_guard,
                            &jump_table_guards);
                    }
                    std::uint8_t branch_register = 0xFFU;
                    std::uint32_t table_address = 0;
                    if (!active_guard.requires_low_byte_normalization &&
                        MatchJumpTableBranch(instruction, operands,
                                             &branch_register, &table_address) &&
                        branch_register == active_guard.index_register &&
                        ReadJumpTableTargets(image, table_address,
                                             active_guard.entry_count,
                                             &record.table_targets))
                    {
                        record.kind = AotInstructionKind::kJumpTable;
                        record.table_index_register = branch_register;
                        ++plan->jump_table_count;
                        plan->jump_table_target_count += static_cast<std::uint32_t>(
                            record.table_targets.size());
                        plan->estimated_emitted_bytes +=
                            8U + 4U * record.table_targets.size();
                        for (const std::uint32_t target : record.table_targets)
                        {
                            pending.push_back(target);
                        }
                        block.instructions.push_back(std::move(record));
                        break;
                    }
                }
                std::uint8_t segment_override_register = 0xFFU;
                if (IsTranslatableSegmentOverrideMem(
                        instruction, operands, &segment_override_register))
                {
                    // Translated natively when the emitter can re-encode it;
                    // otherwise the emitter falls back to a boundary. Ends the
                    // block like a boundary so the emitted sequence carries its
                    // own fallthrough jump (Task 264 Phase 3a).
                    record.kind = AotInstructionKind::kSegmentOverrideMem;
                    record.segment_override_register = segment_override_register;
                    record.fallthrough_target = next;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 48U;
                    pending.push_back(next);
                    break;
                }
                std::uint8_t segment_register = 0xFFU;
                std::uint8_t gpr_register = 0xFFU;
                if (ReadGuardedSegmentLoadRegisters(
                        instruction, operands, &segment_register,
                        &gpr_register))
                {
                    record.kind = AotInstructionKind::kGuardedSegmentLoad;
                    record.segment_register = segment_register;
                    record.gpr_register = gpr_register;
                    record.fallthrough_target = next;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 42U;
                    pending.push_back(next);
                    break;
                }
                if (ReadGuardedSegmentReadRegisters(
                        instruction, operands, &segment_register,
                        &gpr_register))
                {
                    record.kind = AotInstructionKind::kGuardedSegmentRead;
                    record.segment_register = segment_register;
                    record.gpr_register = gpr_register;
                    record.fallthrough_target = next;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 31U;
                    pending.push_back(next);
                    break;
                }
                if (ReadGuardedSegmentPopRegister(
                        instruction, bytes, &segment_register))
                {
                    record.kind = AotInstructionKind::kGuardedSegmentPop;
                    record.segment_register = segment_register;
                    record.fallthrough_target = next;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 48U;
                    pending.push_back(next);
                    break;
                }
                if (ReadGuardedPortIoInstruction(instruction, operands))
                {
                    record.kind = AotInstructionKind::kPortIo;
                    record.fallthrough_target = next;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 14U;
                    pending.push_back(next);
                    break;
                }
                if (IsHleBoundary(instruction, operands))
                {
                    record.kind = AotInstructionKind::kHleBoundary;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 14U;
                    pending.push_back(next);
                    break;
                }
                const auto category = instruction.meta.category;
                if (category == ZYDIS_CATEGORY_RET)
                {
                    record.kind = AotInstructionKind::kReturn;
                    block.instructions.push_back(std::move(record));
                    ++plan->return_count;
                    break;
                }
                if (category == ZYDIS_CATEGORY_CALL ||
                    category == ZYDIS_CATEGORY_COND_BR ||
                    category == ZYDIS_CATEGORY_UNCOND_BR)
                {
                    std::uint32_t target = 0;
                    if (!ReadDirectTarget(
                            instruction, operands, address, &target))
                    {
                        record.kind = AotInstructionKind::kIndirectExit;
                        block.instructions.push_back(std::move(record));
                        ++plan->indirect_exit_count;
                        break;
                    }
                    record.direct_target = target;
                    record.fallthrough_target = next;
                    if (category == ZYDIS_CATEGORY_CALL)
                    {
                        record.kind = AotInstructionKind::kDirectCall;
                        ++plan->direct_call_count;
                        if (instruction.length < 5U)
                        {
                            plan->estimated_emitted_bytes +=
                                5U - instruction.length;
                        }
                        pending.push_back(target);
                        pending.push_back(next);
                    }
                    else if (category == ZYDIS_CATEGORY_COND_BR)
                    {
                        record.kind = AotInstructionKind::kConditionalBranch;
                        ++plan->conditional_branch_count;
                        if (instruction.length < 6U)
                        {
                            plan->estimated_emitted_bytes += 6U - instruction.length;
                        }
                        if (instruction.mnemonic == ZYDIS_MNEMONIC_JNBE &&
                            !block.instructions.empty())
                        {
                            JumpTableGuard table_guard;
                            if (ReadJumpTableGuard(decoder,
                                                   block.instructions.back(),
                                                   &table_guard))
                            {
                                jump_table_guards.emplace(next, table_guard);
                            }
                        }
                        pending.push_back(target);
                        pending.push_back(next);
                    }
                    else
                    {
                        record.kind = AotInstructionKind::kDirectJump;
                        ++plan->direct_jump_count;
                        if (instruction.length < 5U)
                        {
                            plan->estimated_emitted_bytes += 5U - instruction.length;
                        }
                        pending.push_back(target);
                    }
                    block.instructions.push_back(std::move(record));
                    break;
                }
                record.kind = AotInstructionKind::kCopy;
                block.instructions.push_back(std::move(record));
                ++plan->copy_instruction_count;
                address = next;
            }
        }
        sweep_jump_table_guards = false;
        if (plan->instruction_count < kMaximumInstructions)
        {
            // Task 330 stage 5: one pass re-walks every record of every block,
            // and a reclassification schedules another pass. The pass and visit
            // counts here are the first measurement of how often that happens.
            PlanBuildPhaseTimer sweep_timer(profile, PlanBuildPhase::kSweep);
            if (profile != nullptr)
            {
                ++profile->sweep_pass_count;
            }
            for (AotBasicBlock& swept_block : plan->blocks)
            {
                for (AotInstructionRecord& swept_record :
                     swept_block.instructions)
                {
                    if (profile != nullptr)
                    {
                        ++profile->sweep_record_visit_count;
                    }
                    if (TryPropagateLowByteJumpTableGuard(
                            decoder, jump_table_guards, swept_record,
                            &jump_table_guards))
                    {
                        sweep_jump_table_guards = true;
                    }
                    if (!TryReclassifyJumpTable(image, decoder,
                                                jump_table_guards,
                                                &swept_record))
                    {
                        continue;
                    }
                    if (swept_record.kind == AotInstructionKind::kHleBoundary)
                    {
                        --plan->hle_boundary_count;
                    }
                    else
                    {
                        --plan->indirect_exit_count;
                    }
                    swept_record.kind = AotInstructionKind::kJumpTable;
                    ++plan->jump_table_count;
                    plan->jump_table_target_count +=
                        static_cast<std::uint32_t>(
                            swept_record.table_targets.size());
                    plan->estimated_emitted_bytes +=
                        8U + 4U * swept_record.table_targets.size();
                    for (const std::uint32_t target :
                         swept_record.table_targets)
                    {
                        pending.push_back(target);
                    }
                    sweep_jump_table_guards = true;
                }
            }
        }
    }
    if (!pending.empty())
    {
        ++plan->analysis_limit_count;
    }
    plan->elapsed_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    plan->valid = plan->instruction_count != 0U;
    plan->message = plan->valid
        ? "AOT translation plan is ready"
        : "AOT translation plan contains no reachable instructions";
    return plan->valid;
}

bool BuildAotTranslationPlanFromEntry(const RelocatedRuntimeImage& image,
                                      std::uint32_t entry_address,
                                      AotTranslationPlan* plan)
{
    return BuildAotTranslationPlanFromEntry(
        image, entry_address, {}, plan);
}

bool BuildAotTranslationPlan(const RelocatedRuntimeImage& image,
                             AotTranslationPlan* plan)
{
    return BuildAotTranslationPlanFromEntry(
        image, image.relocated_entry_linear_address, plan);
}

}  // namespace repiu::runtime
