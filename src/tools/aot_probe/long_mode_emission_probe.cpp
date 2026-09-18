#include "long_mode_emission_probe.h"

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <Zydis.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
using repiu::runtime::GuestCodeDefaultOperandSize;

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

// Task 657. The validator must use the same absolute-form predicate as the
// emitter: mod=00, rm=5 is absolute, while rm=7 preserves EDI as the base.
bool ProbeLongModeSegmentOverrideCoverage()
{
    constexpr std::uint32_t kSegmentBase = 0x00127000U;
    AotInstructionRecord segment_record;
    segment_record.guest_address = kSegmentBase;
    segment_record.kind = AotInstructionKind::kSegmentOverrideMem;
    segment_record.length = 4U;
    segment_record.segment_override_register = 2U;
    segment_record.fallthrough_target = kSegmentBase + 4U;
    segment_record.bytes = {0x66U, 0x36U, 0x89U, 0x07U};

    AotInstructionRecord return_record;
    return_record.guest_address = kSegmentBase + 4U;
    return_record.kind = AotInstructionKind::kReturn;
    return_record.length = 1U;
    return_record.bytes = {0xC3U};

    AotBasicBlock block;
    block.guest_address = kSegmentBase;
    block.instructions.push_back(segment_record);
    block.instructions.push_back(return_record);

    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kSegmentBase;
    plan.hle_boundary_count = 1U;
    plan.return_count = 1U;
    plan.instruction_count = 2U;
    plan.source_code_bytes = 5U;
    plan.blocks.push_back(std::move(block));

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;

    const runtime::AotSegmentOverrideSite* site = nullptr;
    if (built && image.segment_override_sites.size() == 1U)
    {
        site = &image.segment_override_sites[0];
    }
    const runtime::AotAddressMapEntry* map = nullptr;
    if (built)
    {
        for (const runtime::AotAddressMapEntry& candidate :
             image.address_map)
        {
            if (candidate.guest_address == segment_record.guest_address)
            {
                map = &candidate;
                break;
            }
        }
    }

    bool access_layout = false;
    bool coverage = false;
    bool corruption_rejected = false;
    if (site != nullptr && map != nullptr &&
        site->displacement_offset >= site->cache_offset + 4U &&
        site->displacement_offset + 4U <= image.bytes.size())
    {
        const std::uint32_t access_offset = site->displacement_offset - 4U;
        access_layout = map->cache_offset == site->cache_offset &&
            image.bytes[access_offset] == 0x66U &&
            image.bytes[access_offset + 1U] == 0x67U &&
            image.bytes[access_offset + 2U] == 0x89U &&
            image.bytes[access_offset + 3U] == 0x87U;
        coverage = runtime::ValidateAotCodeCacheHleCoverage(plan, image);
        if (coverage)
        {
            AotCodeCacheImage broken = image;
            broken.bytes[access_offset + 3U] = 0x86U;
            std::uint32_t failure_guest = 0U;
            corruption_rejected =
                !runtime::ValidateAotCodeCacheHleCoverage(
                    plan, broken, &failure_guest) &&
                failure_guest == segment_record.guest_address;
        }
    }

    const bool ok = built && site != nullptr && map != nullptr &&
        access_layout && coverage && corruption_rejected;
    std::cout << "long_mode_segment_override_base_coverage="
              << (ok ? "true" : "false")
              << ",built=" << (built ? "true" : "false")
              << ",site=" << (site != nullptr ? "true" : "false")
              << ",map=" << (map != nullptr ? "true" : "false")
              << ",layout=" << (access_layout ? "true" : "false")
              << ",coverage=" << (coverage ? "true" : "false")
              << ",corruption_rejected="
              << (corruption_rejected ? "true" : "false") << "\n";
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

// Task 630. A conditional branch's not-taken edge.
//
// The emitter used to append a fallthrough branch only for a `kCopy` tail and
// let a conditional tail rely on its fallthrough being the bytes that
// physically follow. That held for as long as block order matched guest order,
// and a dynamic append whose entry is the fallthrough breaks it: the
// fallthrough is emitted first, the branch's block lands later, and the bytes
// after the branch belong to whatever came next in the image.
//
// Two plans over the same three instructions, differing only in block order, so
// what is measured is the edge rather than the encoding.
struct ConditionalFallthroughPlan
{
    AotTranslationPlan plan;
    std::uint32_t branch_address = 0U;
    std::uint32_t fallthrough_address = 0U;
};

// `mov eax, [ebx+4]`, `jnz`, then a port I/O tail. Port I/O closes its block
// without an edge of its own, so any `kBlockFallthrough` in these images is the
// one being measured.
ConditionalFallthroughPlan MakeConditionalFallthroughPlan(
    const bool fallthrough_first)
{
    ConditionalFallthroughPlan built;
    const std::vector<std::uint8_t> branch_bytes = {0x75U, 0x02U};

    AotInstructionRecord head;
    head.guest_address = kBase;
    head.kind = AotInstructionKind::kCopy;
    head.length = static_cast<std::uint8_t>(kBaseRelative.size());
    head.bytes = kBaseRelative;

    AotInstructionRecord branch;
    branch.guest_address = kBase + head.length;
    branch.kind = AotInstructionKind::kConditionalBranch;
    branch.length = static_cast<std::uint8_t>(branch_bytes.size());
    branch.bytes = branch_bytes;
    branch.mnemonic = static_cast<std::uint16_t>(ZYDIS_MNEMONIC_JNZ);
    branch.direct_target = kBase;

    AotInstructionRecord tail;
    tail.guest_address = branch.guest_address + branch.length;
    tail.kind = AotInstructionKind::kPortIo;
    tail.length = static_cast<std::uint8_t>(kPortIo.size());
    tail.bytes = kPortIo;

    AotBasicBlock branch_block;
    branch_block.guest_address = head.guest_address;
    branch_block.instructions.push_back(head);
    branch_block.instructions.push_back(branch);

    AotBasicBlock fallthrough_block;
    fallthrough_block.guest_address = tail.guest_address;
    fallthrough_block.instructions.push_back(tail);

    built.plan.valid = true;
    built.branch_address = branch.guest_address;
    built.fallthrough_address = tail.guest_address;
    if (fallthrough_first)
    {
        built.plan.entry_address = fallthrough_block.guest_address;
        built.plan.blocks.push_back(fallthrough_block);
        built.plan.blocks.push_back(branch_block);
    }
    else
    {
        built.plan.entry_address = branch_block.guest_address;
        built.plan.blocks.push_back(branch_block);
        built.plan.blocks.push_back(fallthrough_block);
    }
    return built;
}

bool CacheOffsetOf(const AotCodeCacheImage& image,
                   const std::uint32_t guest_address,
                   std::uint32_t* offset)
{
    for (const repiu::runtime::AotAddressMapEntry& map : image.address_map)
    {
        if (map.guest_address == guest_address)
        {
            *offset = map.cache_offset;
            return true;
        }
    }
    return false;
}

bool ProbeConditionalBranchFallthrough()
{
    // The fallthrough is the next block, so adjacency already carries the edge
    // and nothing is added. This is the item that says the fix costs the common
    // case nothing.
    const ConditionalFallthroughPlan adjacent =
        MakeConditionalFallthroughPlan(false);
    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage adjacent_image;
    bool adjacent_ok =
        BuildAotCodeCacheImage(adjacent.plan, options, &adjacent_image) &&
        adjacent_image.valid;
    for (const repiu::runtime::AotCodeCacheFixup& fixup :
         adjacent_image.fixups)
    {
        if (fixup.kind == AotFixupKind::kBlockFallthrough)
        {
            adjacent_ok = false;
        }
    }

    // The same three instructions with the fallthrough emitted first. Adjacency
    // no longer carries the edge, so it has to be written and resolved back to
    // the earlier offset.
    const ConditionalFallthroughPlan reordered =
        MakeConditionalFallthroughPlan(true);
    AotCodeCacheImage reordered_image;
    bool reordered_ok =
        BuildAotCodeCacheImage(reordered.plan, options, &reordered_image) &&
        reordered_image.valid;
    std::uint32_t fallthrough_offset = 0U;
    reordered_ok = reordered_ok &&
        CacheOffsetOf(reordered_image, reordered.fallthrough_address,
                      &fallthrough_offset);
    bool edge_resolved = false;
    for (const repiu::runtime::AotCodeCacheFixup& fixup :
         reordered_image.fixups)
    {
        if (fixup.kind != AotFixupKind::kBlockFallthrough ||
            fixup.guest_source != reordered.branch_address ||
            fixup.guest_target != reordered.fallthrough_address ||
            !fixup.resolved || fixup.cache_patch_offset == 0U ||
            fixup.cache_patch_offset + 4U > reordered_image.bytes.size())
        {
            continue;
        }
        std::int32_t displacement = 0;
        std::memcpy(&displacement,
                    reordered_image.bytes.data() + fixup.cache_patch_offset,
                    sizeof(displacement));
        const std::int64_t landing =
            static_cast<std::int64_t>(fixup.cache_patch_offset) + 4 +
            displacement;
        edge_resolved =
            reordered_image.bytes[fixup.cache_patch_offset - 1U] == 0xE9U &&
            landing == static_cast<std::int64_t>(fallthrough_offset);
    }
    reordered_ok = reordered_ok && edge_resolved;

    const bool ok = adjacent_ok && reordered_ok;
    std::cout << "long_mode_emission_conditional_fallthrough_adjacent="
              << (adjacent_ok ? "true" : "false")
              << ",reordered=" << (reordered_ok ? "true" : "false") << "\n";
    return ok;
}

// Task 710. Timer safe points under long mode.
//
// The emitter planted a safe point on every backward edge for i386 and on none
// at all for long mode, because the long-mode branch continued past the switch
// that planted them. A guest spinning in long-mode AOT code could therefore not
// take a timer request, and pumpit2a's zero-length PIU.BIN retry -- which Win32
// leaves after 37 reads -- ran 769,639 times in ten seconds on Linux x64.
//
// Three claims, each on the same plan so only the host mode varies: long mode
// plants the same number of safe points as i386; its compare uses the SIB form,
// because `83 3D disp32` is RIP-relative there; and the image still passes the
// emitter's own decode verification, which counts each entry's instructions.
bool ProbeTimerSafePointsInLongMode()
{
    const ConditionalFallthroughPlan built =
        MakeConditionalFallthroughPlan(false);

    // The i386 image of this plan is not a valid cache on its own -- its
    // branch target resolves outside the image, and that is so with safe points
    // off too, because the plan was written for the long-mode path (Task 630).
    // What is compared from it is what the emitter planted and wrote, which is
    // decided before resolution and is what every Win32 run executes.
    AotCodeCacheBuildOptions i386_options;
    i386_options.enable_timer_safe_points = true;
    AotCodeCacheImage i386_image;
    BuildAotCodeCacheImage(built.plan, i386_options, &i386_image);

    // The long-mode image has to be valid outright: its verifier decodes every
    // entry and compares the instruction count with the emitter's own, so a
    // safe point counted wrong fails here.
    AotCodeCacheBuildOptions long_options;
    long_options.enable_long_mode_emission = true;
    long_options.enable_timer_safe_points = true;
    AotCodeCacheImage long_image;
    const bool long_built =
        BuildAotCodeCacheImage(built.plan, long_options, &long_image) &&
        long_image.valid;

    const bool planted = long_built &&
        !i386_image.timer_safe_point_sites.empty() &&
        long_image.timer_safe_point_sites.size() ==
            i386_image.timer_safe_point_sites.size();

    bool long_encoding = planted;
    for (const auto& site : long_image.timer_safe_point_sites)
    {
        const std::vector<std::uint8_t>& bytes = long_image.bytes;
        long_encoding = long_encoding &&
            site.request_address_offset >= 3U &&
            site.request_address_offset + 5U <= bytes.size() &&
            bytes[site.cache_offset] == 0x9CU &&
            bytes[site.request_address_offset - 3U] == 0x83U &&
            bytes[site.request_address_offset - 2U] == 0x3CU &&
            bytes[site.request_address_offset - 1U] == 0x25U &&
            bytes[site.request_address_offset + 4U] == 0x00U &&
            bytes[site.breakpoint_offset] == 0xCCU;
    }
    // The i386 bytes are the ones every Win32 run has executed; they must not
    // have moved.
    bool i386_encoding = planted;
    for (const auto& site : i386_image.timer_safe_point_sites)
    {
        const std::vector<std::uint8_t>& bytes = i386_image.bytes;
        i386_encoding = i386_encoding &&
            site.request_address_offset >= 2U &&
            bytes[site.cache_offset] == 0x9CU &&
            bytes[site.request_address_offset - 2U] == 0x83U &&
            bytes[site.request_address_offset - 1U] == 0x3DU &&
            bytes[site.breakpoint_offset] == 0xCCU;
    }

    // Disabled stays disabled: the long-mode branch plants nothing unless asked.
    AotCodeCacheBuildOptions off_options;
    off_options.enable_long_mode_emission = true;
    AotCodeCacheImage off_image;
    const bool off_ok =
        BuildAotCodeCacheImage(built.plan, off_options, &off_image) &&
        off_image.valid && off_image.timer_safe_point_sites.empty();

    const bool all = planted && long_encoding && i386_encoding && off_ok;
    std::cout << "long_mode_emission_timer_safe_points=planted="
              << (planted ? "true" : "false")
              << ",long_valid=" << (long_built ? "true" : "false")
              << ",i386_sites=" << i386_image.timer_safe_point_sites.size()
              << ",long_sites=" << long_image.timer_safe_point_sites.size()
              << ",long_encoding=" << (long_encoding ? "true" : "false")
              << ",i386_encoding=" << (i386_encoding ? "true" : "false")
              << ",disabled=" << (off_ok ? "true" : "false") << "\n";
    return all;
}

bool ProbeUnresolvedBlockFallthroughLookup()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kBase;
    AotBasicBlock block;
    block.guest_address = kBase;
    AotInstructionRecord instruction;
    instruction.guest_address = kBase;
    instruction.kind = AotInstructionKind::kCopy;
    instruction.length = 1U;
    instruction.bytes = {0x90U};
    block.instructions.push_back(instruction);
    plan.blocks.push_back(block);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid && image.fixups.size() == 1U;
    if (!built)
    {
        std::cout << "long_mode_fallthrough_lookup=false\n";
        return false;
    }

    const repiu::runtime::AotCodeCacheFixup& fixup = image.fixups[0];
    constexpr std::uint32_t cache_base = 0x20000000U;
    const std::uint32_t cache_size =
        static_cast<std::uint32_t>(image.bytes.size());
    std::vector<repiu::runtime::AotCodeCacheFixup> fixups = image.fixups;
    const std::uint32_t sentinel =
        cache_base + fixup.cache_patch_offset - 1U;
    std::uint32_t target = 0U;
    const bool exact =
        fixup.kind == AotFixupKind::kBlockFallthrough && !fixup.resolved &&
        image.bytes[fixup.cache_patch_offset - 1U] == 0xCCU &&
        repiu::runtime::FindAotBlockFallthroughTarget(
            fixups, cache_base, cache_size, sentinel, &target) &&
        target == kBase + 1U;
    const bool adjacent_rejected =
        !repiu::runtime::FindAotBlockFallthroughTarget(
            fixups, cache_base, cache_size, sentinel + 1U, &target);

    fixups[0].resolved = true;
    const bool resolved_rejected =
        !repiu::runtime::FindAotBlockFallthroughTarget(
            fixups, cache_base, cache_size, sentinel, &target);
    fixups[0].resolved = false;
    fixups[0].kind = AotFixupKind::kDirectJump;
    const bool other_kind_rejected =
        !repiu::runtime::FindAotBlockFallthroughTarget(
            fixups, cache_base, cache_size, sentinel, &target);

    const bool ok = exact && adjacent_rejected && resolved_rejected &&
        other_kind_rejected;
    std::cout << "long_mode_fallthrough_lookup="
              << (ok ? "true" : "false") << "\n";
    return ok;
}

