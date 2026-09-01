// Task 514, web port Stage 2. Counts the x86 instructions the guest uses, so
// the size of the Stage 3 interpreter stops being a guess.
//
// This tool discovers nothing. `runtime::BuildAotTranslationPlan` already walks
// the guest's control-flow graph by recursive descent, decodes with Zydis in
// 32-bit legacy mode, and records the mnemonic and the original bytes for every
// instruction it reaches. What was missing was a reporter -- the same shape as
// Task 512, where the counters were already filled and nothing printed them.
//
// Building a second disassembler would have been the wrong instrument. Its
// discovery reach would differ from the engine's, and then the census would be
// measuring the tool rather than the guest. What Stage 3 has to implement is
// what the engine actually translates.
//
// See docs/design/20260828-514-guest-instruction-census.md.

#include "repiu/exe/dos4gw_loader.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_long_mode_compatibility.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/target/target_profile.h"

#include <Zydis.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// The relocated base the analyzer already uses, kept identical so the two tools
// describe the same image.
constexpr std::uint32_t kRelocatedImageBase = 0x01000000U;

// LE object flag bit for executable. Only those objects are swept linearly; a
// data object decoded as code would inflate the upper bound past any use.
constexpr std::uint32_t kLeObjectExecutable = 0x0004U;

struct MnemonicTally
{
    std::uint64_t instructions = 0;
    std::set<std::string> forms;
};

// Task 560. Named rather than numbered, because the point of the breakdown is
// to be read. A kind this does not know about must say so loudly rather than
// silently joining another bucket -- a new record kind added later would
// otherwise appear as one of these and be counted as work already understood.
const char* AotInstructionKindName(const repiu::runtime::AotInstructionKind kind)
{
    using repiu::runtime::AotInstructionKind;
    switch (kind)
    {
        case AotInstructionKind::kCopy: return "kCopy";
        case AotInstructionKind::kDirectCall: return "kDirectCall";
        case AotInstructionKind::kDirectJump: return "kDirectJump";
        case AotInstructionKind::kConditionalBranch:
            return "kConditionalBranch";
        case AotInstructionKind::kReturn: return "kReturn";
        case AotInstructionKind::kHleBoundary: return "kHleBoundary";
        case AotInstructionKind::kIndirectExit: return "kIndirectExit";
        case AotInstructionKind::kJumpTable: return "kJumpTable";
        case AotInstructionKind::kSegmentOverrideMem:
            return "kSegmentOverrideMem";
        case AotInstructionKind::kGuardedSegmentRead:
            return "kGuardedSegmentRead";
        case AotInstructionKind::kGuardedSegmentLoad:
            return "kGuardedSegmentLoad";
        case AotInstructionKind::kGuardedSegmentPop:
            return "kGuardedSegmentPop";
        case AotInstructionKind::kPortIo: return "kPortIo";
    }
    return "kUnknown";
}

// Task 560. Whether the emitter can spell this condition. A `Jcc` whose
// mnemonic has no `0F 8x` here is a boundary on both hosts, so the census must
// ask the same question the emitter asks rather than counting every
// `kConditionalBranch` as emitted.
bool ReadsConditionOpcode(const std::uint16_t mnemonic)
{
    switch (static_cast<ZydisMnemonic>(mnemonic))
    {
        case ZYDIS_MNEMONIC_JO: case ZYDIS_MNEMONIC_JNO:
        case ZYDIS_MNEMONIC_JB: case ZYDIS_MNEMONIC_JNB:
        case ZYDIS_MNEMONIC_JZ: case ZYDIS_MNEMONIC_JNZ:
        case ZYDIS_MNEMONIC_JBE: case ZYDIS_MNEMONIC_JNBE:
        case ZYDIS_MNEMONIC_JS: case ZYDIS_MNEMONIC_JNS:
        case ZYDIS_MNEMONIC_JP: case ZYDIS_MNEMONIC_JNP:
        case ZYDIS_MNEMONIC_JL: case ZYDIS_MNEMONIC_JNL:
        case ZYDIS_MNEMONIC_JLE: case ZYDIS_MNEMONIC_JNLE:
            return true;
        default:
            return false;
    }
}

// Task 556. What the x64 emitter can produce from this guest, and what stops
// the rest.
//
// The totals here are deliberately NOT the authority: the image is built with
// `enable_long_mode_emission` and the emitter's own counters are the headline,
// because a census that reimplements the emission rule drifts from it the first
// time the rule changes. These tallies exist to explain those counters, and
// whether the two agree is printed rather than assumed.
struct LongModeTally
{
    std::uint64_t considered = 0;
    std::uint64_t copied = 0;
    std::uint64_t lowered = 0;
    // Task 560. Direct jumps and conditional branches, emitted as themselves.
    // Kept apart from `copied` because they are not copies of the guest's bytes
    // -- the opcode survives but the displacement is rewritten for the cache.
    std::uint64_t branches = 0;
    // Task 562. Returns, emitted as a slot that asks a resolver at run time.
    // Apart from `branches` because a branch resolves when the image is built
    // and a return cannot: its target is not known until the guest runs.
    std::uint64_t returns = 0;
    // Task 568. Segment-override slots, kept apart from `branches` and
    // `returns` because unlike either they are not finished when emitted: the
    // engine patches the guard and the folded base before the cache runs.
    std::uint64_t segment_overrides = 0;
    std::uint64_t guarded_segment_loads = 0;
    std::uint64_t refused = 0;
    // Refused because the plan record is not `kCopy` at all -- every control
    // flow record, every guarded segment slot, every port I/O record. Counted
    // apart from the classifier's refusals because no lowering of an
    // instruction's bytes would change it; it needs an x64 slot instead.
    std::uint64_t refused_non_copy = 0;
    // Task 560. Which record kinds those are, because "not a kCopy record" is
    // the largest refusal there is and naming it that way says nothing about
    // what to build next. A block terminator and a port I/O slot are both in
    // here, and they are not the same unit of work.
    std::map<std::string, std::uint64_t> refused_kinds;
    std::map<std::string, std::uint64_t> refusal_reasons;
    // The mnemonics behind the refusals, by volume, so the next unit is chosen
    // from data rather than from an impression of what is common.
    std::map<std::string, std::uint64_t> refused_mnemonics;
    std::uint64_t blocks = 0;
    std::uint64_t blocks_complete = 0;
    // Task 563. Reachability from the entry, which is a different question
    // from coverage: a chain stops at the first block it cannot complete
    // regardless of how much of the image is emittable.
    std::set<std::uint32_t> complete_blocks;
    std::uint64_t reachable_blocks = 0;
    std::uint64_t reachable_instructions = 0;
    // Task 565. Blocks reachable once serviced boundaries are walked through.
    // "Reachable with no runtime help" and "reachable if the dispatcher does
    // its job" are different claims and both are worth having.
    std::uint64_t reachable_serviced_blocks = 0;
    // Where a chain stopped, by the kind of the record that stopped it. This is
    // the list the next unit should be chosen from -- not the image-wide
    // refusal counts, which say what is missing rather than what is in the way.
    std::map<std::string, std::uint64_t> frontier_kinds;
    std::uint64_t frontier_edges_outside_plan = 0;
    // Where the first chain stopped, so the claim can be checked in a
    // disassembler rather than taken on trust.
    std::uint32_t frontier_first_address = 0;
    std::uint32_t walk_entry_address = 0;
    std::vector<std::uint8_t> frontier_first_bytes;
};


