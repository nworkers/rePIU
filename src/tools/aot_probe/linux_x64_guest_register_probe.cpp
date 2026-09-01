#include "linux_x64_guest_register_probe.h"
#include "repiu/runtime/aot_segment_patch.h"

#include "repiu/platform/fault_handler.h"
#include "repiu/platform/linux_x64_aot_dispatch.h"
#include "repiu/platform/linux_x64_guest_registers.h"
#include "repiu/platform/virtual_memory.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_code_cache_reservation.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <Zydis.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::runtime::AotBasicBlock;
using repiu::runtime::AotCodeCacheBuildOptions;
using repiu::runtime::AotCodeCacheImage;
using repiu::runtime::AotInstructionKind;
using repiu::runtime::AotInstructionRecord;
using repiu::runtime::AotTranslationPlan;
using repiu::runtime::BuildAotCodeCacheImage;

// Task 558, extended by Task 559. Bytes the x64 emitter produced, executed.
//
// Everything before Task 558 built bytes and looked at them. This places them,
// loads the guest registers where the mapping says they live, puts guest ESP in
// R15, and calls.
//
// The state block is indexed by x86 register number -- offset 4*n -- so its own
// layout is the mapping. The assembly bridge uses those offsets literally.
struct GuestRegisterProbeState
{
    std::uint32_t gpr[8] = {};
    std::uint64_t observed_r15 = 0;
};

static_assert(offsetof(GuestRegisterProbeState, gpr) == 0);
static_assert(offsetof(GuestRegisterProbeState, observed_r15) == 32);

extern "C" void RepiuLinuxX64GuestRegisterProbe(void* code, void* state);

constexpr std::size_t kEax = 0;
constexpr std::size_t kEcx = 1;
constexpr std::size_t kEdx = 2;
constexpr std::size_t kEbx = 3;
constexpr std::size_t kEsp = 4;
constexpr std::size_t kEbp = 5;
constexpr std::size_t kEsi = 6;
constexpr std::size_t kEdi = 7;

constexpr std::uint32_t kGuestBase = 0x00140000U;
constexpr std::size_t kCodeBytes = 0x1000U;
constexpr std::size_t kRegionBytes = 0x4000U;

using Program = std::vector<std::vector<std::uint8_t>>;

// One emitted, placed, runnable program. The region is one reservation with the
// first page executable and the rest left writable, because the guest stack has
// to be written to and executable pages are not.
struct PlacedProgram
{
    bool valid = false;
    runtime::AotCodeCacheReservation reservation;
    std::uint8_t* code = nullptr;
    std::uint8_t* data = nullptr;
    std::uint32_t data_address = 0;
    std::uint32_t copied = 0;
    std::uint32_t lowered = 0;
    std::uint32_t refused = 0;
    // Task 567. Where the probe's own closing `ret` sits, so a handler can send
    // a trapped run there rather than leaving it standing on an INT3.
    std::uint32_t ret_offset = 0;
};

AotTranslationPlan MakePlan(const Program& program)
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestBase;
    AotBasicBlock block;
    block.guest_address = kGuestBase;
    std::uint32_t address = kGuestBase;
    for (const std::vector<std::uint8_t>& bytes : program)
    {
        AotInstructionRecord record;
        record.guest_address = address;
        record.kind = AotInstructionKind::kCopy;
        record.length = static_cast<std::uint8_t>(bytes.size());
        record.bytes = bytes;
        block.instructions.push_back(record);
        address += static_cast<std::uint32_t>(bytes.size());
    }
    // A return closes the block so no fallthrough edge is emitted. It becomes
    // an INT3 under long-mode emission, which is why only the bytes up to the
    // last kCopy are copied out.
    AotInstructionRecord tail;
    tail.guest_address = address;
    tail.kind = AotInstructionKind::kReturn;
    tail.length = 1U;
    tail.bytes = {0xC3U};
    block.instructions.push_back(tail);
    plan.blocks.push_back(block);
    return plan;
}

bool EmittedPrefix(const AotCodeCacheImage& image,
                   const std::uint32_t last_copy_address,
                   std::vector<std::uint8_t>* out)
{
    for (const repiu::runtime::AotAddressMapEntry& map : image.address_map)
    {
        if (map.guest_address != last_copy_address)
        {
            continue;
        }
        const std::size_t end =
            static_cast<std::size_t>(map.cache_offset) + map.emitted_length;
        if (end > image.bytes.size() || end + 1U > kCodeBytes)
        {
            return false;
        }
        out->assign(image.bytes.begin(), image.bytes.begin() + end);
        // The way back is the probe's, appended rather than emitted. What runs
        // before it is exactly what the emitter produced.
        out->push_back(0xC3U);
        return true;
    }
    return false;
}