bool ProbeIndirectFallbackStackCleanup()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kBase;

    const auto append_block = [&](const std::uint32_t address,
                                  const std::uint8_t modrm) {
        AotBasicBlock block;
        block.guest_address = address;
        AotInstructionRecord instruction;
        instruction.guest_address = address;
        instruction.kind = AotInstructionKind::kIndirectExit;
        instruction.length = 2U;
        instruction.bytes = {0xFFU, modrm};
        block.instructions.push_back(std::move(instruction));
        plan.blocks.push_back(std::move(block));
    };
    append_block(kBase, 0xD0U);          // call eax
    append_block(kBase + 0x20U, 0xE0U);  // jmp eax

    AotCodeCacheBuildOptions options;
    options.enable_dbt_indirect_miss_dispatch = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid && image.dbt_indirect_dispatch_sites.size() == 2U;
    bool call_ok = false;
    bool jump_ok = false;
    if (built)
    {
        for (const repiu::runtime::AotDbtIndirectDispatchSite& site :
             image.dbt_indirect_dispatch_sites)
        {
            if (site.fallback_cache_offset + 5U > image.bytes.size())
            {
                continue;
            }
            const std::uint8_t* const fallback =
                image.bytes.data() + site.fallback_cache_offset;
            const bool shape = fallback[0] == 0x8DU &&
                fallback[1] == 0x64U && fallback[2] == 0x24U &&
                fallback[4] == 0xCCU;
            if (site.is_call)
            {
                call_ok = shape && fallback[3] == 0x04U;
            }
            else
            {
                jump_ok = shape && fallback[3] == 0x08U;
            }
        }
    }
    const bool ok = built && call_ok && jump_ok;
    std::cout << "indirect_fallback_call_return_preserved="
              << (call_ok ? "true" : "false")
              << ",jump_metadata_removed="
              << (jump_ok ? "true" : "false") << "\n";
    return ok;
}

