#include "long_mode_emission_probe.h"

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::runtime::AotBasicBlock;
using repiu::runtime::AotCodeCacheBuildOptions;
using repiu::runtime::AotCodeCacheImage;
using repiu::runtime::AotFixupKind;
using repiu::runtime::AotInstructionKind;
using repiu::runtime::AotInstructionRecord;
using repiu::runtime::AotTranslationPlan;
using repiu::runtime::BuildAotCodeCacheImage;

// Task 553. What the emitter does with one plan, under both settings.
//
// The emitter produces bytes and executes nothing, so this probe runs on every
// host and its answers have to agree across them -- the same reason Task 550
// kept the classifier probe outside the x64 fence. A Windows run and a Linux
// x64 run disagreeing here would mean the emitter's judgement had drifted from
// the classifier's.
//
// The plan below is one block holding one instruction of each outcome, in the
// order the design's flowchart names them.

constexpr std::uint32_t kBase = 0x00120000U;

// xor eax, eax. Register-only work at 32 bits: the subset the classifier is
// willing to call identical.
const std::vector<std::uint8_t> kCopyable = {0x31U, 0xC0U};

// mov eax, [ebx+4]. A memory operand through a base register, which long mode
// would compute in RBX without the prefix.
const std::vector<std::uint8_t> kBaseRelative = {0x8BU, 0x43U, 0x04U};
const std::vector<std::uint8_t> kBaseRelativeLowered = {0x67U, 0x8BU, 0x43U,
                                                        0x04U};

// mov eax, [0x12345678]. ModRM mod=00 rm=101, which long mode reads as
// RIP-relative -- the divergence a prefix alone does not fix.
const std::vector<std::uint8_t> kAbsolute = {0x8BU, 0x05U, 0x78U, 0x56U,
                                             0x34U, 0x12U};
// 0x67, then the same opcode, then ModRM rm=100 with SIB base=101 index=100,
// then the displacement unchanged.
const std::vector<std::uint8_t> kAbsoluteLowered = {
    0x67U, 0x8BU, 0x04U, 0x25U, 0x78U, 0x56U, 0x34U, 0x12U};

// mov eax, [0x12345678] in the moffs form. Long mode reads the offset as eight
// bytes, so the instruction's own length changes and every decode after it
// moves. Nothing raises.
//
// This was `40` (`inc eax`) until Task 557 gave that a re-encoding. The item is
// the same one either way -- an encoding that must never be copied -- and it
// moved because the classifier learned to lower the old example, which is the
// direction this is supposed to go.
const std::vector<std::uint8_t> kSilentlyDifferent = {0xA1U, 0x78U, 0x56U,
                                                      0x34U, 0x12U};

// inc eax, and its lowering. Task 557: the register moves out of the opcode and
// into a ModRM byte, because in long mode `40` is a REX prefix.
const std::vector<std::uint8_t> kIncEax = {0x40U};
const std::vector<std::uint8_t> kIncEaxLowered = {0xFFU, 0xC0U};

// add esp, 16. Task 555. In long mode this writes `ESP`, zero-extending into
// the host's `RSP` -- it destroys the stack pointer the host returns on. It
// carries no memory operand, so it reached `kIdenticalBytes` and would have
// been copied verbatim into the cache.
const std::vector<std::uint8_t> kStackPointerWrite = {0x83U, 0xC4U, 0x10U};

// Port I/O: `in eax, dx`. It stands for the kinds long mode still has no slot
// for, and it closes a block without a fallthrough edge because its kind is not
// `kCopy`.
//
// A return did both jobs until Task 562 gave returns a slot. A probe that went
// on asserting a boundary there would have been asserting the past -- which is
// what it did, and what turned this item red the moment returns started
// working.
const std::vector<std::uint8_t> kPortIo = {0xEDU};

struct PlannedInstruction
{
    std::uint32_t guest_address = 0U;
    AotInstructionKind kind = AotInstructionKind::kCopy;
    const std::vector<std::uint8_t>* bytes = nullptr;
};

std::vector<PlannedInstruction> PlannedInstructions()
{
    std::vector<PlannedInstruction> planned;
    std::uint32_t address = kBase;
    const auto add = [&](const AotInstructionKind kind,
                         const std::vector<std::uint8_t>& bytes) {
        planned.push_back({address, kind, &bytes});
        address += static_cast<std::uint32_t>(bytes.size());
    };
    add(AotInstructionKind::kCopy, kCopyable);
    add(AotInstructionKind::kCopy, kBaseRelative);
    add(AotInstructionKind::kCopy, kAbsolute);
    add(AotInstructionKind::kCopy, kSilentlyDifferent);
    add(AotInstructionKind::kCopy, kIncEax);
    add(AotInstructionKind::kCopy, kStackPointerWrite);
    add(AotInstructionKind::kPortIo, kPortIo);
    return planned;
}

