#include "long_mode_emission_probe.h"

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
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

// `LES`, which long mode reads as a three-byte VEX prefix. Nothing raises; the
// bytes simply become some other instruction.
//
// This example has now moved twice. It was `40` (`inc eax`) until Task 557 gave
// that a re-encoding, then the moffs `A1` until Task 565 gave *that* one. The
// item being checked is the same throughout -- an encoding that must never be
// copied -- and each move happened because the classifier learned to lower the
// previous example, which is the direction this is supposed to go.
//
// It is worth noticing that the example keeps having to move. A probe pinned to
// a specific encoding measures that encoding; what this item is for is the
// category, so it will move again.
const std::vector<std::uint8_t> kSilentlyDifferent = {0xC4U, 0x04U, 0x24U};

// inc eax, and its lowering. Task 557: the register moves out of the opcode and
// into a ModRM byte, because in long mode `40` is a REX prefix.
const std::vector<std::uint8_t> kIncEax = {0x40U};
const std::vector<std::uint8_t> kIncEaxLowered = {0xFFU, 0xC0U};

// add esp, 16. Task 555. In long mode this writes `ESP`, zero-extending into
// the host's `RSP` -- it destroys the stack pointer the host returns on. It
// carries no memory operand, so it reached `kIdenticalBytes` and would have
// been copied verbatim into the cache.
const std::vector<std::uint8_t> kStackPointerWrite = {0x83U, 0xC4U, 0x10U};
// Task 564: `add r15d, 16`. REX.B before the opcode, and ModRM `rm` from `100`
// (ESP) to `111` (R15). Same opcode, same immediate, one byte longer.
const std::vector<std::uint8_t> kStackPointerWriteLowered = {0x41U, 0x83U,
                                                             0xC7U, 0x10U};

// Port I/O: `in eax, dx`. It stands for the kinds long mode still has no slot
// for, and it closes a block without a fallthrough edge because its kind is not
// `kCopy`.
//
// A return did both jobs until Task 562 gave returns a slot. A probe that went
// on asserting a boundary there would have been asserting the past -- which is
// what it did, and what turned this item red the moment returns started
// working.
const std::vector<std::uint8_t> kPortIo = {0xEDU};

// Object 3's `66 CB`: a far return whose correct width depends on the guest
// descriptor mode. It must not enter the near-return resolver slot until that
// descriptor-aware ABI exists.
const std::vector<std::uint8_t> kFarReturn = {0x66U, 0xCBU};

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
    add(AotInstructionKind::kFarReturn, kFarReturn);
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
    // Task 555 found this passing as identical bytes; Task 564 re-encodes it.
    //
    // `add esp, 16` becomes `add r15d, 16`: a REX.B inserted before the opcode
    // and the ModRM `rm` moved from `100` (ESP) to `111` (R15). Guest ESP lives
    // in R15D, so this is the same arithmetic on the register that actually
    // holds it -- and the host's RSP, which the original would have written, is
    // left alone.
    const bool stack = Expect("long_mode_emission_stack_pointer_to_r15", image,
                              planned[5].guest_address,
                              kStackPointerWriteLowered);
    // The kinds long mode still has no slot for, standing in for the
    // hand-built 32-bit slots that must not reach a long-mode image.
    //
    // It used to say "everything that is not `kCopy`", which stopped being the
    // rule when Tasks 560 to 562 gave jumps, branches, calls and returns their
    // own long-mode slots. What must reach a boundary is now what the emitter
    // has not built, and port I/O is one of those.
    const bool non_copy = Expect("long_mode_emission_non_copy_boundary", image,
                                 planned[7].guest_address, {0xCCU}) &&
        HasBoundaryFixupAt(image, planned[7].guest_address);
    const bool far_return = Expect("long_mode_emission_far_return_boundary",
                                   image, planned[6].guest_address, {0xCCU}) &&
        HasBoundaryFixupAt(image, planned[6].guest_address);

    const bool counted = image.long_mode_emission_enabled &&
        image.long_mode_copied_count == 1U &&
        // Task 564 moved the stack-pointer write from refused to lowered.
        image.long_mode_lowered_count == 4U &&
        image.long_mode_refused_count == 3U;
    std::cout << "long_mode_emission_counts=" << (counted ? "true" : "false")
              << ",copied=" << image.long_mode_copied_count
              << ",lowered=" << image.long_mode_lowered_count
              << ",refused=" << image.long_mode_refused_count << "\n";
    return built && copied && prefixed && sib && refused && inc_dec && stack &&
        far_return && non_copy && counted;
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