// Task 680. A 16-bit copy record may use a dedicated lowering, while a
// 16-bit control-flow record must not enter a 32-bit long-mode slot.
bool Probe16BitModeEmission()
{
    constexpr std::uint32_t base = kBase + 0x100U;

    // First exercise the same object-mode metadata path used by the real
    // planner. The extra bytes keep the planner's bounded decode window
    // readable while the first record must still stop at the word immediate.
    repiu::runtime::RelocatedRuntimeImage guest_image;
    guest_image.valid = true;
    repiu::runtime::RelocatedRuntimeObject guest_object;
    guest_object.object_index = 3U;
    guest_object.relocated_base_address = base;
    guest_object.virtual_size = 15U;
    guest_object.flags = repiu::runtime::kLeObjectExecutable;
    guest_object.memory.assign(15U, 0x90U);
    guest_object.memory[0] = 0xBCU;
    guest_object.memory[1] = 0x00U;
    guest_object.memory[2] = 0x20U;
    guest_image.objects.push_back(std::move(guest_object));
    guest_image.code_mode_ranges.push_back({
        base, 15U, repiu::runtime::kLeObjectExecutable});
    AotTranslationPlan planned_from_object;
    const bool plan_built =
        repiu::runtime::BuildAotTranslationPlanFromEntry(
            guest_image, base, &planned_from_object);
    const AotInstructionRecord* first_planned = nullptr;
    if (plan_built)
    {
        for (const AotBasicBlock& planned_block : planned_from_object.blocks)
        {
            if (!planned_block.instructions.empty())
            {
                first_planned = &planned_block.instructions.front();
                break;
            }
        }
    }
    const bool planner_mode = first_planned != nullptr &&
        first_planned->guest_address == base &&
        first_planned->length == 3U &&
        first_planned->bytes == std::vector<std::uint8_t>{
            0xBCU, 0x00U, 0x20U} &&
        first_planned->guest_code_default_operand_size ==
            GuestCodeDefaultOperandSize::k16;

    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;
    AotBasicBlock block;
    block.guest_address = base;

    AotInstructionRecord stack_pointer;
    stack_pointer.guest_address = base;
    stack_pointer.kind = AotInstructionKind::kCopy;
    stack_pointer.length = 3U;
    stack_pointer.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    stack_pointer.bytes = {0xBCU, 0x00U, 0x20U};
    block.instructions.push_back(stack_pointer);

    AotInstructionRecord branch;
    branch.guest_address = base + 3U;
    branch.kind = AotInstructionKind::kDirectJump;
    branch.length = 2U;
    branch.direct_target = base;
    branch.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    branch.bytes = {0xEBU, 0xFBU};
    block.instructions.push_back(branch);
    plan.blocks.push_back(block);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    std::vector<std::uint8_t> emitted;
    const bool stack_bytes = built && EmittedBytes(image, base, &emitted) &&
        emitted == std::vector<std::uint8_t>{
            0x66U, 0x41U, 0xBFU, 0x00U, 0x20U};
    const bool branch_boundary = built &&
        EmittedBytes(image, base + 3U, &emitted) &&
        emitted == std::vector<std::uint8_t>{0xCCU} &&
        HasBoundaryFixupAt(image, base + 3U);
    const bool ok = planner_mode && stack_bytes && branch_boundary;
    std::cout << "long_mode_emission_16bit_mode="
              << (ok ? "true" : "false") << ",planner_mode="
              << (planner_mode ? "true" : "false") << ",refused="
              << image.long_mode_refused_count << "\n";
    return ok;
}