const char* DivergenceName(const repiu::runtime::LongModeDivergence divergence)
{
    switch (divergence)
    {
        case repiu::runtime::LongModeDivergence::kNone:
            return "unproven";
        case repiu::runtime::LongModeDivergence::kSilentlyDifferent:
            return "silently-different";
        case repiu::runtime::LongModeDivergence::kInvalidInLongMode:
            return "invalid-in-long-mode";
        case repiu::runtime::LongModeDivergence::kOperandWidth:
            return "operand-width";
        case repiu::runtime::LongModeDivergence::kAddressSize:
            return "address-size";
        case repiu::runtime::LongModeDivergence::kRipRelativeDisplacement:
            return "rip-relative";
        case repiu::runtime::LongModeDivergence::kSegmentRegister:
            return "segment-register";
        case repiu::runtime::LongModeDivergence::kStackPointerRegister:
            return "stack-pointer";
    }
    return "unknown";
}

// The emitter's `kCopy` rule, mirrored: classify, and if a lowering is named,
// require that it actually produces bytes. A named lowering the rewriter
// declines is still a refusal, which is what the emitter does too.
bool ClassifyEmittable(const repiu::runtime::AotInstructionRecord& record,
                       bool* lowered, std::string* reason)
{
    const repiu::runtime::LongModeCompatibilityResult verdict =
        repiu::runtime::ClassifyLongModeBytes(record.bytes.data(),
                                              record.bytes.size());
    *lowered = false;
    if (verdict.compatibility ==
        repiu::runtime::LongModeByteCompatibility::kIdenticalBytes)
    {
        return true;
    }
    if (verdict.lowering != repiu::runtime::LongModeLowering::kNone)
    {
        std::uint8_t bytes[repiu::runtime::kMaxLoweredBytes] = {};
        std::size_t count = 0;
        if (repiu::runtime::LowerLongModeBytes(record.bytes.data(),
                                               record.bytes.size(), bytes,
                                               &count) && count != 0U)
        {
            *lowered = true;
            return true;
        }
        *reason = std::string(DivergenceName(verdict.divergence)) +
            "/lowering-declined";
        return false;
    }
    *reason = DivergenceName(verdict.divergence);
    return false;
}
// Task 565. Whether the emitter produces this record, asked in one place.
//
// It was written out twice inside the walk before -- once to decide whether a
// block was traversable and once to name what stopped it -- and two copies of
// the emission rule is exactly the drift the census's `agrees=` line exists to
// catch elsewhere.
bool RecordIsEmitted(const repiu::runtime::AotInstructionRecord& record)
{
    using repiu::runtime::AotInstructionKind;
    switch (record.kind)
    {
        case AotInstructionKind::kCopy:
        {
            bool lowered = false;
            std::string reason;
            return !record.bytes.empty() &&
                ClassifyEmittable(record, &lowered, &reason);
        }
        case AotInstructionKind::kDirectJump:
        case AotInstructionKind::kDirectCall:
            return true;
        case AotInstructionKind::kConditionalBranch:
            return ReadsConditionOpcode(record.mnemonic);
        case AotInstructionKind::kReturn:
            return repiu::runtime::LongModeReturnDispatchAvailable();
        case AotInstructionKind::kSegmentOverrideMem:
            // Task 568. Emitted when the slot admits its shape, and patched by
            // the engine before the cache runs -- the same contract the i386
            // slot has always had.
            return repiu::runtime::LongModeSegmentOverrideEmittable(record);
        case AotInstructionKind::kGuardedSegmentLoad:
            return repiu::runtime::LongModeGuardedSegmentLoadEmittable(record);
        default:
            return false;
    }
}