AotTranslationPlan MakePlan()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kBase;
    AotBasicBlock block;
    block.guest_address = kBase;
    for (const PlannedInstruction& planned : PlannedInstructions())
    {
        AotInstructionRecord record;
        record.guest_address = planned.guest_address;
        record.kind = planned.kind;
        record.length = static_cast<std::uint8_t>(planned.bytes->size());
        record.bytes = *planned.bytes;
        block.instructions.push_back(record);
    }
    plan.blocks.push_back(block);
    return plan;
}

// The emitted bytes for one guest address, read back through the address map
// rather than by counting offsets, which is how the emitter itself names them.
bool EmittedBytes(const AotCodeCacheImage& image,
                  const std::uint32_t guest_address,
                  std::vector<std::uint8_t>* out)
{
    for (const repiu::runtime::AotAddressMapEntry& map : image.address_map)
    {
        if (map.guest_address != guest_address)
        {
            continue;
        }
        const std::size_t end =
            static_cast<std::size_t>(map.cache_offset) + map.emitted_length;
        if (end > image.bytes.size())
        {
            return false;
        }
        out->assign(image.bytes.begin() + map.cache_offset,
                    image.bytes.begin() + end);
        return true;
    }
    return false;
}

bool Expect(const char* name, const AotCodeCacheImage& image,
            const std::uint32_t guest_address,
            const std::vector<std::uint8_t>& expected)
{
    std::vector<std::uint8_t> emitted;
    const bool ok = EmittedBytes(image, guest_address, &emitted) &&
        emitted == expected;
    std::cout << "  " << name << "=" << (ok ? "true" : "false") << "\n";
    return ok;
}

bool HasBoundaryFixupAt(const AotCodeCacheImage& image,
                        const std::uint32_t guest_address)
{
    for (const repiu::runtime::AotCodeCacheFixup& fixup : image.fixups)
    {
        if (fixup.kind == AotFixupKind::kHleBoundary &&
            fixup.guest_source == guest_address)
        {
            return true;
        }
    }
    return false;
}

// A. With the option off, nothing about the emitter has changed.
//
// This is the item the whole unit rests on: until now the classifier and the
// lowering stood apart from the emitter, and "no i386 behaviour changed" was
// true because nothing called them. Wiring them ends that, so the property has
// to become something checked rather than something remembered.
bool ProbeDefaultIsUnchanged()
{
    AotCodeCacheImage image;
    bool ok = BuildAotCodeCacheImage(MakePlan(), &image) && image.valid;
    std::cout << "long_mode_emission_default_builds="
              << (ok ? "true" : "false") << "\n";
    for (const PlannedInstruction& planned : PlannedInstructions())
    {
        if (planned.kind != AotInstructionKind::kCopy)
        {
            continue;
        }
        std::vector<std::uint8_t> emitted;
        const bool verbatim =
            EmittedBytes(image, planned.guest_address, &emitted) &&
            emitted == *planned.bytes;
        ok = ok && verbatim;
        if (!verbatim)
        {
            std::cout << "  long_mode_emission_default_verbatim_at_"
                      << std::hex << planned.guest_address << std::dec
                      << "=false\n";
        }
    }
    const bool quiet = !image.long_mode_emission_enabled &&
        image.long_mode_copied_count == 0U &&
        image.long_mode_lowered_count == 0U &&
        image.long_mode_refused_count == 0U;
    std::cout << "long_mode_emission_default_unchanged="
              << (ok ? "true" : "false") << ",counters_quiet="
              << (quiet ? "true" : "false") << "\n";
    return ok && quiet;
}