// Task 681. A mode16 67+66 LEA uses the same cache path as object 3. The
// operand-size prefix is removed, the address-size prefix remains, and ESP is
// remapped to R15. A following non-copy mode16 record stays a boundary.
bool Probe16BitLeaModeEmission()
{
    constexpr std::uint32_t base = kBase + 0x200U;
    const std::vector<std::uint8_t> lea_bytes = {
        0x67U, 0x66U, 0x8DU, 0x8CU, 0x24U,
        0x00U, 0xE0U, 0xFFU, 0xFFU,
    };
    const std::vector<std::uint8_t> expected = {
        0x67U, 0x41U, 0x8DU, 0x8CU, 0x27U,
        0x00U, 0xE0U, 0xFFU, 0xFFU,
    };

    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;
    AotBasicBlock block;
    block.guest_address = base;
    AotInstructionRecord lea;
    lea.guest_address = base;
    lea.kind = AotInstructionKind::kCopy;
    lea.length = static_cast<std::uint8_t>(lea_bytes.size());
    lea.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    lea.bytes = lea_bytes;
    block.instructions.push_back(lea);

    AotInstructionRecord boundary;
    boundary.guest_address = base + static_cast<std::uint32_t>(lea_bytes.size());
    boundary.kind = AotInstructionKind::kPortIo;
    boundary.length = 1U;
    boundary.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    boundary.bytes = {0xEDU};
    block.instructions.push_back(boundary);
    plan.blocks.push_back(block);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    std::vector<std::uint8_t> emitted;
    const bool lea_ok = built && EmittedBytes(image, base, &emitted) &&
        emitted == expected;
    const bool boundary_ok = built &&
        EmittedBytes(image, boundary.guest_address, &emitted) &&
        emitted == std::vector<std::uint8_t>{0xCCU} &&
        HasBoundaryFixupAt(image, boundary.guest_address);
    const bool ok = lea_ok && boundary_ok;
    std::cout << "long_mode_emission_16bit_lea32="
              << (ok ? "true" : "false") << ",lowered="
              << (lea_ok ? 1 : 0) << ",boundary="
              << (boundary_ok ? 1 : 0) << "\n";
    return ok;
}

