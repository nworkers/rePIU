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
    std::uint64_t refused = 0;
    // Refused because the plan record is not `kCopy` at all -- every control
    // flow record, every guarded segment slot, every port I/O record. Counted
    // apart from the classifier's refusals because no lowering of an
    // instruction's bytes would change it; it needs an x64 slot instead.
    std::uint64_t refused_non_copy = 0;
    std::map<std::string, std::uint64_t> refusal_reasons;
    // The mnemonics behind the refusals, by volume, so the next unit is chosen
    // from data rather than from an impression of what is common.
    std::map<std::string, std::uint64_t> refused_mnemonics;
    std::uint64_t blocks = 0;
    std::uint64_t blocks_complete = 0;
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
                if (record.kind !=
                    repiu::runtime::AotInstructionKind::kCopy)
                {
                    ++long_mode.refused;
                    ++long_mode.refused_non_copy;
                    ++long_mode.refusal_reasons["not-a-copy-record"];
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
            }
        }
    }

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
    const std::uint64_t emitted = long_mode.copied + long_mode.lowered;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  considered          " << long_mode.considered << "\n";
    std::cout << "  copied              " << long_mode.copied << "  ("
              << percent(long_mode.copied, long_mode.considered) << "%)\n";
    std::cout << "  lowered             " << long_mode.lowered << "  ("
              << percent(long_mode.lowered, long_mode.considered) << "%)\n";
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
        long_mode_image.long_mode_refused_count == long_mode.refused;
    std::cout << "  emitter counters    copied="
              << long_mode_image.long_mode_copied_count
              << " lowered=" << long_mode_image.long_mode_lowered_count
              << " refused=" << long_mode_image.long_mode_refused_count
              << "  agrees=" << (agrees ? "true" : "false") << "\n\n";

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