// Task 560. Split out of `Place` so a probe that builds its own plan -- a
// branch needs more than one block, and `MakePlan` makes one -- can still place
// and run it through exactly the same path.
PlacedProgram PlaceImage(const char* label, const AotCodeCacheImage& image,
                         std::uint32_t last_copy)
{
    PlacedProgram placed;
    placed.copied = image.long_mode_copied_count;
    placed.lowered = image.long_mode_lowered_count;
    placed.refused = image.long_mode_refused_count;
    if (placed.refused != 0U)
    {
        // Task 562 changed this number. The closing return used to be the one
        // refusal every program here carried; it is emitted now, so any refusal
        // at all means an INT3 landed inside the program and what ran would not
        // be what the probe claims.
        std::cout << "  " << label << " refused=" << placed.refused
                  << " (expected 0)\n";
        return placed;
    }

    std::vector<std::uint8_t> executable;
    if (!EmittedPrefix(image, last_copy, &executable))
    {
        std::cout << "  " << label << " prefix=false\n";
        return placed;
    }

    placed.reservation = runtime::ReserveAotCodeCacheMemory(kRegionBytes);
    if (!placed.reservation.valid || placed.reservation.base == nullptr)
    {
        std::cout << "  " << label << " reservation=false\n";
        return placed;
    }
    placed.code = static_cast<std::uint8_t*>(placed.reservation.base);
    placed.data = placed.code + kCodeBytes;
    placed.data_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(placed.data));
    placed.ret_offset = static_cast<std::uint32_t>(executable.size()) - 1U;
    std::memcpy(placed.code, executable.data(), executable.size());

    // Only the code page becomes executable. The rest stays writable, because
    // the guest stack lives there and an executable page is not writable.
    repiu::platform::MemoryProtection previous =
        repiu::platform::MemoryProtection::kNoAccess;
    if (!repiu::platform::ProtectMemory(
            placed.code, kCodeBytes,
            repiu::platform::MemoryProtection::kExecuteRead, &previous))
    {
        std::cout << "  " << label << " protect=false\n";
        runtime::ReleaseAotCodeCacheMemory(placed.reservation);
        return placed;
    }
    placed.valid = true;
    return placed;
}

PlacedProgram Place(const char* label, const Program& program)
{
    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(MakePlan(program), options, &image) ||
        !image.valid)
    {
        std::cout << "  " << label << " emitted=false message=\""
                  << image.message << "\"\n";
        return PlacedProgram{};
    }
    std::uint32_t last_copy = kGuestBase;
    for (std::size_t index = 0; index + 1U < program.size(); ++index)
    {
        last_copy += static_cast<std::uint32_t>(program[index].size());
    }
    return PlaceImage(label, image, last_copy);
}

void Release(const PlacedProgram& placed)
{
    if (placed.reservation.valid)
    {
        runtime::ReleaseAotCodeCacheMemory(placed.reservation);
    }
}

bool Check(const char* name, const std::uint64_t observed,
           const std::uint64_t expected)
{
    const bool ok = observed == expected;
    std::cout << "  " << name << " observed=0x" << std::hex << observed
              << " expected=0x" << expected << std::dec
              << (ok ? "" : "  MISMATCH") << "\n";
    return ok;
}

// A. The register mapping, and that R15 survives code writing all seven.
bool ProbeMapping()
{
    // mov eax,[ebx+4] (0x67 lowering) · inc eax (FF C0) · add eax,ecx and four
    // moves (copied). Between them they write all seven mapped registers.
    const Program program = {
        {0x8BU, 0x43U, 0x04U}, {0x40U},        {0x01U, 0xC8U},
        {0x89U, 0xC2U},        {0x89U, 0xD6U}, {0x89U, 0xF7U},
        {0x89U, 0xFDU},
    };
    const PlacedProgram placed = Place("mapping", program);
    if (!placed.valid)
    {
        std::cout << "guest_register_mapping=false\n";
        return false;
    }
    constexpr std::uint32_t kDataValue = 0x11223344U;
    constexpr std::uint32_t kGuestEcx = 0x00001000U;
    constexpr std::uint32_t kGuestEsp = 0x00ABCDEFU;
    std::memcpy(placed.data + 4U, &kDataValue, sizeof(kDataValue));

    GuestRegisterProbeState state;
    state.gpr[kEbx] = placed.data_address;
    state.gpr[kEcx] = kGuestEcx;
    state.gpr[kEsp] = kGuestEsp;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);

    const std::uint32_t expected = kDataValue + 1U + kGuestEcx;
    bool ok = Check("eax", state.gpr[kEax], expected);
    ok = Check("edx", state.gpr[kEdx], expected) && ok;
    ok = Check("ebp", state.gpr[kEbp], expected) && ok;
    // R15 held guest ESP across all of that, and its upper half is zero -- the
    // bridge poisons it before the 32-bit load, so a zero here is that load's
    // doing rather than what happened to be there.
    ok = Check("r15", state.observed_r15, kGuestEsp) && ok;
    std::cout << "guest_register_mapping=" << (ok ? "true" : "false")
              << " copied=" << placed.copied << " lowered=" << placed.lowered
              << "\n";
    Release(placed);
    return ok;
}

// B. Task 559. The stack sequences, executed: does a push reach guest memory,
// does a pop bring it back, and does ESP move by exactly four.
bool ProbeStackData()
{
    // push eax · push ecx · pop edx
    // Leaves ESP four below where it started, with eax's value at that address.
    const Program program = {{0x50U}, {0x51U}, {0x5AU}};
    const PlacedProgram placed = Place("stack", program);
    if (!placed.valid)
    {
        std::cout << "guest_stack_data=false\n";
        return false;
    }
    constexpr std::uint32_t kPushedEax = 0xA1B2C3D4U;
    constexpr std::uint32_t kPushedEcx = 0x5566778899U & 0xFFFFFFFFU;
    // The guest stack lives in the writable half of the region, high enough
    // that pushes move down into it rather than off the page.
    const std::uint32_t stack_top = placed.data_address + 0x800U;

    GuestRegisterProbeState state;
    state.gpr[kEax] = kPushedEax;
    state.gpr[kEcx] = kPushedEcx;
    state.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);

    // The pop took what the second push wrote.
    bool ok = Check("edx_from_pop", state.gpr[kEdx], kPushedEcx);
    // Two pushes and one pop leave ESP four below the top.
    ok = Check("esp_after", state.observed_r15,
               static_cast<std::uint64_t>(stack_top - 4U)) && ok;
    // And the first push really wrote guest memory, read back here rather than
    // inferred from a register.
    std::uint32_t in_memory = 0U;
    std::memcpy(&in_memory, placed.data + 0x800U - 4U, sizeof(in_memory));
    ok = Check("guest_stack_memory", in_memory, kPushedEax) && ok;
    std::cout << "guest_stack_data=" << (ok ? "true" : "false")
              << " lowered=" << placed.lowered << "\n";
    Release(placed);
    return ok;
}