// Task 682. The actual object-3 frontier is a prefix-free mode16 LEA with a
// 16-bit address calculation. Its multi-instruction scratch lowering must be
// decoded as one cache entry, and a following mode16 non-copy record remains a
// separate boundary.
bool Probe16BitLea16ModeEmission()
{
    constexpr std::uint32_t base = kBase + 0x300U;
    const std::vector<std::uint8_t> lea_bytes = {
        0x8DU, 0x8CU, 0x24U, 0x00U,
    };
    const std::vector<std::uint8_t> expected = {
        0x44U, 0x0FU, 0xB7U, 0xF6U,
        0x67U, 0x66U, 0x41U, 0x8DU, 0x8EU,
        0x24U, 0x00U, 0x00U, 0x00U,
    };

    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;
    AotBasicBlock block;
    block.guest_address = base;
    AotInstructionRecord lea;
    lea.guest_address = base;
    lea.kind = AotInstructionKind::kCopy;
    lea.length = static_cast<std::uint8_t>(lea_bytes.size());
    lea.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    lea.bytes = lea_bytes;
    block.instructions.push_back(lea);

    AotInstructionRecord boundary;
    boundary.guest_address = base + static_cast<std::uint32_t>(lea_bytes.size());
    boundary.kind = AotInstructionKind::kPortIo;
    boundary.length = 1U;
    boundary.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    boundary.bytes = {0xEDU};
    block.instructions.push_back(boundary);
    plan.blocks.push_back(block);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    std::vector<std::uint8_t> emitted;
    const bool lea_ok = built && EmittedBytes(image, base, &emitted) &&
        emitted == expected;
    const bool boundary_ok = built &&
        EmittedBytes(image, boundary.guest_address, &emitted) &&
        emitted == std::vector<std::uint8_t>{0xCCU} &&
        HasBoundaryFixupAt(image, boundary.guest_address);
    const bool ok = lea_ok && boundary_ok;
    std::cout << "long_mode_emission_16bit_lea16="
              << (ok ? "true" : "false") << ",lowered="
              << (lea_ok ? 1 : 0) << ",boundary="
              << (boundary_ok ? 1 : 0) << "\n";
    return ok;
}