// Task 610. The guest uses the operand-size-prefixed form `MOV BX,DS` while
// entering LINEXE. The planner must classify its GPR16 destination so a
// long-mode cache cannot copy the instruction and read the host DS register.
bool ProbeSegmentReadGpr16Classification()
{
    runtime::RelocatedRuntimeImage read_runtime;
    read_runtime.valid = true;
    read_runtime.relocated_image_base = 0x00126000U;
    read_runtime.relocated_entry_linear_address = 0x00126000U;
    runtime::RelocatedRuntimeObject read_object;
    read_object.relocated_base_address = 0x00126000U;
    read_object.memory = {0x66U, 0x8CU, 0xDBU, 0xC3U};
    read_object.memory.resize(32U, 0x90U);
    read_object.virtual_size =
        static_cast<std::uint32_t>(read_object.memory.size());
    read_runtime.objects.push_back(std::move(read_object));

    runtime::AotTranslationPlan plan;
    const bool plan_built = runtime::BuildAotTranslationPlanFromEntry(
        read_runtime, read_runtime.relocated_entry_linear_address, &plan);
    const bool classified = plan_built && !plan.blocks.empty() &&
        !plan.blocks[0].instructions.empty() &&
        plan.blocks[0].instructions[0].kind ==
            runtime::AotInstructionKind::kGuardedSegmentRead &&
        plan.blocks[0].instructions[0].segment_register == 3U &&
        plan.blocks[0].instructions[0].gpr_register == 3U;

    runtime::AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    runtime::AotCodeCacheImage image;
    const bool image_built = classified &&
        runtime::BuildAotCodeCacheImage(plan, options, &image) && image.valid;
    const bool fail_closed = image_built && !image.address_map.empty() &&
        image.bytes[image.address_map[0].cache_offset] == 0xCCU &&
        HasBoundaryFixupAt(image, read_runtime.relocated_entry_linear_address);
    const bool ok = classified && fail_closed;
    std::cout << "long_mode_segment_read_gpr16_ds_classified="
              << (classified ? "true" : "false")
              << ",fail_closed=" << (fail_closed ? "true" : "false")
              << "\n";
    return ok;
}