// C. Task 559 decision 1. Guest PUSH and POP change no flags, so the emitted
// sequence must not either -- which is why the ESP adjustment is a LEA and not
// a SUB.
//
// This is the item worth the most here. A wrong value announces itself; wrong
// flags raise nothing at all and simply send the next branch elsewhere.
bool ProbeFlagsSurvive()
{
    // xor edx,edx · cmp eax,eax (ZF=1) · push ebx · pop edi · setz dl
    // EDX comes back 1 only if the push and pop left ZF alone.
    const Program program = {
        {0x31U, 0xD2U}, {0x39U, 0xC0U},        {0x53U},
        {0x5FU},        {0x0FU, 0x94U, 0xC2U},
    };
    const PlacedProgram placed = Place("flags", program);
    if (!placed.valid)
    {
        std::cout << "guest_stack_flags=false\n";
        return false;
    }
    GuestRegisterProbeState state;
    state.gpr[kEax] = 0x12345678U;
    state.gpr[kEbx] = 0x0BADF00DU;
    state.gpr[kEsp] = placed.data_address + 0x800U;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);

    const bool ok = Check("zf_after_push_pop", state.gpr[kEdx], 1U);
    // The pop still moved the value, so a passing flag check is not a sequence
    // that did nothing.
    const bool moved = Check("edi_from_pop", state.gpr[kEdi], 0x0BADF00DU);
    std::cout << "guest_stack_flags=" << (ok && moved ? "true" : "false")
              << "\n";
    Release(placed);
    return ok && moved;
}

// D. PUSHFD and POPFD round trip through guest memory and restore the flags.
bool ProbeFlagsRoundTrip()
{
    // cmp eax,eax (ZF=1) · pushfd · cmp eax,ebx (ZF=0) · popfd · setz dl
    // EDX is 1 only if popfd brought the saved ZF back.
    const Program program = {
        {0x39U, 0xC0U}, {0x9CU},        {0x39U, 0xD8U},
        {0x9DU},        {0x0FU, 0x94U, 0xC2U},
    };
    const PlacedProgram placed = Place("flags_round_trip", program);
    if (!placed.valid)
    {
        std::cout << "guest_stack_flags_round_trip=false\n";
        return false;
    }
    GuestRegisterProbeState state;
    state.gpr[kEax] = 0x12345678U;
    state.gpr[kEbx] = 0x87654321U;
    const std::uint32_t stack_top = placed.data_address + 0x800U;
    state.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);

    bool ok = Check("zf_restored", state.gpr[kEdx], 1U);
    // Balanced: pushfd then popfd leaves ESP where it started.
    ok = Check("esp_balanced", state.observed_r15,
               static_cast<std::uint64_t>(stack_top)) && ok;
    std::cout << "guest_stack_flags_round_trip=" << (ok ? "true" : "false")
              << "\n";
    Release(placed);
    return ok;
}