// Task 683. A mode16 LOOPNZ gets a dedicated long-mode slot. Its taken edge
// is an ordinary conditional fixup, while the not-taken path exits the slot so
// the enclosing block can provide an explicit block-fallthrough edge.
bool Probe16BitLoopNzModeEmission()
{
    constexpr std::uint32_t base = kBase + 0x400U;
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;

    AotBasicBlock loop_block;
    loop_block.guest_address = base;
    AotInstructionRecord loop;
    loop.guest_address = base;
    loop.kind = AotInstructionKind::kConditionalBranch;
    loop.length = 2U;
    loop.direct_target = base + 0x20U;
    loop.fallthrough_target = base + 2U;
    loop.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    loop.bytes = {0xE0U, 0x01U};
    loop_block.instructions.push_back(loop);
    plan.blocks.push_back(std::move(loop_block));

    // Emit the target first so the loop's fallthrough cannot be satisfied by
    // adjacency. This makes the two edge contracts visible independently.
    AotBasicBlock target_block;
    target_block.guest_address = base + 0x20U;
    AotInstructionRecord target_boundary;
    target_boundary.guest_address = base + 0x20U;
    target_boundary.kind = AotInstructionKind::kPortIo;
    target_boundary.length = 1U;
    target_boundary.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    target_boundary.bytes = {0xEDU};
    target_block.instructions.push_back(target_boundary);
    plan.blocks.push_back(std::move(target_block));

    AotBasicBlock fallthrough_block;
    fallthrough_block.guest_address = base + 2U;
    AotInstructionRecord fallthrough_boundary;
    fallthrough_boundary.guest_address = base + 2U;
    fallthrough_boundary.kind = AotInstructionKind::kPortIo;
    fallthrough_boundary.length = 1U;
    fallthrough_boundary.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    fallthrough_boundary.bytes = {0xEDU};
    fallthrough_block.instructions.push_back(fallthrough_boundary);
    plan.blocks.push_back(std::move(fallthrough_block));

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    const std::vector<std::uint8_t> expected = {
        0x9CU,
        0x66U, 0xFFU, 0xC9U,
        0x66U, 0x85U, 0xC9U,
        0x74U, 0x11U,
        0x4CU, 0x8BU, 0x34U, 0x24U,
        0x49U, 0x0FU, 0xBAU, 0xE6U, 0x06U,
        0x72U, 0x06U,
        0x9DU,
        0xE9U, 0x06U, 0x00U, 0x00U, 0x00U,
        0x9DU,
    };
    std::vector<std::uint8_t> emitted;
    const bool slot_bytes = built && EmittedBytes(image, base, &emitted) &&
        emitted == expected;
    bool conditional_fixup = false;
    bool block_fallthrough_fixup = false;
    for (const repiu::runtime::AotCodeCacheFixup& fixup : image.fixups)
    {
        conditional_fixup = conditional_fixup ||
            (fixup.kind == AotFixupKind::kConditionalBranch &&
             fixup.guest_source == base &&
             fixup.guest_target == base + 0x20U && fixup.resolved &&
             fixup.cache_patch_offset == 22U);
        block_fallthrough_fixup = block_fallthrough_fixup ||
            (fixup.kind == AotFixupKind::kBlockFallthrough &&
             fixup.guest_source == base &&
             fixup.guest_target == base + 2U && fixup.resolved &&
             fixup.cache_patch_offset == 28U);
    }
    const bool ok = slot_bytes && conditional_fixup &&
        block_fallthrough_fixup;
    std::cout << "long_mode_emission_16bit_loopnz="
              << (ok ? "true" : "false") << ",slot="
              << (slot_bytes ? 1 : 0) << ",conditional_fixup="
              << (conditional_fixup ? 1 : 0) << ",fallthrough_fixup="
              << (block_fallthrough_fixup ? 1 : 0) << "\n";
    return ok;
}

// An unresolved taken target must neutralise only the complete loop entry;
// its separate block-fallthrough edge must not patch bytes back into that
// entry.
bool Probe16BitLoopNzUnresolvedTarget()
{
    constexpr std::uint32_t base = kBase + 0x500U;
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;
    AotBasicBlock block;
    block.guest_address = base;
    AotInstructionRecord loop;
    loop.guest_address = base;
    loop.kind = AotInstructionKind::kConditionalBranch;
    loop.length = 2U;
    loop.direct_target = base + 0x20U;
    loop.fallthrough_target = base + 2U;
    loop.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    loop.bytes = {0xE0U, 0x01U};
    block.instructions.push_back(loop);
    plan.blocks.push_back(std::move(block));

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    std::vector<std::uint8_t> emitted;
    const bool neutralised = built && EmittedBytes(image, base, &emitted) &&
        emitted.size() == 27U &&
        std::all_of(emitted.begin(), emitted.end(),
                    [](const std::uint8_t byte) { return byte == 0xCCU; });
    const bool fallthrough_tail_neutralised = built && image.bytes.size() >=
        28U && image.bytes[27U] == 0xCCU;
    const bool ok = neutralised && fallthrough_tail_neutralised &&
        image.long_mode_unresolved_branch_count == 2U;
    std::cout << "long_mode_emission_16bit_loopnz_unresolved="
              << (ok ? "true" : "false") << ",entry="
              << (neutralised ? 1 : 0) << ",fallthrough="
              << (fallthrough_tail_neutralised ? 1 : 0) << "\n";
    return ok;
}

// Task 686. A mode16 short Jcc uses the shared long-mode direct-branch slot;
// the planner has already supplied the rebased target and fallthrough edges.
bool Probe16BitConditionalBranchModeEmission()
{
    constexpr std::uint32_t base = kBase + 0x600U;
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;

    AotBasicBlock branch_block;
    branch_block.guest_address = base;
    AotInstructionRecord branch;
    branch.guest_address = base;
    branch.kind = AotInstructionKind::kConditionalBranch;
    branch.length = 2U;
    branch.mnemonic = static_cast<std::uint16_t>(ZYDIS_MNEMONIC_JZ);
    branch.direct_target = base + 0x20U;
    branch.fallthrough_target = base + 2U;
    branch.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    branch.bytes = {0x74U, 0x01U};
    branch_block.instructions.push_back(branch);
    plan.blocks.push_back(std::move(branch_block));

    AotBasicBlock target_block;
    target_block.guest_address = base + 0x20U;
    AotInstructionRecord target;
    target.guest_address = base + 0x20U;
    target.kind = AotInstructionKind::kPortIo;
    target.length = 1U;
    target.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    target.bytes = {0xEDU};
    target_block.instructions.push_back(target);
    plan.blocks.push_back(std::move(target_block));

    AotBasicBlock fallthrough_block;
    fallthrough_block.guest_address = base + 2U;
    AotInstructionRecord fallthrough;
    fallthrough.guest_address = base + 2U;
    fallthrough.kind = AotInstructionKind::kPortIo;
    fallthrough.length = 1U;
    fallthrough.guest_code_default_operand_size =
        GuestCodeDefaultOperandSize::k16;
    fallthrough.bytes = {0xEDU};
    fallthrough_block.instructions.push_back(fallthrough);
    plan.blocks.push_back(std::move(fallthrough_block));

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    std::vector<std::uint8_t> emitted;
    const bool slot_bytes = built && EmittedBytes(image, base, &emitted) &&
        emitted.size() == 6U && emitted[0] == 0x0FU &&
        emitted[1] == 0x84U;
    bool conditional_fixup = false;
    bool fallthrough_fixup = false;
    for (const repiu::runtime::AotCodeCacheFixup& fixup : image.fixups)
    {
        conditional_fixup = conditional_fixup ||
            (fixup.kind == AotFixupKind::kConditionalBranch &&
             fixup.guest_source == base &&
             fixup.guest_target == base + 0x20U && fixup.resolved &&
             fixup.cache_patch_offset == 2U);
        fallthrough_fixup = fallthrough_fixup ||
            (fixup.kind == AotFixupKind::kBlockFallthrough &&
             fixup.guest_source == base && fixup.guest_target == base + 2U &&
             fixup.resolved && fixup.cache_patch_offset == 7U);
    }
    const bool ok = slot_bytes && conditional_fixup && fallthrough_fixup;
    std::cout << "long_mode_emission_16bit_jcc="
              << (ok ? "true" : "false") << ",slot="
              << (slot_bytes ? 1 : 0) << ",conditional_fixup="
              << (conditional_fixup ? 1 : 0) << ",fallthrough_fixup="
              << (fallthrough_fixup ? 1 : 0) << "\n";
    return ok;
}