// Task 563. Walks the blocks execution could actually reach from the entry.
//
// Stops at a block that is not complete and records the kind that stopped it,
// which is the question "what is in the way" rather than "what is missing".
void WalkReachable(const repiu::runtime::AotTranslationPlan& plan,
                   const std::set<std::uint32_t>& complete,
                   LongModeTally* tally)
{
    using repiu::runtime::AotBasicBlock;
    using repiu::runtime::AotInstructionKind;

    std::map<std::uint32_t, const AotBasicBlock*> by_address;
    for (const AotBasicBlock& block : plan.blocks)
    {
        by_address.emplace(block.guest_address, &block);
    }

    tally->walk_entry_address = plan.entry_address;
    std::set<std::uint32_t> visited;
    std::vector<std::uint32_t> pending{plan.entry_address};
    while (!pending.empty())
    {
        const std::uint32_t address = pending.back();
        pending.pop_back();
        if (!visited.insert(address).second)
        {
            continue;
        }
        const auto found = by_address.find(address);
        if (found == by_address.end())
        {
            // An edge the planner never gave a block. Counted rather than
            // ignored: it is a hole in the plan, not in the emitter.
            ++tally->frontier_edges_outside_plan;
            continue;
        }
        const AotBasicBlock& block = *found->second;
        // Task 565. A boundary is a door, not a wall.
        //
        // The walk as Task 563 wrote it stopped at every record the emitter did
        // not produce, which conflated two different things. An `INT 21h` is
        // never going to be emitted -- it is a DOS service and belongs to the
        // HLE dispatcher by design -- and on i386 the handler services it and
        // execution carries on at the next instruction. Counting that as the
        // end of a chain measures the emitter for something that was never its
        // job.
        //
        // So a block whose only unemitted records are serviced kinds is walked
        // through, and counted apart: "reachable if boundaries are serviced" is
        // a different claim from "reachable with no runtime help at all", and
        // both are worth having.
        const bool block_complete = complete.find(address) != complete.end();
        bool serviceable = true;
        if (!block_complete)
        {
            for (const repiu::runtime::AotInstructionRecord& record :
                 block.instructions)
            {
                if (record.kind == AotInstructionKind::kHleBoundary ||
                    record.kind == AotInstructionKind::kPortIo)
                {
                    continue;
                }
                if (!RecordIsEmitted(record))
                {
                    serviceable = false;
                    break;
                }
            }
        }
        // An empty block is not serviceable, it is nothing -- and asking for
        // its tail is what crashed the first run of this.
        if (!block_complete && serviceable && !block.instructions.empty())
        {
            ++tally->reachable_serviced_blocks;
            const repiu::runtime::AotInstructionRecord& tail =
                block.instructions.back();
            // Same successor rules as below; a serviced boundary does not end
            // the block, so its tail decides where control goes next.
            switch (tail.kind)
            {
                case AotInstructionKind::kConditionalBranch:
                    pending.push_back(tail.direct_target);
                    pending.push_back(tail.guest_address + tail.length);
                    break;
                case AotInstructionKind::kDirectJump:
                    pending.push_back(tail.direct_target);
                    break;
                case AotInstructionKind::kDirectCall:
                    pending.push_back(tail.direct_target);
                    pending.push_back(tail.fallthrough_target);
                    break;
                case AotInstructionKind::kReturn:
                    break;
                default:
                    pending.push_back(tail.guest_address + tail.length);
                    break;
            }
            continue;
        }
        if (!block_complete)
        {
            // The first record this host cannot emit is what stopped the chain.
            for (const repiu::runtime::AotInstructionRecord& record :
                 block.instructions)
            {
                if (!RecordIsEmitted(record))
                {
                    // Keyed by the reason as well as the kind, for Task 557's
                    // reason: "a kCopy was refused" does not say which unit
                    // would clear it, and the point of this table is to choose
                    // one.
                    std::string key = AotInstructionKindName(record.kind);
                    if (record.kind == AotInstructionKind::kCopy)
                    {
                        bool lowered = false;
                        std::string reason;
                        ClassifyEmittable(record, &lowered, &reason);
                        key += "  " + (reason.empty() ? "no-bytes" : reason);
                    }
                    if (tally->frontier_first_address == 0U)
                    {
                        tally->frontier_first_address = record.guest_address;
                        // The bytes too. Task 565 spent a round guessing which
                        // encoding was in the way and guessed wrong; the bytes
                        // turn that into a fact costing nothing.
                        tally->frontier_first_bytes = record.bytes;
                    }
                    ++tally->frontier_kinds[key];
                    break;
                }
            }
            continue;
        }

        ++tally->reachable_blocks;
        tally->reachable_instructions += block.instructions.size();
        if (block.instructions.empty())
        {
            continue;
        }
        const repiu::runtime::AotInstructionRecord& tail =
            block.instructions.back();
        switch (tail.kind)
        {
            case AotInstructionKind::kConditionalBranch:
                pending.push_back(tail.direct_target);
                pending.push_back(tail.guest_address + tail.length);
                break;
            case AotInstructionKind::kDirectJump:
                pending.push_back(tail.direct_target);
                break;
            case AotInstructionKind::kDirectCall:
                pending.push_back(tail.direct_target);
                pending.push_back(tail.fallthrough_target);
                break;
            case AotInstructionKind::kReturn:
                // Nowhere static to go; the caller's fallthrough is already in.
                break;
            default:
                pending.push_back(tail.guest_address + tail.length);
                break;
        }
    }
}

struct X87Tally
{
    std::uint64_t total = 0;
    // The three that decide design 514's question. An 80-bit value that reaches
    // memory is visible to the guest as a format, and no f64 reproduces it.
    std::uint64_t float80_memory_operands = 0;
    std::uint64_t control_word_access = 0;
    std::uint64_t environment_save_restore = 0;
    std::map<std::string, std::uint64_t> mnemonics;
};

bool ReadBinaryFile(const std::filesystem::path& path,
                    std::vector<std::uint8_t>* data,
                    std::string* error_message)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error_message = "failed to open " + path.string();
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    if (size < 0)
    {
        *error_message = "failed to size " + path.string();
        return false;
    }
    stream.seekg(0, std::ios::beg);
    data->resize(static_cast<std::size_t>(size));
    if (size > 0 &&
        !stream.read(reinterpret_cast<char*>(data->data()), size))
    {
        *error_message = "failed to read " + path.string();
        return false;
    }
    return true;
}