// Task 560. A conditional branch, taken and not taken.
//
// Checking one direction would be checking a straight line: a `jz` that never
// jumps and an emitter that dropped the branch entirely produce the same
// answer. Two directions from one image is what makes it a branch.
//
// Three blocks, because that is what a branch needs. Block 1 tests ECX and
// jumps to block 3; block 2 is the fallthrough that block 1 skips when the
// branch is taken; block 3 is the join. The plan is built here rather than
// through `MakePlan` because that helper makes one block, and one block cannot
// hold an edge.
bool ProbeConditionalBranch()
{
    // Block 1 @ +0x00: mov eax,0x1111 · test ecx,ecx · jz L
    // Block 2 @ +0x09: mov eax,0x2222          (skipped when taken)
    // Block 3 @ +0x0E: nop                     (L, the join)
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestBase;

    const std::uint32_t block2 = kGuestBase + 0x09U;
    const std::uint32_t label = kGuestBase + 0x0EU;

    AotBasicBlock first;
    first.guest_address = kGuestBase;
    AotInstructionRecord load_taken;
    load_taken.guest_address = kGuestBase;
    load_taken.kind = AotInstructionKind::kCopy;
    load_taken.bytes = {0xB8U, 0x11U, 0x11U, 0x00U, 0x00U};
    load_taken.length = 5U;
    first.instructions.push_back(load_taken);
    AotInstructionRecord test;
    test.guest_address = kGuestBase + 0x05U;
    test.kind = AotInstructionKind::kCopy;
    test.bytes = {0x85U, 0xC9U};
    test.length = 2U;
    first.instructions.push_back(test);
    AotInstructionRecord branch;
    branch.guest_address = kGuestBase + 0x07U;
    branch.kind = AotInstructionKind::kConditionalBranch;
    branch.mnemonic = static_cast<std::uint16_t>(ZYDIS_MNEMONIC_JZ);
    branch.direct_target = label;
    branch.fallthrough_target = block2;
    branch.length = 2U;
    branch.bytes = {0x74U, 0x05U};
    first.instructions.push_back(branch);
    plan.blocks.push_back(first);

    AotBasicBlock second;
    second.guest_address = block2;
    AotInstructionRecord load_fallthrough;
    load_fallthrough.guest_address = block2;
    load_fallthrough.kind = AotInstructionKind::kCopy;
    load_fallthrough.bytes = {0xB8U, 0x22U, 0x22U, 0x00U, 0x00U};
    load_fallthrough.length = 5U;
    second.instructions.push_back(load_fallthrough);
    plan.blocks.push_back(second);

    AotBasicBlock join;
    join.guest_address = label;
    AotInstructionRecord nop;
    nop.guest_address = label;
    nop.kind = AotInstructionKind::kCopy;
    nop.bytes = {0x90U};
    nop.length = 1U;
    join.instructions.push_back(nop);
    // The closing return, exactly as `MakePlan` ends its one block. Without it
    // the join block falls through to an address no block owns, and the first
    // run of this probe showed what that costs: the emitter turned that edge
    // into a boundary and reported `unresolved=1`. The neutralisation was
    // right; the program was the thing that was wrong.
    AotInstructionRecord tail;
    tail.guest_address = label + 1U;
    tail.kind = AotInstructionKind::kReturn;
    tail.length = 1U;
    tail.bytes = {0xC3U};
    join.instructions.push_back(tail);
    plan.blocks.push_back(join);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(plan, options, &image) || !image.valid)
    {
        std::cout << "guest_conditional_branch=false message=\""
                  << image.message << "\"\n";
        return false;
    }
    // One branch, and its edge landed inside the image. An unresolved edge here
    // would mean the branch reached a boundary instead of a target, and the two
    // directions below would then be measuring an INT3.
    if (image.long_mode_branch_count != 1U ||
        image.long_mode_unresolved_branch_count != 0U)
    {
        std::cout << "guest_conditional_branch=false branches="
                  << image.long_mode_branch_count << " unresolved="
                  << image.long_mode_unresolved_branch_count << "\n";
        return false;
    }

    const PlacedProgram placed = PlaceImage("branch", image, label);
    if (!placed.valid)
    {
        std::cout << "guest_conditional_branch=false\n";
        return false;
    }

    GuestRegisterProbeState taken;
    taken.gpr[kEcx] = 0U;  // ZF set, so the branch is taken
    RepiuLinuxX64GuestRegisterProbe(placed.code, &taken);

    GuestRegisterProbeState fell_through;
    fell_through.gpr[kEcx] = 1U;  // ZF clear, so it falls through
    RepiuLinuxX64GuestRegisterProbe(placed.code, &fell_through);

    bool ok = Check("branch_taken_eax", taken.gpr[kEax], 0x1111U);
    ok = Check("branch_fallthrough_eax", fell_through.gpr[kEax], 0x2222U) && ok;
    std::cout << "guest_conditional_branch=" << (ok ? "true" : "false")
              << " branches=" << image.long_mode_branch_count << "\n";
    Release(placed);
    return ok;
}

// Task 561. A direct call: the jump, and the return address it leaves behind.
//
// Checking only that control arrived at the callee would confirm a jump, not a
// call. What makes it a call is the guest return address on the guest stack, so
// that is read out of guest memory directly rather than inferred.
//
// Three blocks again. The caller calls the third; the second is the fallthrough
// the call must step over, and its value is what would come back if the emitter
// had produced a plain fallthrough instead of a jump to the target.
bool ProbeDirectCall()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestBase;

    const std::uint32_t after_call = kGuestBase + 0x0AU;
    const std::uint32_t callee = kGuestBase + 0x0FU;

    AotBasicBlock caller;
    caller.guest_address = kGuestBase;
    AotInstructionRecord marker;
    marker.guest_address = kGuestBase;
    marker.kind = AotInstructionKind::kCopy;
    marker.bytes = {0xB8U, 0x11U, 0x11U, 0x00U, 0x00U};
    marker.length = 5U;
    caller.instructions.push_back(marker);
    AotInstructionRecord call;
    call.guest_address = kGuestBase + 0x05U;
    call.kind = AotInstructionKind::kDirectCall;
    call.direct_target = callee;
    call.fallthrough_target = after_call;
    call.length = 5U;
    call.bytes = {0xE8U, 0x05U, 0x00U, 0x00U, 0x00U};
    caller.instructions.push_back(call);
    plan.blocks.push_back(caller);

    // Stepped over when the call jumps. Left without a closing return so it
    // falls through into the callee, which keeps the image to exactly one
    // refusal -- the callee's own return.
    AotBasicBlock skipped;
    skipped.guest_address = after_call;
    AotInstructionRecord skipped_marker;
    skipped_marker.guest_address = after_call;
    skipped_marker.kind = AotInstructionKind::kCopy;
    skipped_marker.bytes = {0xB8U, 0x22U, 0x22U, 0x00U, 0x00U};
    skipped_marker.length = 5U;
    skipped.instructions.push_back(skipped_marker);
    plan.blocks.push_back(skipped);

    AotBasicBlock target;
    target.guest_address = callee;
    AotInstructionRecord callee_marker;
    callee_marker.guest_address = callee;
    callee_marker.kind = AotInstructionKind::kCopy;
    callee_marker.bytes = {0xB8U, 0x33U, 0x33U, 0x00U, 0x00U};
    callee_marker.length = 5U;
    target.instructions.push_back(callee_marker);
    AotInstructionRecord tail;
    tail.guest_address = callee + 5U;
    tail.kind = AotInstructionKind::kReturn;
    tail.length = 1U;
    tail.bytes = {0xC3U};
    target.instructions.push_back(tail);
    plan.blocks.push_back(target);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(plan, options, &image) || !image.valid)
    {
        std::cout << "guest_direct_call=false message=\"" << image.message
                  << "\"\n";
        return false;
    }
    // One branch is the call. The fallthrough edge out of the skipped block is
    // a `kBlockFallthrough` and is not counted here.
    if (image.long_mode_branch_count != 1U ||
        image.long_mode_unresolved_branch_count != 0U)
    {
        std::cout << "guest_direct_call=false branches="
                  << image.long_mode_branch_count << " unresolved="
                  << image.long_mode_unresolved_branch_count << "\n";
        return false;
    }

    const PlacedProgram placed = PlaceImage("call", image, callee);
    if (!placed.valid)
    {
        std::cout << "guest_direct_call=false\n";
        return false;
    }
    const std::uint32_t stack_top = placed.data_address + 0x800U;
    GuestRegisterProbeState state;
    state.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);

    // The callee ran, which means the jump went to the target rather than
    // falling through: 0x2222 here would be the block the call had to skip.
    bool ok = Check("call_reached_callee", state.gpr[kEax], 0x3333U);
    // And it was a call, not a jump. The guest return address is read out of
    // guest stack memory rather than taken from a register.
    std::uint32_t pushed = 0U;
    std::memcpy(&pushed, placed.data + 0x800U - 4U, sizeof(pushed));
    ok = Check("call_return_address", pushed, after_call) && ok;
    ok = Check("call_esp", state.observed_r15,
               static_cast<std::uint64_t>(stack_top - 4U)) && ok;
    std::cout << "guest_direct_call=" << (ok ? "true" : "false") << "\n";
    Release(placed);
    return ok;
}

