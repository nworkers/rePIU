#include "linux_x64_guest_register_probe.h"

#include "repiu/platform/linux_x64_guest_registers.h"
#include "repiu/platform/virtual_memory.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_code_cache_reservation.h"
#include "repiu/runtime/aot_translation_plan.h"

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

PlacedProgram Place(const char* label, const Program& program)
{
    PlacedProgram placed;
    AotCodeCacheBuildOptions options;
    options.enable_long_mode_emission = true;
    AotCodeCacheImage image;
    if (!BuildAotCodeCacheImage(MakePlan(program), options, &image) ||
        !image.valid)
    {
        std::cout << "  " << label << " emitted=false message=\""
                  << image.message << "\"\n";
        return placed;
    }
    placed.copied = image.long_mode_copied_count;
    placed.lowered = image.long_mode_lowered_count;
    placed.refused = image.long_mode_refused_count;
    if (placed.refused != 1U)
    {
        // One refusal is the closing return. More means an INT3 landed inside
        // the program, and what ran would not be what this probe claims.
        std::cout << "  " << label << " refused=" << placed.refused
                  << " (expected 1, the closing return)\n";
        return placed;
    }

    std::uint32_t last_copy = kGuestBase;
    for (std::size_t index = 0; index + 1U < program.size(); ++index)
    {
        last_copy += static_cast<std::uint32_t>(program[index].size());
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

}  // namespace

bool RunLinuxX64GuestRegisterProbe()
{
    const bool mapping = ProbeMapping();
    const bool stack = ProbeStackData();
    const bool flags = ProbeFlagsSurvive();
    const bool round_trip = ProbeFlagsRoundTrip();
    const bool all = mapping && stack && flags && round_trip;
    std::cout << "linux_x64_guest_register_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