bool Probe16BitConditionalBranchUnresolvedTarget()
{
    constexpr std::uint32_t base = kBase + 0x700U;
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = base;
    AotBasicBlock block;
    block.guest_address = base;
    AotInstructionRecord branch;
    branch.guest_address = base;
    branch.kind = AotInstructionKind::kConditionalBranch;
    branch.length = 2U;
    branch.mnemonic = static_cast<std::uint16_t>(ZYDIS_MNEMONIC_JZ);
    branch.direct_target = base + 0x20U;
    branch.fallthrough_target = base + 2U;
    branch.guest_code_default_operand_size = GuestCodeDefaultOperandSize::k16;
    branch.bytes = {0x74U, 0x01U};
    block.instructions.push_back(branch);
    plan.blocks.push_back(std::move(block));

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built = BuildAotCodeCacheImage(plan, options, &image) &&
        image.valid;
    std::vector<std::uint8_t> emitted;
    const bool entry_neutralised = built && EmittedBytes(image, base, &emitted) &&
        emitted.size() == 6U &&
        std::all_of(emitted.begin(), emitted.end(),
                    [](const std::uint8_t byte) { return byte == 0xCCU; });
    const bool fallthrough_neutralised = built && image.bytes.size() >= 7U &&
        image.bytes[6U] == 0xCCU;
    const bool ok = entry_neutralised && fallthrough_neutralised &&
        image.long_mode_unresolved_branch_count == 2U;
    std::cout << "long_mode_emission_16bit_jcc_unresolved="
              << (ok ? "true" : "false") << ",entry="
              << (entry_neutralised ? 1 : 0) << ",fallthrough="
              << (fallthrough_neutralised ? 1 : 0) << "\n";
    return ok;
}

}  // namespace