// Task 561. A call whose target is not in the image, checked as bytes.
//
// This path nearly shipped wrong. The first version overwrote the call's *jump*
// with the INT3 and left the push in front of it, which would have run: guest
// ESP moved and a return address stored, and then a boundary whose handler
// resumes at the guest's own `call` and pushes a second time. The slot has to
// become a boundary from its first byte.
//
// Nothing is executed here -- an INT3 is the point of it. What is checked is
// that the emitter reported the refusal and that the first byte of the slot is
// the trap, so the push cannot run ahead of it.
bool ProbeUnresolvedCall()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestBase;

    AotBasicBlock caller;
    caller.guest_address = kGuestBase;
    AotInstructionRecord call;
    call.guest_address = kGuestBase;
    call.kind = AotInstructionKind::kDirectCall;
    // A target no block in this plan owns.
    call.direct_target = kGuestBase + 0x8000U;
    call.fallthrough_target = kGuestBase + 0x05U;
    call.length = 5U;
    call.bytes = {0xE8U, 0x00U, 0x00U, 0x00U, 0x00U};
    caller.instructions.push_back(call);
    plan.blocks.push_back(caller);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(plan, options, &image) || !image.valid)
    {
        std::cout << "guest_unresolved_call=false message=\"" << image.message
                  << "\"\n";
        return false;
    }

    bool ok = image.long_mode_unresolved_branch_count == 1U;
    std::uint32_t slot = 0xFFFFFFFFU;
    for (const repiu::runtime::AotAddressMapEntry& entry : image.address_map)
    {
        if (entry.guest_address == kGuestBase)
        {
            slot = entry.cache_offset;
        }
    }
    const bool traps_first = slot < image.bytes.size() &&
        image.bytes[slot] == 0xCCU;
    ok = ok && traps_first;
    std::cout << "  unresolved_call_reported="
              << image.long_mode_unresolved_branch_count
              << " traps_at_slot_start=" << (traps_first ? 1 : 0) << "\n";
    std::cout << "guest_unresolved_call=" << (ok ? "true" : "false") << "\n";
    return ok;
}

// Task 562. What the resolver is asked, and what it answers with.
//
// The address map the image already carries is the whole lookup: a guest
// address in, the host address of that guest instruction's cache entry out.
struct ResolverContext
{
    const AotCodeCacheImage* image = nullptr;
    const std::uint8_t* code = nullptr;
    std::uint32_t asked = 0;
    std::uint32_t calls = 0;
};

ResolverContext g_resolver_context;

std::uintptr_t TestResolver(void* context,
                            repiu::platform::LinuxX64AotDispatchFrame* frame)
{
    auto* const state = static_cast<ResolverContext*>(context);
    if (state == nullptr || frame == nullptr || state->image == nullptr)
    {
        return 0U;
    }
    ++state->calls;
    state->asked = frame->guest_source;
    for (const repiu::runtime::AotAddressMapEntry& entry :
         state->image->address_map)
    {
        if (entry.guest_address == frame->guest_source)
        {
            return reinterpret_cast<std::uintptr_t>(state->code) +
                entry.cache_offset;
        }
    }
    // Nothing found is answered with zero, which the thunk turns into a trap.
    return 0U;
}