// The operand signature is what an interpreter actually implements. `MOV` is
// one mnemonic and dozens of forms, and each form is its own code, so level A
// alone would under-state Stage 3 by an order of magnitude.
//
// Only explicit operands are named. Zydis also reports the hidden ones -- the
// flags register, the implicit stack pointer of a PUSH -- and those follow from
// the mnemonic rather than adding a form to write.
std::string BuildOperandSignature(const ZydisDecodedInstruction& instruction,
                                  const ZydisDecodedOperand* operands)
{
    std::string signature;
    for (ZyanU8 index = 0; index < instruction.operand_count_visible; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if (operand.visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
        {
            continue;
        }
        if (!signature.empty())
        {
            signature += ',';
        }
        switch (operand.type)
        {
        case ZYDIS_OPERAND_TYPE_REGISTER:
            signature += 'r';
            break;
        case ZYDIS_OPERAND_TYPE_MEMORY:
            signature += 'm';
            break;
        case ZYDIS_OPERAND_TYPE_POINTER:
            signature += 'p';
            break;
        case ZYDIS_OPERAND_TYPE_IMMEDIATE:
            signature += 'i';
            break;
        default:
            signature += '?';
            break;
        }
        signature += std::to_string(static_cast<unsigned>(operand.size));
    }
    if (signature.empty())
    {
        signature = "-";
    }
    return signature;
}

bool IsFloat80Memory(const ZydisDecodedOperand& operand)
{
    return operand.type == ZYDIS_OPERAND_TYPE_MEMORY &&
           (operand.size == 80U ||
            operand.element_type == ZYDIS_ELEMENT_TYPE_FLOAT80);
}

void AccumulateX87(const ZydisDecodedInstruction& instruction,
                   const ZydisDecodedOperand* operands,
                   X87Tally* tally)
{
    if (instruction.meta.isa_set != ZYDIS_ISA_SET_X87)
    {
        return;
    }
    ++tally->total;
    ++tally->mnemonics[ZydisMnemonicGetString(instruction.mnemonic)];

    for (ZyanU8 index = 0; index < instruction.operand_count_visible; ++index)
    {
        if (IsFloat80Memory(operands[index]))
        {
            ++tally->float80_memory_operands;
        }
    }

    switch (instruction.mnemonic)
    {
    case ZYDIS_MNEMONIC_FLDCW:
    case ZYDIS_MNEMONIC_FNSTCW:
        ++tally->control_word_access;
        break;
    case ZYDIS_MNEMONIC_FNSAVE:
    case ZYDIS_MNEMONIC_FRSTOR:
    case ZYDIS_MNEMONIC_FNSTENV:
    case ZYDIS_MNEMONIC_FLDENV:
        ++tally->environment_save_restore;
        break;
    default:
        break;
    }
}

// The upper bound. A linear sweep decodes whatever sits in the executable
// object, data included, so its distinct-mnemonic count is larger than the truth
// -- exactly as the recursive-descent count is smaller than it. Neither is the
// answer; the gap between them is what is honestly known.
struct LinearSweep
{
    std::uint64_t decoded = 0;
    std::uint64_t failed = 0;
    std::set<std::string> mnemonics;
};

LinearSweep SweepObjectLinearly(const ZydisDecoder& decoder,
                                const std::vector<std::uint8_t>& memory)
{
    LinearSweep sweep;
    std::size_t offset = 0;
    while (offset < memory.size())
    {
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder, memory.data() + offset, memory.size() - offset,
                &instruction, operands)))
        {
            ++sweep.decoded;
            sweep.mnemonics.insert(ZydisMnemonicGetString(instruction.mnemonic));
            offset += instruction.length;
            continue;
        }
        // One byte, not one instruction: a failed decode says nothing about
        // where the next instruction starts, and skipping further would hide
        // code behind whatever byte happened to be undecodable.
        ++sweep.failed;
        ++offset;
    }
    return sweep;
}

const repiu::target::TargetProfile* SelectTargetProfile(
    int argc,
    char** argv,
    std::filesystem::path* explicit_path)
{
    const repiu::target::TargetProfile* default_profile =
        repiu::target::FindTargetProfileById("pumpit1");
    if (argc < 2)
    {
        return default_profile;
    }
    const std::string_view first_arg(argv[1]);
    const repiu::target::TargetProfile* selected =
        repiu::target::FindTargetProfileById(first_arg);
    if (selected != nullptr)
    {
        if (argc >= 3 && explicit_path != nullptr)
        {
            *explicit_path = std::filesystem::path(argv[2]);
        }
        return selected;
    }
    if (explicit_path != nullptr)
    {
        *explicit_path = std::filesystem::path(argv[1]);
    }
    return default_profile;
}

void PrintUsage()
{
    std::cerr << "usage: repiu_instruction_census [target-id] <PIU.EXE>\n"
                 "Counts the x86 instructions reachable from the guest entry "
                 "point.\n";
}

}  // namespace