// Task 716. The planner makes a `CS:` data access an HLE boundary on every
// host, and x64 has nothing that services one; pumpit2a's timer-driven `itoa`
// died on its first `mov al, cs:[edx+table]`. Long mode emits exactly that
// case as a copy and nothing else that shares the boundary kind: the other
// three records here are boundaries for a reason besides their prefix.
//
// Each record is its own block, as the planner leaves a boundary. The admitted
// one has to carry its own jump to the next guest instruction, so the check
// follows that jump to where it lands rather than looking at its opcode alone.
bool ProbeCsDataBoundaryEmission()
{
    constexpr std::uint32_t kCsBase = 0x00130000U;
    const std::vector<std::uint8_t> itoa_load = {
        0x2EU, 0x8AU, 0x82U, 0x58U, 0x74U, 0x0EU, 0x00U};
    const std::vector<std::uint8_t> itoa_lowered = {
        0x67U, 0x2EU, 0x8AU, 0x82U, 0x58U, 0x74U, 0x0EU, 0x00U};
    // Another segment keeps its boundary.
    const std::vector<std::uint8_t> es_load = {0x26U, 0x8AU, 0x02U};
    // Port I/O through a CS-overridden source is still port I/O.
    const std::vector<std::uint8_t> cs_outsb = {0x2EU, 0x6EU};
    // 16-bit code has a real CS base.
    const std::vector<std::uint8_t> cs_sixteen = {0x2EU, 0x8AU, 0x07U};

    struct Entry
    {
        const std::vector<std::uint8_t>* bytes;
        GuestCodeDefaultOperandSize mode;
        std::uint32_t address;
    };
    std::vector<Entry> entries = {
        {&itoa_load, GuestCodeDefaultOperandSize::k32, 0U},
        {&es_load, GuestCodeDefaultOperandSize::k32, 0U},
        {&cs_outsb, GuestCodeDefaultOperandSize::k32, 0U},
        {&cs_sixteen, GuestCodeDefaultOperandSize::k16, 0U},
    };
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kCsBase;
    std::uint32_t address = kCsBase;
    for (Entry& entry : entries)
    {
        entry.address = address;
        AotBasicBlock block;
        block.guest_address = address;
        AotInstructionRecord record;
        record.guest_address = address;
        record.kind = AotInstructionKind::kHleBoundary;
        record.length = static_cast<std::uint8_t>(entry.bytes->size());
        record.bytes = *entry.bytes;
        record.guest_code_default_operand_size = entry.mode;
        block.instructions.push_back(record);
        plan.blocks.push_back(block);
        ++plan.hle_boundary_count;
        address += static_cast<std::uint32_t>(entry.bytes->size());
    }

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    const bool built =
        BuildAotCodeCacheImage(plan, options, &image) && image.valid;

    std::vector<std::uint8_t> emitted;
    bool admitted = built &&
        EmittedBytes(image, entries[0].address, &emitted) &&
        emitted.size() == itoa_lowered.size() + 5U &&
        std::equal(itoa_lowered.begin(), itoa_lowered.end(),
                   emitted.begin()) &&
        emitted[itoa_lowered.size()] == 0xE9U &&
        !HasBoundaryFixupAt(image, entries[0].address);
    std::uint32_t next_offset = 0U;
    bool lands = admitted &&
        CacheOffsetOf(image, entries[1].address, &next_offset);
    bool jump_found = false;
    for (const repiu::runtime::AotCodeCacheFixup& fixup : image.fixups)
    {
        if (fixup.kind != AotFixupKind::kBlockFallthrough ||
            fixup.guest_source != entries[0].address)
        {
            continue;
        }
        std::int32_t displacement = 0;
        std::memcpy(&displacement,
                    image.bytes.data() + fixup.cache_patch_offset,
                    sizeof(displacement));
        jump_found = fixup.resolved &&
            fixup.guest_target == entries[1].address &&
            static_cast<std::int64_t>(fixup.cache_patch_offset) + 4 +
                    displacement ==
                static_cast<std::int64_t>(next_offset);
    }
    lands = lands && jump_found;

    bool kept = built;
    for (std::size_t index = 1U; index < entries.size(); ++index)
    {
        std::vector<std::uint8_t> boundary;
        kept = kept && EmittedBytes(image, entries[index].address, &boundary) &&
            boundary == std::vector<std::uint8_t>{0xCCU} &&
            HasBoundaryFixupAt(image, entries[index].address);
    }
    const bool counted = image.long_mode_cs_data_boundary_count == 1U;
    // The engine refuses the whole image when this fails, and the first cut of
    // the change failed it: the emitted bytes were right and the game stopped
    // one second in, with no AOT code at all.
    const bool covered =
        built && repiu::runtime::ValidateAotCodeCacheHleCoverage(plan, image);

    const bool ok = admitted && lands && kept && counted && covered;
    std::cout << "long_mode_emission_cs_data_boundary="
              << (ok ? "true" : "false")
              << ",admitted=" << (admitted ? "true" : "false")
              << ",lands=" << (lands ? "true" : "false")
              << ",kept=" << (kept ? "true" : "false")
              << ",covered=" << (covered ? "true" : "false")
              << ",count=" << image.long_mode_cs_data_boundary_count << "\n";
    return ok;
}

bool RunLongModeEmissionProbe()
{
    const bool default_ok = ProbeDefaultIsUnchanged();
    const bool outcomes_ok = ProbeLongModeOutcomes();
    const bool refused_ok = ProbeAllRefusedStillBuilds();
    const bool sixteen_bit_mode_ok = Probe16BitModeEmission();
    const bool sixteen_bit_lea_ok = Probe16BitLeaModeEmission();
    const bool sixteen_bit_lea16_ok = Probe16BitLea16ModeEmission();
    const bool sixteen_bit_loopnz_ok = Probe16BitLoopNzModeEmission();
    const bool sixteen_bit_loopnz_unresolved_ok =
        Probe16BitLoopNzUnresolvedTarget();
    const bool sixteen_bit_jcc_ok = Probe16BitConditionalBranchModeEmission();
    const bool sixteen_bit_jcc_unresolved_ok =
        Probe16BitConditionalBranchUnresolvedTarget();
    const bool segment_read_gpr16_ok = ProbeSegmentReadGpr16Classification();
    const bool segment_override_coverage_ok =
        ProbeLongModeSegmentOverrideCoverage();
    const bool segment_guard_coverage_ok = ProbeLongModeSegmentGuardCoverage();
    const bool conditional_fallthrough_ok =
        ProbeConditionalBranchFallthrough();
    const bool unresolved_fallthrough_ok =
        ProbeUnresolvedBlockFallthroughLookup();
    const bool indirect_fallback_stack_ok =
        ProbeIndirectFallbackStackCleanup();
    const bool timer_safe_points_ok = ProbeTimerSafePointsInLongMode();
    const bool cs_data_boundary_ok = ProbeCsDataBoundaryEmission();

    const bool all = default_ok && outcomes_ok && refused_ok &&
        sixteen_bit_mode_ok &&
        sixteen_bit_lea_ok &&
        sixteen_bit_lea16_ok &&
        sixteen_bit_loopnz_ok &&
        sixteen_bit_loopnz_unresolved_ok &&
        sixteen_bit_jcc_ok &&
        sixteen_bit_jcc_unresolved_ok &&
        segment_read_gpr16_ok &&
        segment_override_coverage_ok &&
        segment_guard_coverage_ok &&
        conditional_fallthrough_ok &&
        unresolved_fallthrough_ok &&
        indirect_fallback_stack_ok &&
        timer_safe_points_ok &&
        cs_data_boundary_ok;
    std::cout << "long_mode_emission_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