// Task 562. A call and a return, joined.
//
// This is the one piece of evidence the unit worked. Three markers separate
// three different failures: 0x4444 means the return came back and the
// instruction after the call ran; 0x3333 means control reached the callee and
// stopped there; 0x1111 means the call never went at all.
bool ProbeCallAndReturn()
{
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestBase;

    const std::uint32_t after_call = kGuestBase + 0x0AU;
    const std::uint32_t callee = kGuestBase + 0x10U;

    AotBasicBlock caller;
    caller.guest_address = kGuestBase;
    AotInstructionRecord first;
    first.guest_address = kGuestBase;
    first.kind = AotInstructionKind::kCopy;
    first.bytes = {0xB8U, 0x11U, 0x11U, 0x00U, 0x00U};
    first.length = 5U;
    caller.instructions.push_back(first);
    AotInstructionRecord call;
    call.guest_address = kGuestBase + 0x05U;
    call.kind = AotInstructionKind::kDirectCall;
    call.direct_target = callee;
    call.fallthrough_target = after_call;
    call.length = 5U;
    call.bytes = {0xE8U, 0x06U, 0x00U, 0x00U, 0x00U};
    caller.instructions.push_back(call);
    plan.blocks.push_back(caller);

    // Laid out before the block it returns to, because the call has to be able
    // to reach it and the executable prefix is cut after the *last* thing that
    // runs -- which is the instruction the return comes back to.
    AotBasicBlock target;
    target.guest_address = callee;
    AotInstructionRecord callee_marker;
    callee_marker.guest_address = callee;
    callee_marker.kind = AotInstructionKind::kCopy;
    callee_marker.bytes = {0xB8U, 0x33U, 0x33U, 0x00U, 0x00U};
    callee_marker.length = 5U;
    target.instructions.push_back(callee_marker);
    AotInstructionRecord ret;
    ret.guest_address = callee + 5U;
    ret.kind = AotInstructionKind::kReturn;
    ret.length = 1U;
    ret.bytes = {0xC3U};
    target.instructions.push_back(ret);
    plan.blocks.push_back(target);

    AotBasicBlock resumed;
    resumed.guest_address = after_call;
    AotInstructionRecord resumed_marker;
    resumed_marker.guest_address = after_call;
    resumed_marker.kind = AotInstructionKind::kCopy;
    resumed_marker.bytes = {0xB8U, 0x44U, 0x44U, 0x00U, 0x00U};
    resumed_marker.length = 5U;
    resumed.instructions.push_back(resumed_marker);
    AotInstructionRecord resumed_ret;
    resumed_ret.guest_address = after_call + 5U;
    resumed_ret.kind = AotInstructionKind::kReturn;
    resumed_ret.length = 1U;
    resumed_ret.bytes = {0xC3U};
    resumed.instructions.push_back(resumed_ret);
    plan.blocks.push_back(resumed);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(plan, options, &image) || !image.valid)
    {
        std::cout << "guest_call_and_return=false message=\"" << image.message
                  << "\"\n";
        return false;
    }
    if (image.long_mode_return_count != 2U ||
        image.long_mode_refused_count != 0U)
    {
        std::cout << "guest_call_and_return=false returns="
                  << image.long_mode_return_count << " refused="
                  << image.long_mode_refused_count << "\n";
        return false;
    }

    const PlacedProgram placed = PlaceImage("call_return", image, after_call);
    if (!placed.valid)
    {
        std::cout << "guest_call_and_return=false\n";
        return false;
    }

    g_resolver_context = ResolverContext{};
    g_resolver_context.image = &image;
    g_resolver_context.code = placed.code;
    repiu::platform::LinuxX64AotDispatchFrame frame;
    repiu::platform::InstallLinuxX64Dispatch(&frame, &g_resolver_context,
                                             &TestResolver);

    const std::uint32_t stack_top = placed.data_address + 0x800U;
    GuestRegisterProbeState state;
    state.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);
    repiu::platform::ClearLinuxX64Dispatch();

    bool ok = Check("returned_to_after_call", state.gpr[kEax], 0x4444U);
    // The resolver was asked about the address the call pushed, which is what
    // makes this a return rather than a jump that happened to land right.
    ok = Check("resolver_asked", g_resolver_context.asked, after_call) && ok;
    ok = Check("resolver_calls", g_resolver_context.calls, 1U) && ok;
    // And the guest stack is balanced again: the call pushed four, the return
    // took them back.
    ok = Check("esp_balanced", state.observed_r15,
               static_cast<std::uint64_t>(stack_top)) && ok;
    std::cout << "guest_call_and_return=" << (ok ? "true" : "false") << "\n";
    Release(placed);
    return ok;
}

// Task 564. Guest `ESP` named in each of the three encoding places, executed.
//
// The third case is the one that needs care. `add esp, 16` re-encoded wrongly
// writes the *host's* stack pointer, and a probe that only compared values
// might not notice -- so what is checked is that the run comes back at all.
// Without the re-encoding, RSP would have moved by 16 at that instruction and
// the return address would be read from the wrong place.
bool ProbeStackPointerReencode()
{
    // mov eax,[esp+8] (SIB base) · add esp,16 (ModRM rm) · mov edx,esp (ModRM
    // reg, with ESP as the source).
    const Program program = {
        {0x8BU, 0x44U, 0x24U, 0x08U},
        {0x83U, 0xC4U, 0x10U},
        {0x89U, 0xE2U},
    };
    const PlacedProgram placed = Place("esp_reencode", program);
    if (!placed.valid)
    {
        std::cout << "guest_esp_reencode=false\n";
        return false;
    }
    constexpr std::uint32_t kOnStack = 0xC0FFEE01U;
    const std::uint32_t stack_top = placed.data_address + 0x800U;
    // The value `[esp+8]` must find.
    std::memcpy(placed.data + 0x800U + 8U, &kOnStack, sizeof(kOnStack));

    GuestRegisterProbeState state;
    state.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &state);

    // The SIB-base form read the guest stack rather than the host's.
    bool ok = Check("esp_memory_base", state.gpr[kEax], kOnStack);
    // The ModRM-rm form moved guest ESP, which lives in R15D.
    ok = Check("esp_register_operand", state.observed_r15,
               static_cast<std::uint64_t>(stack_top + 16U)) && ok;
    // The ModRM-reg form read guest ESP out, after the add.
    ok = Check("esp_as_source", state.gpr[kEdx], stack_top + 16U) && ok;
    // Reaching this line at all is the host's stack pointer having survived
    // `add esp,16`. Un-re-encoded, that instruction moves RSP and the return
    // never comes back here.
    std::cout << "guest_esp_reencode=" << (ok ? "true" : "false")
              << " lowered=" << placed.lowered << "\n";
    Release(placed);
    return ok;
}