// B. With the option on, each outcome produces the bytes the design names.
bool ProbeLongModeOutcomes()
{
    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const std::vector<PlannedInstruction> planned = PlannedInstructions();
    // Decision 3's evidence. Without the verification decode changing mode with
    // the option, the lowered bytes below are measured with a 32-bit decoder,
    // where `0x67` means a 16-bit address size -- and correct bytes report as
    // decode failures. This line is where that would show.
    const bool built = BuildAotCodeCacheImage(MakePlan(), options, &image) &&
        image.valid;
    std::cout << "long_mode_emission_image_valid=" << (built ? "true" : "false")
              << ",decode_failures=" << image.decode_failure_count << "\n";

    const bool copied = Expect("long_mode_emission_copied", image,
                               planned[0].guest_address, kCopyable);
    const bool prefixed = Expect("long_mode_emission_address_size_prefix",
                                 image, planned[1].guest_address,
                                 kBaseRelativeLowered);
    const bool sib = Expect("long_mode_emission_absolute_to_sib", image,
                            planned[2].guest_address, kAbsoluteLowered);
    // The central refusal. `40` copied verbatim is a program that runs and is
    // wrong, so this item is written as "one INT3, and a boundary fixup that
    // names this guest address".
    const bool refused = Expect("long_mode_emission_refused_silent", image,
                                planned[3].guest_address, {0xCCU}) &&
        HasBoundaryFixupAt(image, planned[3].guest_address);
    // Task 557. The INC that used to be refused outright.
    const bool inc_dec = Expect("long_mode_emission_inc_to_modrm", image,
                                planned[4].guest_address, kIncEaxLowered);
    // Task 555. The stack pointer, which had been passing as identical bytes.
    const bool stack = Expect("long_mode_emission_refused_stack_pointer", image,
                              planned[5].guest_address, {0xCCU}) &&
        HasBoundaryFixupAt(image, planned[5].guest_address);
    // The kinds long mode still has no slot for, standing in for the
    // hand-built 32-bit slots that must not reach a long-mode image.
    //
    // It used to say "everything that is not `kCopy`", which stopped being the
    // rule when Tasks 560 to 562 gave jumps, branches, calls and returns their
    // own long-mode slots. What must reach a boundary is now what the emitter
    // has not built, and port I/O is one of those.
    const bool non_copy = Expect("long_mode_emission_non_copy_boundary", image,
                                 planned[6].guest_address, {0xCCU}) &&
        HasBoundaryFixupAt(image, planned[6].guest_address);

    const bool counted = image.long_mode_emission_enabled &&
        image.long_mode_copied_count == 1U &&
        image.long_mode_lowered_count == 3U &&
        image.long_mode_refused_count == 3U;
    std::cout << "long_mode_emission_counts=" << (counted ? "true" : "false")
              << ",copied=" << image.long_mode_copied_count
              << ",lowered=" << image.long_mode_lowered_count
              << ",refused=" << image.long_mode_refused_count << "\n";
    return built && copied && prefixed && sib && refused && inc_dec && stack &&
        non_copy && counted;
}

// C. A plan the emitter can produce nothing for still builds.
//
// Fail-closed means a boundary at every instruction, not a failed build: the
// runtime resumes the guest from those INT3s, so an image of nothing but
// boundaries is a working image that happens to be slow. A build that failed
// instead would take the whole cache down over one unsupported byte.
bool ProbeAllRefusedStillBuilds()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kBase;
    AotBasicBlock block;
    block.guest_address = kBase;
    AotInstructionRecord record;
    record.guest_address = kBase;
    record.kind = AotInstructionKind::kCopy;
    record.length = 1U;
    record.bytes = kSilentlyDifferent;
    block.instructions.push_back(record);
    record.guest_address = kBase + 1U;
    block.instructions.push_back(record);
    // A non-`kCopy` tail closes the block. A block whose tail is `kCopy` also
    // gets a fallthrough edge, and its target would have to be a block of its
    // own -- a property of the planner rather than of this unit, and not what
    // this item is asking about.
    //
    // Port I/O rather than a return, since Task 562: this item counts three
    // refusals, and a return is emitted now.
    record.guest_address = kBase + 2U;
    record.kind = AotInstructionKind::kPortIo;
    record.bytes = kPortIo;
    block.instructions.push_back(record);
    plan.blocks.push_back(block);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool ok = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid && image.long_mode_refused_count == 3U &&
        image.long_mode_copied_count == 0U &&
        image.long_mode_lowered_count == 0U;
    std::cout << "long_mode_emission_all_refused_builds="
              << (ok ? "true" : "false") << "\n";
    return ok;
}

}  // namespace

bool RunLongModeEmissionProbe()
{
    const bool default_ok = ProbeDefaultIsUnchanged();
    const bool outcomes_ok = ProbeLongModeOutcomes();
    const bool refused_ok = ProbeAllRefusedStillBuilds();

    const bool all = default_ok && outcomes_ok && refused_ok;
    std::cout << "long_mode_emission_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