int main(int argc, char** argv)
{
    std::filesystem::path path;
    const repiu::target::TargetProfile* profile =
        SelectTargetProfile(argc, argv, &path);
    if (path.empty() || profile == nullptr)
    {
        PrintUsage();
        return 2;
    }

    std::vector<std::uint8_t> data;
    std::string read_error;
    if (!ReadBinaryFile(path, &data, &read_error))
    {
        std::cerr << read_error << "\n";
        return 1;
    }

    repiu::exe::ParseError error;
    repiu::exe::Dos4gwLoadResult load_result;
    if (!repiu::exe::LoadDos4gwExecutable(data, *profile, &load_result, &error))
    {
        std::cerr << "load failed: " << error.message << "\n";
        return 1;
    }

    repiu::runtime::RuntimeMemoryPlan runtime_plan;
    if (!repiu::runtime::BuildRuntimeMemoryPlan(load_result, &runtime_plan,
                                                &error))
    {
        std::cerr << "runtime plan failed: " << error.message << "\n";
        return 1;
    }

    repiu::runtime::RelocatableRuntimeImagePlan relocatable_plan;
    if (!repiu::runtime::BuildRelocatableRuntimeImagePlan(
            load_result, kRelocatedImageBase, &relocatable_plan, &error))
    {
        std::cerr << "relocatable plan failed: " << error.message << "\n";
        return 1;
    }

    repiu::runtime::RelocatedRuntimeImage image;
    if (!repiu::runtime::BuildRelocatedRuntimeImage(
            load_result, relocatable_plan, &image, &error))
    {
        std::cerr << "relocated image failed: " << error.message << "\n";
        return 1;
    }

    repiu::runtime::AotTranslationPlan plan;
    if (!repiu::runtime::BuildAotTranslationPlan(image, &plan) || !plan.valid)
    {
        std::cerr << "translation plan failed: " << plan.message << "\n";
        return 1;
    }

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                                       ZYDIS_STACK_WIDTH_32)))
    {
        std::cerr << "failed to initialize Zydis legacy-32 decoder\n";
        return 1;
    }

    std::map<std::string, MnemonicTally> tallies;
    std::set<std::string> forms;
    X87Tally x87;
    std::uint64_t counted = 0;
    std::uint64_t redecode_failures = 0;

    for (const repiu::runtime::AotBasicBlock& block : plan.blocks)
    {
        for (const repiu::runtime::AotInstructionRecord& record :
             block.instructions)
        {
            if (record.bytes.empty())
            {
                continue;
            }
            ZydisDecodedInstruction instruction{};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                    &decoder, record.bytes.data(), record.bytes.size(),
                    &instruction, operands)))
            {
                // The plan decoded these bytes once already, so a failure here
                // is a contradiction rather than an ordinary miss. Counted and
                // printed instead of skipped.
                ++redecode_failures;
                continue;
            }
            ++counted;

            const std::string mnemonic =
                ZydisMnemonicGetString(instruction.mnemonic);
            const std::string form =
                mnemonic + " " + BuildOperandSignature(instruction, operands);
            MnemonicTally& tally = tallies[mnemonic];
            ++tally.instructions;
            tally.forms.insert(form);
            forms.insert(form);

            AccumulateX87(instruction, operands, &x87);
        }
    }

    // Task 556. The same plan through the x64 emitter's rules.
    //
    // The emitter's dedup is mirrored here -- it skips a guest address it has
    // already emitted -- so that the totals below can be compared with the
    // image's own counters. Without that the comparison fails for a reason
    // that has nothing to do with what is being measured.
    LongModeTally long_mode;
    {
        std::set<std::uint32_t> seen;
        for (const repiu::runtime::AotBasicBlock& block : plan.blocks)
        {
            ++long_mode.blocks;
            bool complete = !block.instructions.empty();
            for (const repiu::runtime::AotInstructionRecord& record :
                 block.instructions)
            {
                if (!seen.insert(record.guest_address).second)
                {
                    continue;
                }
                ++long_mode.considered;
                // Task 560. Two of the control-flow kinds are emitted now, so
                // they stop being refusals. Whether their *edge* resolves is
                // not knowable here -- that depends on what else landed in the
                // cache -- so this counts them as emitted and the emitter's own
                // `long_mode_unresolved_branch_count` reports the ones that had
                // to become boundaries after all. The `agrees=` line below is
                // what keeps the two from drifting.
                // Task 562. A return is emitted only where the dispatch thunk
                // exists, which is the first long-mode outcome that depends on
                // the host rather than on the instruction. The emitter is asked
                // rather than the `#if` copied, because a copy is what drifts.
                if (record.kind ==
                        repiu::runtime::AotInstructionKind::kReturn &&
                    repiu::runtime::LongModeReturnDispatchAvailable())
                {
                    ++long_mode.returns;
                    continue;
                }
                if (record.kind ==
                        repiu::runtime::AotInstructionKind::kDirectJump ||
                    // Task 561. A direct call is emitted as a lowered push plus
                    // the same jump, and the push is the stack sequence that
                    // already exists, so nothing here can decline it.
                    record.kind ==
                        repiu::runtime::AotInstructionKind::kDirectCall ||
                    (record.kind == repiu::runtime::AotInstructionKind::
                                        kConditionalBranch &&
                     ReadsConditionOpcode(record.mnemonic)))
                {
                    ++long_mode.branches;
                    continue;
                }
                // Task 568. Emitted as a guarded slot the engine patches. This
                // is the third place in this file that has had to learn a new
                // emitted kind, and `agrees=` caught the omission each time --
                // which is the argument for the line existing.
                if (record.kind == repiu::runtime::AotInstructionKind::
                                       kSegmentOverrideMem &&
                    repiu::runtime::LongModeSegmentOverrideEmittable(record))
                {
                    ++long_mode.segment_overrides;
                    continue;
                }
                if (record.kind == repiu::runtime::AotInstructionKind::
                                       kGuardedSegmentLoad &&
                    repiu::runtime::LongModeGuardedSegmentLoadEmittable(record))
                {
                    ++long_mode.guarded_segment_loads;
                    continue;
                }
                if (record.kind !=
                    repiu::runtime::AotInstructionKind::kCopy)
                {
                    ++long_mode.refused;
                    ++long_mode.refused_non_copy;
                    ++long_mode.refusal_reasons["not-a-copy-record"];
                    ++long_mode.refused_kinds[AotInstructionKindName(
                        record.kind)];
                    complete = false;
                    continue;
                }
                bool lowered = false;
                std::string reason;
                if (record.bytes.empty() ||
                    !ClassifyEmittable(record, &lowered, &reason))
                {
                    ++long_mode.refused;
                    ++long_mode.refusal_reasons[
                        reason.empty() ? "no-bytes" : reason];
                    complete = false;
                    ZydisDecodedInstruction refused_instruction{};
                    if (!record.bytes.empty() &&
                        ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(
                            &decoder, nullptr, record.bytes.data(),
                            record.bytes.size(), &refused_instruction)))
                    {
                        // Keyed by mnemonic *and* reason. Task 557 needed to
                        // know whether the 21 remaining `inc` were prefixed
                        // forms or memory-operand forms, and a bare mnemonic
                        // count could not say. The next lowerings will ask the
                        // same question of `push` and `mov`.
                        ++long_mode.refused_mnemonics[
                            std::string(ZydisMnemonicGetString(
                                refused_instruction.mnemonic)) + "  " + reason];
                    }
                    continue;
                }
                if (lowered)
                {
                    ++long_mode.lowered;
                }
                else
                {
                    ++long_mode.copied;
                }
            }
            if (complete)
            {
                ++long_mode.blocks_complete;
                long_mode.complete_blocks.insert(block.guest_address);
            }
        }
    }

    // Task 563. How far execution could get from the entry point.
    //
    // "86% of instructions and 64% of blocks" are counts over the whole image,
    // and an image is not a run. What decides whether anything executes is
    // reachability: a chain from the entry that stops at the first block it
    // cannot complete, however small a fraction of the image that block is.
    //
    // A call is followed both ways -- into the callee and on at the
    // fallthrough -- because that is what happens. A return is not followed at
    // all: its target is whatever called, and the caller's fallthrough was
    // already enqueued when the call was taken. That makes this an
    // under-approximation in one direction only, which is the safe one.
    WalkReachable(plan, long_mode.complete_blocks, &long_mode);

    // The authority. Building the image runs the emitter itself, so these three
    // cannot drift from what would actually be emitted.
    repiu::runtime::AotCodeCacheBuildOptions long_mode_options;
    long_mode_options.enable_long_mode_emission = true;
    repiu::runtime::AotCodeCacheImage long_mode_image;
    const bool long_mode_built = repiu::runtime::BuildAotCodeCacheImage(
        plan, long_mode_options, &long_mode_image);

    LinearSweep sweep;
    std::uint64_t swept_bytes = 0;
    for (const repiu::runtime::RelocatedRuntimeObject& object : image.objects)
    {
        if ((object.flags & kLeObjectExecutable) == 0U ||
            object.memory.empty())
        {
            continue;
        }
        swept_bytes += object.memory.size();
        const LinearSweep object_sweep =
            SweepObjectLinearly(decoder, object.memory);
        sweep.decoded += object_sweep.decoded;
        sweep.failed += object_sweep.failed;
        sweep.mnemonics.insert(object_sweep.mnemonics.begin(),
                               object_sweep.mnemonics.end());
    }

    std::cout << "== guest instruction census ==\n";
    std::cout << "target=" << profile->id << " path=" << path.string() << "\n";
    std::cout << "image_base=0x" << std::hex << std::uppercase
              << image.relocated_image_base << " entry=0x"
              << image.relocated_entry_linear_address << std::dec
              << std::nouppercase << "\n\n";

    std::cout << "-- level A/B: reachable from the entry point --\n";
    std::cout << "  blocks              " << plan.block_count << "\n";
    std::cout << "  instructions        " << plan.instruction_count << "\n";
    std::cout << "  counted here        " << counted << "\n";
    std::cout << "  distinct mnemonics  " << tallies.size() << "\n";
    std::cout << "  distinct forms      " << forms.size() << "\n\n";

    std::cout << "-- upper bound: linear sweep of the executable objects --\n";
    std::cout << "  bytes swept         " << swept_bytes << "\n";
    std::cout << "  decoded             " << sweep.decoded << "\n";
    std::cout << "  undecodable bytes   " << sweep.failed << "\n";
    std::cout << "  distinct mnemonics  " << sweep.mnemonics.size() << "\n\n";

    // "Between 120 and 193" is too weak to plan Stage 3 from. What makes the
    // range actionable is which mnemonics sit in the gap: an interpreter has to
    // cover the ones that are real code the recursive descent could not reach,
    // and can ignore the ones that are data decoded by accident. Naming them is
    // what lets a person tell the two apart -- a count never could.
    std::vector<std::string> sweep_only;
    std::vector<std::string> reachable_only;
    for (const std::string& mnemonic : sweep.mnemonics)
    {
        if (tallies.find(mnemonic) == tallies.end())
        {
            sweep_only.push_back(mnemonic);
        }
    }
    for (const auto& entry : tallies)
    {
        if (sweep.mnemonics.find(entry.first) == sweep.mnemonics.end())
        {
            reachable_only.push_back(entry.first);
        }
    }

    std::cout << "-- the gap: in the sweep, not reached from the entry ("
              << sweep_only.size() << ") --\n ";
    for (const std::string& mnemonic : sweep_only)
    {
        std::cout << ' ' << mnemonic;
    }
    std::cout << "\n\n";

    // Expected to be empty. A mnemonic reached from the entry that a linear
    // sweep of the same bytes never produced would mean the sweep walked past
    // its address misaligned, which is worth seeing rather than assuming away.
    std::cout << "-- reached from the entry, absent from the sweep ("
              << reachable_only.size() << ") --\n ";
    for (const std::string& mnemonic : reachable_only)
    {
        std::cout << ' ' << mnemonic;
    }
    std::cout << "\n\n";

    std::cout << "-- uncertainty in the reachable set --\n";
    std::cout << "  indirect exits      " << plan.indirect_exit_count << "\n";
    std::cout << "  jump tables         " << plan.jump_table_count << "\n";
    std::cout << "  outside-image targets "
              << plan.outside_image_target_count << "\n";
    std::cout << "  decode failures     " << plan.decode_failure_count << "\n";
    std::cout << "  analysis limits     " << plan.analysis_limit_count << "\n";
    std::cout << "  re-decode failures  " << redecode_failures << "\n\n";

    std::cout << "-- x87 --\n";
    std::cout << "  x87 instructions    " << x87.total << "\n";
    std::cout << "  80-bit memory ops   " << x87.float80_memory_operands << "\n";
    std::cout << "  control-word access " << x87.control_word_access << "\n";
    std::cout << "  env save/restore    " << x87.environment_save_restore
              << "\n";
    std::cout << "  distinct x87 mnemonics " << x87.mnemonics.size() << "\n";
    for (const auto& entry : x87.mnemonics)
    {
        std::cout << "    " << std::left << std::setw(10) << entry.first
                  << std::right << std::setw(8) << entry.second << "\n";
    }
    std::cout << "\n";

    // Level C. Ordered by static frequency, because that is the order an
    // interpreter can be built in: the head of this list is most of the program.
    std::vector<std::pair<std::string, MnemonicTally>> ordered(tallies.begin(),
                                                              tallies.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& left, const auto& right) {
                  if (left.second.instructions != right.second.instructions)
                  {
                      return left.second.instructions >
                             right.second.instructions;
                  }
                  return left.first < right.first;
              });

    std::cout << "-- level C: top 30 by static frequency --\n";
    std::cout << "  " << std::left << std::setw(12) << "mnemonic"
              << std::right << std::setw(10) << "count" << std::setw(8)
              << "forms" << std::setw(9) << "share" << std::setw(11)
              << "cumulative" << "\n";
    std::uint64_t cumulative = 0;
    std::size_t shown = 0;
    for (const auto& entry : ordered)
    {
        cumulative += entry.second.instructions;
        if (shown < 30)
        {
            const double share = counted == 0
                ? 0.0
                : 100.0 * static_cast<double>(entry.second.instructions) /
                      static_cast<double>(counted);
            const double cumulative_share = counted == 0
                ? 0.0
                : 100.0 * static_cast<double>(cumulative) /
                      static_cast<double>(counted);
            std::cout << "  " << std::left << std::setw(12) << entry.first
                      << std::right << std::setw(10)
                      << entry.second.instructions << std::setw(8)
                      << entry.second.forms.size() << std::setw(8)
                      << std::fixed << std::setprecision(2) << share << "%"
                      << std::setw(10) << cumulative_share << "%\n";
        }
        ++shown;
    }
    std::cout << std::defaultfloat << "\n";

    // Task 556. What an x64 host could emit from this guest today.
    const auto percent = [](std::uint64_t part, std::uint64_t whole) {
        return whole == 0 ? 0.0
                          : 100.0 * static_cast<double>(part) /
                                static_cast<double>(whole);
    };
    std::cout << "-- x64 long-mode emission (Task 553 rules) --\n";
    if (!long_mode_built)
    {
        std::cout << "  image did not build: " << long_mode_image.message
                  << "\n";
    }
    const std::uint64_t emitted =
        long_mode.copied + long_mode.lowered + long_mode.branches +
        long_mode.returns + long_mode.segment_overrides +
        long_mode.guarded_segment_loads;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  considered          " << long_mode.considered << "\n";
    std::cout << "  copied              " << long_mode.copied << "  ("
              << percent(long_mode.copied, long_mode.considered) << "%)\n";
    std::cout << "  lowered             " << long_mode.lowered << "  ("
              << percent(long_mode.lowered, long_mode.considered) << "%)\n";
    std::cout << "  branches            " << long_mode.branches << "  ("
              << percent(long_mode.branches, long_mode.considered) << "%)\n";
    std::cout << "  returns             " << long_mode.returns << "  ("
              << percent(long_mode.returns, long_mode.considered) << "%)\n";
    std::cout << "  segment overrides   " << long_mode.segment_overrides << "  ("
              << percent(long_mode.segment_overrides, long_mode.considered)
              << "%)\n";
    std::cout << "  guarded seg loads   "
              << long_mode.guarded_segment_loads << "  ("
              << percent(long_mode.guarded_segment_loads,
                         long_mode.considered) << "%)\n";
    std::cout << "  emittable           " << emitted << "  ("
              << percent(emitted, long_mode.considered) << "%)\n";
    std::cout << "  refused             " << long_mode.refused << "  ("
              << percent(long_mode.refused, long_mode.considered) << "%)\n";
    std::cout << "    of which not a kCopy record  "
              << long_mode.refused_non_copy << "\n";
    // The number that decides whether anything runs. A block that stops at an
    // INT3 has no way to the next one.
    std::cout << "  blocks              " << long_mode.blocks << "\n";
    std::cout << "  blocks complete     " << long_mode.blocks_complete << "  ("
              << percent(long_mode.blocks_complete, long_mode.blocks)
              << "%)\n";
    std::cout << std::defaultfloat;

    // Whether the explanation matches the authority. A mismatch means the
    // census copied the emitter's rule wrongly, or the emitter and the
    // classifier disagree -- either is a finding rather than a rounding note.
    const bool agrees = long_mode_built &&
        long_mode_image.long_mode_copied_count == long_mode.copied &&
        long_mode_image.long_mode_lowered_count == long_mode.lowered &&
        long_mode_image.long_mode_branch_count == long_mode.branches &&
        long_mode_image.long_mode_return_count == long_mode.returns &&
        long_mode_image.long_mode_segment_override_count ==
            long_mode.segment_overrides &&
        long_mode_image.long_mode_guarded_segment_load_count ==
            long_mode.guarded_segment_loads &&
        long_mode_image.long_mode_refused_count == long_mode.refused;
    std::cout << "  emitter counters    copied="
              << long_mode_image.long_mode_copied_count
              << " lowered=" << long_mode_image.long_mode_lowered_count
              << " branches=" << long_mode_image.long_mode_branch_count
              << " returns=" << long_mode_image.long_mode_return_count
              << " segments="
              << long_mode_image.long_mode_segment_override_count
              << " segloads="
              << long_mode_image.long_mode_guarded_segment_load_count
              << " refused=" << long_mode_image.long_mode_refused_count
              << "  agrees=" << (agrees ? "true" : "false") << "\n";
    // Task 560. The edges that had to become boundaries after all, because
    // their target was not in this image. Reported beside the branch count
    // rather than folded into it: an emitted branch whose edge did not resolve
    // is not a branch that runs.
    std::cout << "  branch edges        emitted="
              << long_mode_image.long_mode_branch_count << " unresolved="
              << long_mode_image.long_mode_unresolved_branch_count << "\n\n";

    std::cout << "-- why the rest cannot be emitted --\n";
    {
        std::vector<std::pair<std::string, std::uint64_t>> reasons(
            long_mode.refusal_reasons.begin(),
            long_mode.refusal_reasons.end());
        std::sort(reasons.begin(), reasons.end(),
                  [](const auto& left, const auto& right) {
                      return left.second > right.second;
                  });
        for (const auto& reason : reasons)
        {
            std::cout << "  " << std::left << std::setw(30) << reason.first
                      << std::right << std::setw(10) << reason.second << "\n";
        }
    }
    // Task 560. The largest refusal, split by what it actually is. `kCopy` is
    // the emitter's whole long-mode subset, so everything else arrives here as
    // one number -- and one number cannot say whether the work in front is a
    // branch encoding, a dispatch resolver, or a port I/O slot.
    // Task 563. Coverage says what is missing from the image; this says what is
    // in the way of a run. They are different questions and the second is the
    // one that decides whether anything executes.
    std::cout << "  reachable serviced  "
              << long_mode.reachable_serviced_blocks
              << "  (walked through a serviced boundary)\n";
    std::cout << "  reachable blocks    " << long_mode.reachable_blocks
              << "  (" << std::fixed << std::setprecision(2)
              << percent(long_mode.reachable_blocks, long_mode.blocks)
              << "% of blocks)\n"
              << "  reachable instrs    "
              << long_mode.reachable_instructions << "\n"
              << std::defaultfloat;

    std::cout << "\n-- where a chain from the entry stops --\n";
    {
        std::vector<std::pair<std::string, std::uint64_t>> frontier(
            long_mode.frontier_kinds.begin(), long_mode.frontier_kinds.end());
        std::sort(frontier.begin(), frontier.end(),
                  [](const auto& left, const auto& right) {
                      return left.second > right.second;
                  });
        for (const auto& entry : frontier)
        {
            std::cout << "  " << std::left << std::setw(30) << entry.first
                      << std::right << std::setw(10) << entry.second << "\n";
        }
        std::cout << "  " << std::left << std::setw(30)
                  << "edge outside the plan" << std::right << std::setw(10)
                  << long_mode.frontier_edges_outside_plan << "\n";
        std::cout << "  entry=0x" << std::hex << long_mode.walk_entry_address
                  << " first stop=0x" << long_mode.frontier_first_address
                  << std::dec << "\n";
        // The bytes as well. Task 565 spent a round reasoning about which
        // encoding was in the way; printing it costs nothing and settles it.
        std::cout << "  first stop bytes ";
        for (const std::uint8_t byte : long_mode.frontier_first_bytes)
        {
            std::cout << " " << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(byte);
        }
        std::cout << std::dec << std::setfill(' ') << "\n";
    }

    // Task 565's frontier is `mov ebx, es:[0x5c]`, and whether that can be
    // handled by simply dropping the prefix turns on one number: the segment's
    // base. Long mode ignores the `CS`/`DS`/`ES`/`SS` overrides and treats
    // their base as zero, so a guest whose bases are already zero loses nothing
    // by having the prefix removed -- and a guest whose bases are not zero
    // needs the i386 path's fold instead.
    //
    // Printed rather than reasoned about, for Task 565's reason: the last two
    // units each spent a round on a guess the bytes could have settled.
    std::cout << "\n-- selector bindings (segment bases) --\n";
    {
        std::size_t shown = 0;
        for (const repiu::runtime::RelocatedSelectorBinding& binding :
             image.selector_bindings)
        {
            if (shown++ >= 16U)
            {
                std::cout << "  ... "
                          << image.selector_bindings.size() - 16U
                          << " more\n";
                break;
            }
            std::cout << "  selector=0x" << std::hex << binding.selector
                      << " base=0x" << binding.relocated_base_address
                      << " limit=0x" << binding.limit << std::dec
                      << " object=" << binding.target_object << "\n";
        }
        if (image.selector_bindings.empty())
        {
            std::cout << "  (none recorded)\n";
        }
    }

    std::cout << "\n-- not-a-copy-record, by plan kind --\n";
    {
        std::vector<std::pair<std::string, std::uint64_t>> kinds(
            long_mode.refused_kinds.begin(), long_mode.refused_kinds.end());
        std::sort(kinds.begin(), kinds.end(),
                  [](const auto& left, const auto& right) {
                      return left.second > right.second;
                  });
        for (const auto& kind : kinds)
        {
            std::cout << "  " << std::left << std::setw(30) << kind.first
                      << std::right << std::setw(10) << kind.second << "  ("
                      << std::fixed << std::setprecision(2)
                      << percent(kind.second, long_mode.refused_non_copy)
                      << "% of non-copy)\n"
                      << std::defaultfloat;
        }
    }

    std::cout << "\n-- refused mnemonics and reasons, by volume (top 30) --\n";
    {
        std::vector<std::pair<std::string, std::uint64_t>> refused(
            long_mode.refused_mnemonics.begin(),
            long_mode.refused_mnemonics.end());
        std::sort(refused.begin(), refused.end(),
                  [](const auto& left, const auto& right) {
                      return left.second > right.second;
                  });
        std::size_t shown = 0;
        for (const auto& entry : refused)
        {
            // Thirty rather than twenty since Task 557 split each row by
            // reason: the same cut showed fewer instructions than it used to,
            // and the item this task was measuring fell just below it.
            if (shown++ >= 30)
            {
                break;
            }
            std::cout << "  " << std::left << std::setw(40) << entry.first
                      << std::right << std::setw(10) << entry.second << "\n";
        }
    }
    std::cout << "\n";

    // The machine-readable pair. Two lines rather than one: Task 512 recorded
    // that a single long line reads worse for both people and scripts.
    std::cout << "[repiu-census] target=" << profile->id
              << " blocks=" << plan.block_count
              << " instructions=" << plan.instruction_count
              << " counted=" << counted
              << " mnemonics=" << tallies.size()
              << " forms=" << forms.size()
              << " sweep_mnemonics=" << sweep.mnemonics.size()
              << " sweep_decoded=" << sweep.decoded
              << " sweep_failed=" << sweep.failed
              << " sweep_only=" << sweep_only.size()
              << " reachable_only=" << reachable_only.size() << "\n";
    std::cout << "[repiu-census-x64] considered=" << long_mode.considered
              << " copied=" << long_mode.copied
              << " lowered=" << long_mode.lowered
              << " refused=" << long_mode.refused
              << " refused_non_copy=" << long_mode.refused_non_copy
              << " blocks=" << long_mode.blocks
              << " blocks_complete=" << long_mode.blocks_complete
              << " image_built=" << (long_mode_built ? 1 : 0)
              << " agrees=" << (agrees ? 1 : 0) << "\n";
    std::cout << "[repiu-census-x87] total=" << x87.total
              << " float80_mem=" << x87.float80_memory_operands
              << " control_word=" << x87.control_word_access
              << " env_save_restore=" << x87.environment_save_restore
              << " mnemonics=" << x87.mnemonics.size()
              << " indirect_exits=" << plan.indirect_exit_count
              << " jump_tables=" << plan.jump_table_count
              << " decode_failures=" << plan.decode_failure_count
              << " redecode_failures=" << redecode_failures << "\n";
    return 0;
}