// Task 567. Where a trapped run is sent, and how many times it trapped.
//
// The guard's fallback is an INT3, so the mismatching run has to be caught or
// it ends the probe. The handler redirects to the closing `ret` the harness
// appended, which is reached with flags restored and guest ESP balanced --
// the fallback path does both before trapping.
std::uintptr_t g_boundary_target = 0;
std::uint32_t g_boundary_hits = 0;

repiu::platform::FaultDisposition OnSegmentBoundary(
    repiu::platform::FaultEvent* event, void* /*user_data*/)
{
    if (event == nullptr || event->registers == nullptr ||
        event->kind != repiu::platform::FaultKind::kBreakpoint)
    {
        return repiu::platform::FaultDisposition::kNotHandled;
    }
    ++g_boundary_hits;
    event->registers->Eip = static_cast<std::uint32_t>(g_boundary_target);
    return repiu::platform::FaultDisposition::kResume;
}

// Task 567. The segment-override slot, patched and then run both ways.
//
// The guard is the whole point of the slot, and a probe that only ran the
// matching case would not be able to tell a guard from its absence -- the
// access would read the right byte either way. So the selector is made to
// disagree on a second run and the boundary has to be reached.
//
// The patching is done here rather than assumed, because nothing on x64 patches
// these yet. An unpatched slot reads with a base of zero, which is the exact
// error the fold exists to prevent, so "emitted" and "correct" are held apart
// by doing the patch in the test that claims correctness.
bool ProbeSegmentOverride()
{
    // 26 8B 1D <disp32>: mov ebx, es:[disp32]. The shape Task 565's chain
    // stopped on, and the one this unit admits.
    AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestBase;
    AotBasicBlock block;
    block.guest_address = kGuestBase;
    AotInstructionRecord access;
    access.guest_address = kGuestBase;
    access.kind = AotInstructionKind::kSegmentOverrideMem;
    access.segment_override_register = 0U;  // ES
    access.bytes = {0x26U, 0x8BU, 0x1DU, 0x40U, 0x00U, 0x00U, 0x00U};
    access.length = 7U;
    block.instructions.push_back(access);
    AotInstructionRecord tail;
    tail.guest_address = kGuestBase + 7U;
    tail.kind = AotInstructionKind::kCopy;
    tail.bytes = {0x90U};
    tail.length = 1U;
    block.instructions.push_back(tail);
    AotInstructionRecord closing;
    closing.guest_address = kGuestBase + 8U;
    closing.kind = AotInstructionKind::kReturn;
    closing.bytes = {0xC3U};
    closing.length = 1U;
    block.instructions.push_back(closing);
    plan.blocks.push_back(block);

    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    options.enable_long_mode_segment_override = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(plan, options, &image) || !image.valid ||
        image.long_mode_segment_override_count != 1U ||
        image.segment_override_sites.size() != 1U)
    {
        std::cout << "guest_segment_override=false slots="
                  << image.long_mode_segment_override_count << " message=\""
                  << image.message << "\"\n";
        return false;
    }

    const PlacedProgram placed =
        PlaceImage("segment", image, kGuestBase + 7U);
    if (!placed.valid)
    {
        std::cout << "guest_segment_override=false\n";
        return false;
    }

    // The guest data the fold has to reach, and the shadow selector the guard
    // reads. Both live in the writable half, whose address is below 4 GiB.
    constexpr std::uint32_t kSegmentBase = 0x100U;
    constexpr std::uint32_t kGuestDisplacement = 0x40U;
    constexpr std::uint16_t kSelector = 0x0024U;
    constexpr std::uint32_t kValue = 0xFEEDFACEU;
    const std::uint32_t shadow_address = placed.data_address;
    const std::uint32_t data_address =
        placed.data_address + 0x200U;
    // The fold: the access must reach base + displacement, so the base is
    // chosen to land the sum on the data.
    const std::uint32_t folded = data_address - kGuestDisplacement;
    std::memcpy(placed.data + 0x200U, &kValue, sizeof(kValue));
    std::uint16_t shadow = kSelector;
    std::memcpy(placed.data, &shadow, sizeof(shadow));
    (void)kSegmentBase;

    // Task 568. Patch with the engine's own patcher, not by hand.
    //
    // Task 567's version of this probe wrote the three fields itself with
    // `memcpy`. That verified the slot but could not verify the thing this unit
    // changes, because the patcher is what is under test. It opens its own
    // write window and re-protects afterwards, so the manual unlock is gone.
    // The write window is the probe's here, because the runtime patcher takes
    // bytes that are already writable -- opening the page is the engine's half
    // of the split, and the engine is not linked into this probe. `PlaceImage`
    // leaves the code execute-only, and patching through that faults, which is
    // how Task 567's first run ended.
    if (!repiu::platform::ProtectMemory(
            placed.code, kCodeBytes,
            repiu::platform::MemoryProtection::kExecuteReadWrite, nullptr))
    {
        std::cout << "guest_segment_override=false,reason=unlock\n";
        Release(placed);
        return false;
    }

    repiu::runtime::AotSegmentTable segment_table;
    repiu::runtime::AotSegmentResolution& resolution =
        segment_table.segments[
            image.segment_override_sites.front().segment_register];
    resolution.shadow_address = shadow_address;
    resolution.selector = kSelector;
    // A base, not a pre-folded address: folding it into the guest displacement
    // is the patcher's job and therefore part of what is under test.
    resolution.base = folded;
    resolution.policy = repiu::runtime::AotSegmentAccessPolicy::kNativeFolded;

    repiu::runtime::AotSegmentOverridePatchStats patch_stats;
    const std::uint32_t patched =
        repiu::runtime::PatchAotSegmentOverrideSites(
            placed.code, image.segment_override_sites, segment_table,
            &patch_stats);
    bool ok = Check("segment_patcher_sites", patched, 1U);
    ok = Check("segment_patcher_native", patch_stats.native_site_count, 1U) &&
         ok;
    if (!repiu::platform::FlushInstructionCacheRange(placed.code, kCodeBytes))
    {
        std::cout << "guest_segment_override=false,reason=flush\n";
        Release(placed);
        return false;
    }

    const std::uint32_t stack_top = placed.data_address + 0x800U;
    GuestRegisterProbeState matched;
    matched.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &matched);

    ok = Check("segment_access_value", matched.gpr[kEbx], kValue) && ok;
    // Flags and the guest stack came back as they went in: the guard's compare
    // is bracketed by the lowered pushfd/popfd, and neither may leak.
    ok = Check("segment_esp_balanced", matched.observed_r15,
               static_cast<std::uint64_t>(stack_top)) && ok;

    // The other direction. Without it this would be measuring the access and
    // not the guard: with the fold correct, a slot that had no guard at all
    // would read the same right value.
    g_boundary_target = reinterpret_cast<std::uintptr_t>(placed.code) +
        placed.ret_offset;
    g_boundary_hits = 0U;
    if (!repiu::platform::InstallFaultHandler(&OnSegmentBoundary, nullptr))
    {
        std::cout << "guest_segment_override=false,reason=handler\n";
        Release(placed);
        return false;
    }
    shadow = static_cast<std::uint16_t>(kSelector + 1U);
    std::memcpy(placed.data, &shadow, sizeof(shadow));
    GuestRegisterProbeState mismatched;
    mismatched.gpr[kEsp] = stack_top;
    mismatched.gpr[kEbx] = 0U;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &mismatched);
    repiu::platform::RemoveFaultHandler();

    ok = Check("segment_guard_boundary", g_boundary_hits, 1U) && ok;
    // The access must not have run: EBX is still what it went in as.
    ok = Check("segment_guard_no_access", mismatched.gpr[kEbx], 0U) && ok;
    // And the fallback restored flags before trapping, so guest ESP balances
    // on this path too.
    ok = Check("segment_guard_esp", mismatched.observed_r15,
               static_cast<std::uint64_t>(stack_top)) && ok;

    // Task 568. The round trip the recorded prologue exists for.
    //
    // Everything above leaves the slot's opening bytes untouched, so the
    // restore writes back what is already there and a wrong recording would
    // only show up as corruption. Routing the site to HLE first makes the
    // restore do real work: with no companion slot the patcher stamps `0xCC`
    // over the first byte, and going back to native has to undo exactly that.
    // This is the path that would have silently broken had the patcher kept
    // its i386 constant.
    repiu::runtime::AotSegmentOverridePatchStats hle_stats;
    resolution.policy = repiu::runtime::AotSegmentAccessPolicy::kHleLowMemory;
    repiu::runtime::PatchAotSegmentOverrideSites(
        placed.code, image.segment_override_sites, segment_table, &hle_stats);
    ok = Check("segment_hle_routed", hle_stats.hle_site_count, 1U) && ok;
    ok = Check("segment_hle_trapped", placed.code[0], 0xCCU) && ok;

    repiu::runtime::AotSegmentOverridePatchStats restore_stats;
    resolution.policy = repiu::runtime::AotSegmentAccessPolicy::kNativeFolded;
    repiu::runtime::PatchAotSegmentOverrideSites(
        placed.code, image.segment_override_sites, segment_table,
        &restore_stats);
    ok = Check("segment_restore_native", restore_stats.native_site_count, 1U) &&
         ok;
    if (!repiu::platform::FlushInstructionCacheRange(placed.code, kCodeBytes))
    {
        std::cout << "guest_segment_override=false,reason=reflush\n";
        Release(placed);
        return false;
    }
    // Executed, not merely compared: the whole slot has to work again, which a
    // byte-by-byte comparison against a remembered head would not establish.
    shadow = kSelector;
    std::memcpy(placed.data, &shadow, sizeof(shadow));
    GuestRegisterProbeState restored;
    restored.gpr[kEsp] = stack_top;
    RepiuLinuxX64GuestRegisterProbe(placed.code, &restored);
    ok = Check("segment_restored_value", restored.gpr[kEbx], kValue) && ok;

    std::cout << "guest_segment_override=" << (ok ? "true" : "false")
              << " slots=" << image.long_mode_segment_override_count << "\n";
    Release(placed);
    return ok;
}

}  // namespace

bool RunLinuxX64GuestRegisterProbe()
{
    const bool mapping = ProbeMapping();
    const bool stack = ProbeStackData();
    const bool flags = ProbeFlagsSurvive();
    const bool round_trip = ProbeFlagsRoundTrip();
    const bool branch = ProbeConditionalBranch();
    const bool call = ProbeDirectCall();
    const bool unresolved = ProbeUnresolvedCall();
    const bool call_return = ProbeCallAndReturn();
    const bool esp = ProbeStackPointerReencode();
    const bool segment = ProbeSegmentOverride();
    const bool all =
        mapping && stack && flags && round_trip && branch && call &&
        unresolved && call_return && esp && segment;
    std::cout << "linux_x64_guest_register_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