// Task 592. The pop guard has a dedicated long-mode ABI: it saves guest flags,
// compares the saved guest-stack selector, and restores flags on both exits.
// It must validate as a slot rather than being mistaken for the i386 layout or
// accepted as arbitrary non-INT3 bytes.
bool ProbeLongModeSegmentGuardCoverage()
{
    runtime::RelocatedRuntimeImage pop_runtime;
    pop_runtime.valid = true;
    pop_runtime.relocated_image_base = 0x00124000U;
    pop_runtime.relocated_entry_linear_address = 0x00124000U;
    runtime::RelocatedRuntimeObject pop_object;
    pop_object.relocated_base_address = 0x00124000U;
    pop_object.memory = {0x07U, 0xC3U};  // pop es; ret
    pop_object.memory.resize(16U, 0x90U);
    pop_object.virtual_size =
        static_cast<std::uint32_t>(pop_object.memory.size());
    pop_runtime.objects.push_back(std::move(pop_object));

    AotTranslationPlan pop_plan;
    AotCodeCacheImage pop_image;
    AotCodeCacheBuildOptions pop_options;
    pop_options.enable_long_mode_emission = true;
    pop_options.enable_guarded_segment_pop = true;
    const bool pop_plan_built = runtime::BuildAotTranslationPlanFromEntry(
        pop_runtime, pop_runtime.relocated_entry_linear_address, &pop_plan);
    const bool pop_image_built = pop_plan_built &&
        runtime::BuildAotCodeCacheImage(pop_plan, pop_options, &pop_image);
    const bool pop_site_ready = pop_image_built &&
        pop_image.guarded_segment_pop_sites.size() == 1U;
    const bool pop_coverage = pop_site_ready &&
        runtime::ValidateAotCodeCacheHleCoverage(pop_plan, pop_image);
    const bool pop_ready = pop_coverage;
    bool pop_corruption_rejected = false;
    if (pop_ready)
    {
        const runtime::AotGuardedSegmentPopSite& site =
            pop_image.guarded_segment_pop_sites[0];
        runtime::AotCodeCacheImage broken_pop = pop_image;
        broken_pop.bytes[site.fallback_offset] = 0x90U;
        std::uint32_t failure_guest = 0U;
        pop_corruption_rejected =
            !runtime::ValidateAotCodeCacheHleCoverage(
                pop_plan, broken_pop, &failure_guest) &&
            failure_guest == pop_runtime.relocated_entry_linear_address;
    }

    const bool all = pop_ready && pop_corruption_rejected;
    runtime::RelocatedRuntimeImage load_runtime;
    load_runtime.valid = true;
    load_runtime.relocated_image_base = 0x00125000U;
    load_runtime.relocated_entry_linear_address = 0x00125000U;
    runtime::RelocatedRuntimeObject load_object;
    load_object.relocated_base_address = 0x00125000U;
    load_object.memory = {0x8EU, 0xC0U, 0x90U, 0xC3U};  // mov es,ax; nop; ret
    load_object.memory.resize(32U, 0x90U);
    load_object.virtual_size =
        static_cast<std::uint32_t>(load_object.memory.size());
    load_runtime.objects.push_back(std::move(load_object));

    AotTranslationPlan load_plan;
    AotCodeCacheImage load_image;
    AotCodeCacheBuildOptions load_options;
    load_options.enable_long_mode_emission = true;
    load_options.enable_guarded_segment_load = true;
    const bool load_plan_built = runtime::BuildAotTranslationPlanFromEntry(
        load_runtime, load_runtime.relocated_entry_linear_address, &load_plan);
    const bool load_image_built = load_plan_built &&
        runtime::BuildAotCodeCacheImage(load_plan, load_options, &load_image);
    const bool load_site_ready = load_image_built &&
        load_image.guarded_segment_load_sites.size() == 1U;
    const bool load_coverage = load_site_ready &&
        runtime::ValidateAotCodeCacheHleCoverage(load_plan, load_image);
    bool load_corruption_rejected = false;
    if (load_coverage)
    {
        const runtime::AotGuardedSegmentLoadSite& site =
            load_image.guarded_segment_load_sites[0];
        runtime::AotCodeCacheImage broken_load = load_image;
        broken_load.bytes[site.fallback_offset] = 0x90U;
        std::uint32_t failure_guest = 0U;
        load_corruption_rejected =
            !runtime::ValidateAotCodeCacheHleCoverage(
                load_plan, broken_load, &failure_guest) &&
            failure_guest == load_runtime.relocated_entry_linear_address;
    }

    const bool all_with_load = all && load_coverage && load_corruption_rejected;
    std::cout << "long_mode_segment_guard_coverage="
              << (all_with_load ? "true" : "false")
              << ",pop_plan=" << (pop_plan_built ? "true" : "false")
              << ",pop_image=" << (pop_image_built ? "true" : "false")
              << ",pop_site=" << (pop_site_ready ? "true" : "false")
              << ",pop_coverage=" << (pop_coverage ? "true" : "false")
              << ",pop_corruption_rejected="
              << (pop_corruption_rejected ? "true" : "false")
              << ",load_plan=" << (load_plan_built ? "true" : "false")
              << ",load_image=" << (load_image_built ? "true" : "false")
              << ",load_image_message=" << load_image.message
              << ",load_decode_failures=" << load_image.decode_failure_count
              << ",load_site=" << (load_site_ready ? "true" : "false")
              << ",load_coverage=" << (load_coverage ? "true" : "false")
              << ",load_corruption_rejected="
              << (load_corruption_rejected ? "true" : "false") << "\n";
    return all_with_load;
}

}  // namespace

bool RunLongModeEmissionProbe()
{
    const bool default_ok = ProbeDefaultIsUnchanged();
    const bool outcomes_ok = ProbeLongModeOutcomes();
    const bool refused_ok = ProbeAllRefusedStillBuilds();
    const bool segment_read_gpr16_ok = ProbeSegmentReadGpr16Classification();
    const bool segment_guard_coverage_ok = ProbeLongModeSegmentGuardCoverage();

    const bool all = default_ok && outcomes_ok && refused_ok &&
        segment_read_gpr16_ok &&
        segment_guard_coverage_ok;
    std::cout << "long_mode_emission_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
