#include "long_mode_lowering_probe.h"

#include "repiu/platform/virtual_memory.h"
#include "repiu/runtime/aot_long_mode_compatibility.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::platform::MemoryProtection;
using repiu::platform::MemoryReservation;
using repiu::runtime::ClassifyLongModeBytes;
using repiu::runtime::GuestCodeDefaultOperandSize;
using repiu::runtime::kMaxLoweredBytes;
using repiu::runtime::LongModeByteCompatibility;
using repiu::runtime::LongModeLowering;
using repiu::runtime::LowerLongModeBytes;

// Task 552. The lowering is checked by running it.
//
// The claim this probe exists for is "rewritten this way, the instruction reads
// the address the guest meant". That is a claim about a processor, and quoting
// a manual at it is not a measurement. So the lowered bytes are written into an
// executable page and called.
//
// The page holding the *data* is placed below 4 GiB deliberately. That is the
// whole content of Task 546's decision 4: a 32-bit address computation
// zero-extended to 64 bits only names the right byte while the target is down
// there. 0x30000000 sits clear of the guest arena, which ends below
// 0x085E7000, and clear of the engine's own text at 0x40000000.
constexpr std::uint32_t kDataPageAddress = 0x30000000U;
constexpr std::size_t kPageBytes = 4096U;
constexpr std::uint32_t kMarker = 0x5A17C0DEU;
constexpr std::size_t kMarkerOffset = 64U;

struct ExecutablePage
{
    void* base = nullptr;
    std::size_t size = 0;
};

bool MakeExecutable(void* base, std::size_t size)
{
    if (!platform::ProtectMemory(base, size,
                                 MemoryProtection::kExecuteReadWrite, nullptr))
    {
        return false;
    }
    return platform::FlushInstructionCacheRange(base, size);
}

// Placed wherever the host likes rather than at a chosen address, because two
// of these at two different addresses is exactly how the RIP-relative question
// gets answered below.
bool AllocateCodePage(ExecutablePage* page)
{
    const MemoryReservation reserved = platform::ReserveMemory(
        nullptr, kPageBytes, true, MemoryProtection::kReadWrite);
    if (!reserved.valid || reserved.base == nullptr)
    {
        return false;
    }
    page->base = reserved.base;
    page->size = kPageBytes;
    return true;
}

void ReleasePage(ExecutablePage* page)
{
    if (page->base != nullptr)
    {
        platform::ReleaseMemory(page->base, page->size);
        page->base = nullptr;
    }
}

bool WriteAndArm(const ExecutablePage& page,
                 const std::vector<std::uint8_t>& code)
{
    if (page.base == nullptr || code.size() > page.size)
    {
        return false;
    }
    std::memcpy(page.base, code.data(), code.size());
    return MakeExecutable(page.base, page.size);
}

bool Lower(const std::vector<std::uint8_t>& guest,
           std::vector<std::uint8_t>* lowered,
           const GuestCodeDefaultOperandSize mode =
               GuestCodeDefaultOperandSize::k32)
{
    std::uint8_t buffer[kMaxLoweredBytes] = {};
    std::size_t produced = 0U;
    if (!LowerLongModeBytes(guest.data(), guest.size(), buffer, &produced,
                            nullptr, mode))
    {
        return false;
    }
    lowered->assign(buffer, buffer + produced);
    return true;
}

// 1. The ordinary base-register form.
//
// `mov eax, [ebx]` lowered to `67 8B 03`, called with a base register whose
// upper half is deliberately filled with rubbish. If the prefix did nothing the
// instruction would address through the whole 64-bit register and touch an
// address nothing has mapped; that it returns the marker is the prefix doing
// what the guest's 32-bit arithmetic did.
bool ProbeAddressSizePrefix(const std::uint32_t* data)
{
    const std::vector<std::uint8_t> guest = {0x8BU, 0x03U};
    std::vector<std::uint8_t> lowered;
    if (!Lower(guest, &lowered) || lowered.size() != 3U ||
        lowered[0] != 0x67U)
    {
        std::cout << "long_mode_lowering_prefix=false,reason=lowering\n";
        return false;
    }

    // push rbx / mov rbx, rdi / <lowered> / pop rbx / ret.
    // RBX is callee-saved under SysV AMD64, so it is preserved around the test
    // rather than simply overwritten.
    std::vector<std::uint8_t> code = {0x53U, 0x48U, 0x89U, 0xFBU};
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.push_back(0x5BU);
    code.push_back(0xC3U);

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_prefix=false,reason=page\n";
        return false;
    }

    using Entry = std::uint32_t (*)(std::uint64_t);
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));

    // The rubbish half is what the prefix has to discard.
    const std::uint64_t base_register =
        (UINT64_C(0xDEADBEEF) << 32) |
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(data) & 0xFFFFFFFFU);
    const std::uint32_t observed = entry(base_register);
    ReleasePage(&page);

    const bool ok = observed == kMarker;
    std::cout << "long_mode_lowering_prefix=" << (ok ? "true" : "false")
              << ",observed=0x" << std::hex << observed << std::dec << "\n";
    return ok;
}

// 3. Task 565. The moffs forms, read and written.
//
// `A1 disp32` is `mov eax, [disp32]` in five bytes and `mov eax, moffs64` in
// nine in long mode, so the instruction's own length changes and every decode
// after it moves. The re-encoding sends it to the same SIB absolute form
// `kAbsoluteToSib` produces.
//
// Both directions are run. A read alone would not show that the store form
// reaches the same address, and `A3` is the one that was blocking the entry
// chain -- as `66 A3 disp32`, the operand-size variant.
bool ProbeMoffs(std::uint32_t* data)
{
    const auto address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(data));
    const auto with_address = [address](std::vector<std::uint8_t> head) {
        for (int shift = 0; shift < 32; shift += 8)
        {
            head.push_back(
                static_cast<std::uint8_t>((address >> shift) & 0xFFU));
        }
        return head;
    };

    // mov eax, [addr] · ret
    std::vector<std::uint8_t> load_lowered;
    if (!Lower(with_address({0xA1U}), &load_lowered) ||
        load_lowered.size() != 8U || load_lowered[0] != 0x67U ||
        load_lowered[1] != 0x8BU)
    {
        std::cout << "long_mode_lowering_moffs=false,reason=load_lowering\n";
        return false;
    }
    std::vector<std::uint8_t> load_code = load_lowered;
    load_code.push_back(0xC3U);

    ExecutablePage load_page;
    if (!AllocateCodePage(&load_page) || !WriteAndArm(load_page, load_code))
    {
        ReleasePage(&load_page);
        std::cout << "long_mode_lowering_moffs=false,reason=page\n";
        return false;
    }
    using Load = std::uint32_t (*)();
    Load load = nullptr;
    std::memcpy(&load, &load_page.base, sizeof(load));
    *data = kMarker;
    const std::uint32_t observed = load();
    ReleasePage(&load_page);

    // mov [addr], eax, with the value arriving in the first argument so the
    // store has something of its own to write.
    std::vector<std::uint8_t> store_lowered;
    if (!Lower(with_address({0xA3U}), &store_lowered) ||
        store_lowered.size() != 8U || store_lowered[1] != 0x89U)
    {
        std::cout << "long_mode_lowering_moffs=false,reason=store_lowering\n";
        return false;
    }
    // mov eax, edi (SysV first argument) · <lowered store> · ret
    std::vector<std::uint8_t> store_code = {0x89U, 0xF8U};
    store_code.insert(store_code.end(), store_lowered.begin(),
                      store_lowered.end());
    store_code.push_back(0xC3U);

    ExecutablePage store_page;
    if (!AllocateCodePage(&store_page) || !WriteAndArm(store_page, store_code))
    {
        ReleasePage(&store_page);
        std::cout << "long_mode_lowering_moffs=false,reason=store_page\n";
        return false;
    }
    using Store = void (*)(std::uint32_t);
    Store store = nullptr;
    std::memcpy(&store, &store_page.base, sizeof(store));
    constexpr std::uint32_t kStored = 0x1234ABCDU;
    *data = 0U;
    store(kStored);
    const std::uint32_t written = *data;
    ReleasePage(&store_page);

    const bool ok = observed == kMarker && written == kStored;
    std::cout << "long_mode_lowering_moffs=" << (ok ? "true" : "false")
              << ",read=0x" << std::hex << observed << ",wrote=0x" << written
              << std::dec << "\n";
    return ok;
}

// 2. The absolute form, and the reason it needs more than a prefix.
//
// `mov eax, [disp32]` is absolute in 32-bit mode and RIP-relative in long mode,
// so the same bytes read a different byte depending on where they were placed.
// The lowering rewrites ModRM into the SIB absolute encoding, which has no such
// dependence -- and the way to show that is to run the same lowered bytes from
// two pages at two different addresses and get the same answer.
//
// The un-lowered form is deliberately not executed. Its whole problem is that
// what it reads depends on where it sits, which on this host means an address
// nothing has mapped; demonstrating that by faulting inside a probe would prove
// the point at the cost of the run.
bool ProbeAbsoluteToSib(const std::uint32_t* data)
{
    const auto address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(data));
    std::vector<std::uint8_t> guest = {0x8BU, 0x05U};
    for (int shift = 0; shift < 32; shift += 8)
    {
        guest.push_back(static_cast<std::uint8_t>((address >> shift) & 0xFFU));
    }

    std::vector<std::uint8_t> lowered;
    if (!Lower(guest, &lowered) || lowered.size() != 8U ||
        lowered[0] != 0x67U || lowered[2] != 0x04U || lowered[3] != 0x25U)
    {
        std::cout << "long_mode_lowering_absolute=false,reason=lowering\n";
        return false;
    }

    std::vector<std::uint8_t> code = lowered;
    code.push_back(0xC3U);

    std::uint32_t observed[2] = {0U, 0U};
    std::uintptr_t placed[2] = {0U, 0U};
    ExecutablePage pages[2];
    bool prepared = true;
    for (int index = 0; index < 2; ++index)
    {
        prepared = prepared && AllocateCodePage(&pages[index]) &&
            WriteAndArm(pages[index], code);
    }
    if (prepared)
    {
        for (int index = 0; index < 2; ++index)
        {
            using Entry = std::uint32_t (*)();
            Entry entry = nullptr;
            std::memcpy(&entry, &pages[index].base, sizeof(entry));
            placed[index] =
                reinterpret_cast<std::uintptr_t>(pages[index].base);
            observed[index] = entry();
        }
    }
    for (int index = 0; index < 2; ++index)
    {
        ReleasePage(&pages[index]);
    }
    if (!prepared)
    {
        std::cout << "long_mode_lowering_absolute=false,reason=page\n";
        return false;
    }

    const bool read_marker =
        observed[0] == kMarker && observed[1] == kMarker;
    // Two different addresses is what makes the agreement mean something. If
    // the host handed back the same page twice the comparison would be empty.
    const bool distinct_pages = placed[0] != placed[1];
    const bool ok = read_marker && distinct_pages;
    std::cout << "long_mode_lowering_absolute=" << (ok ? "true" : "false")
              << ",distinct_pages=" << (distinct_pages ? 1 : 0)
              << ",observed=0x" << std::hex << observed[0] << ",0x"
              << observed[1] << std::dec << "\n";
    return ok;
}

// 2b. The absolute form with an immediate after it (Task 572).
//
// `cmp byte ptr [disp32], imm8` is the shape that stopped the reachable chain at
// `0x10fc2fa`, and the shape 865 of the census's 1,609 refusals share. Its
// displacement is not the last field, which the previous width condition
// required.
//
// Two things have to be shown and they are different claims. That the *address*
// is absolute is what `ProbeAbsoluteToSib` shows for this family already. What
// is new here is that the **immediate survives the SIB insertion**, and an
// immediate that were dropped or misread would still produce a running
// instruction reading a valid address -- it would just compare against the
// wrong number. So the immediate is pinned by its effect: the same instruction
// is run twice against the same byte, once with an immediate equal to it and
// once with one that differs, and the two runs must disagree in ZF.
//
// Comparing against only one immediate would not do. A lowering that lost the
// immediate entirely and compared against zero would still answer "not equal"
// and pass a single unequal case.
bool ProbeAbsoluteToSibImmediate(const std::uint32_t* data)
{
    const auto address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(data));
    // The low byte of the marker, which is what a byte-sized compare against
    // this address reads.
    const auto marker_byte = static_cast<std::uint8_t>(kMarker & 0xFFU);
    const auto other_byte = static_cast<std::uint8_t>(marker_byte ^ 0xFFU);

    const auto build = [address](const std::uint8_t immediate) {
        std::vector<std::uint8_t> guest = {0x80U, 0x3DU};  // cmp byte [abs], ib
        for (int shift = 0; shift < 32; shift += 8)
        {
            guest.push_back(
                static_cast<std::uint8_t>((address >> shift) & 0xFFU));
        }
        guest.push_back(immediate);
        return guest;
    };

    // The exact bytes, not just the length. `67 80 3C 25 <disp32> <imm8>`: the
    // ModRM keeps `reg=111` and takes `rm=100`, the SIB is the no-base no-index
    // form, and the displacement and immediate are copied unchanged.
    const std::vector<std::uint8_t> equal_guest = build(marker_byte);
    std::vector<std::uint8_t> equal_lowered;
    std::vector<std::uint8_t> other_lowered;
    std::vector<std::uint8_t> expected = {0x67U, 0x80U, 0x3CU, 0x25U};
    for (int shift = 0; shift < 32; shift += 8)
    {
        expected.push_back(static_cast<std::uint8_t>((address >> shift) & 0xFFU));
    }
    expected.push_back(marker_byte);
    if (!Lower(equal_guest, &equal_lowered) ||
        !Lower(build(other_byte), &other_lowered) ||
        equal_lowered != expected)
    {
        std::cout << "long_mode_lowering_absolute_imm=false,reason=lowering\n";
        return false;
    }

    // `sete al` then `movzx eax, al`, so the observed value is ZF itself.
    // Nothing between the compare and the `sete` touches flags.
    const std::uint8_t tail[] = {0x0FU, 0x94U, 0xC0U,   // sete al
                                 0x0FU, 0xB6U, 0xC0U,   // movzx eax, al
                                 0xC3U};                // ret
    std::uint32_t observed[2] = {0xFFFFFFFFU, 0xFFFFFFFFU};
    const std::vector<std::uint8_t>* variants[2] = {&equal_lowered,
                                                    &other_lowered};
    ExecutablePage pages[2];
    bool prepared = true;
    for (int index = 0; index < 2; ++index)
    {
        std::vector<std::uint8_t> code = *variants[index];
        code.insert(code.end(), tail, tail + sizeof(tail));
        prepared = prepared && AllocateCodePage(&pages[index]) &&
            WriteAndArm(pages[index], code);
    }
    if (prepared)
    {
        for (int index = 0; index < 2; ++index)
        {
            using Entry = std::uint32_t (*)();
            Entry entry = nullptr;
            std::memcpy(&entry, &pages[index].base, sizeof(entry));
            observed[index] = entry();
        }
    }
    for (int index = 0; index < 2; ++index)
    {
        ReleasePage(&pages[index]);
    }
    if (!prepared)
    {
        std::cout << "long_mode_lowering_absolute_imm=false,reason=page\n";
        return false;
    }

    const bool ok = observed[0] == 1U && observed[1] == 0U;
    std::cout << "long_mode_lowering_absolute_imm=" << (ok ? "true" : "false")
              << ",zf_equal=" << observed[0] << ",zf_other=" << observed[1]
              << "\n";
    return ok;
}

// 2c. What the width condition was protecting, kept after replacing it.
//
// `IsAbsoluteDisplacementForm` reads only ModRM's `mod` and `rm`. Under a `0x67`
// prefix the guest addresses in 16 bits, where `mod=00 rm=101` is `[DI]` --
// not absolute, and carrying no displacement at all. The classifier still names
// `kAbsoluteToSib` for it, so the rewriter is the only thing standing between
// that form and a lowering that would invent a displacement out of whatever
// followed.
//
// Task 572 replaced the arithmetic that used to reject this as a side effect, so
// the refusal is asserted here rather than left to coincidence.
bool ProbeAbsoluteRefusals()
{
    // 67 8B 05 -> mov eax, [di] in 32-bit mode: mod=00 rm=101, no displacement.
    const std::uint8_t sixteen_bit_addressing[] = {0x67U, 0x8BU, 0x05U};
    const auto verdict = ClassifyLongModeBytes(sixteen_bit_addressing,
                                               sizeof(sixteen_bit_addressing));
    const bool classified_absolute =
        verdict.lowering == LongModeLowering::kAbsoluteToSib;

    std::uint8_t buffer[kMaxLoweredBytes] = {};
    std::size_t produced = 0U;
    const bool refused = !LowerLongModeBytes(sixteen_bit_addressing,
                                             sizeof(sixteen_bit_addressing),
                                             buffer, &produced);

    const bool ok = classified_absolute && refused;
    std::cout << "long_mode_lowering_absolute_refusals="
              << (ok ? "true" : "false")
              << ",classified_absolute=" << (classified_absolute ? 1 : 0)
              << ",refused=" << (refused ? 1 : 0) << "\n";
    return ok;
}

// 2d. The `ESP` re-encode on a two-byte opcode (Task 574).
//
// `movzx esi, byte ptr [esp+8]` is `0F B6` with `ESP` as the SIB base, and it
// was refused because the lowering located the opcode as the byte before ModRM
// -- true only of a one-byte opcode.
//
// The bytes are compared exactly rather than by length, because the failure this
// guards against has the right length. A REX inserted one byte late gives
// `0F 41 B6 ...`, which is a *different instruction* long mode decodes and runs
// without raising: the `41` becomes a prefix on `B6`, and `0F` picks up
// whatever follows. Only the byte string tells the two apart.
bool ProbeStackPointerTwoByteOpcode()
{
    // movzx esi, byte ptr [esp+8]  ->  REX.B before the whole opcode, and the
    // SIB base from `100` (ESP) to `111`. Naming R15 takes both: the REX bit
    // supplies the high bit and the field the low three, so a check that
    // expected the SIB byte to survive unchanged would be checking for a
    // lowering that still addressed through host RSP.
    const std::vector<std::uint8_t> two_byte = {0x0FU, 0xB6U, 0x74U, 0x24U,
                                                0x08U};
    const std::vector<std::uint8_t> two_byte_expected = {
        0x41U, 0x0FU, 0xB6U, 0x74U, 0x27U, 0x08U};
    // mov eax, [esp+8], the one-byte form Task 564 already handled. Kept here
    // so the change to how the opcode is located is shown not to move it.
    const std::vector<std::uint8_t> one_byte = {0x8BU, 0x44U, 0x24U, 0x08U};
    const std::vector<std::uint8_t> one_byte_expected = {0x41U, 0x8BU, 0x44U,
                                                         0x27U, 0x08U};

    std::vector<std::uint8_t> two_byte_lowered;
    std::vector<std::uint8_t> one_byte_lowered;
    const bool two_byte_ok = Lower(two_byte, &two_byte_lowered) &&
        two_byte_lowered == two_byte_expected;
    const bool one_byte_ok = Lower(one_byte, &one_byte_lowered) &&
        one_byte_lowered == one_byte_expected;

    const auto hex = [](const std::vector<std::uint8_t>& bytes) {
        std::string text;
        for (const std::uint8_t byte : bytes)
        {
            const char digits[] = "0123456789abcdef";
            text += digits[(byte >> 4U) & 0x0FU];
            text += digits[byte & 0x0FU];
        }
        return text;
    };

    const bool ok = two_byte_ok && one_byte_ok;
    std::cout << "long_mode_lowering_two_byte_esp=" << (ok ? "true" : "false")
              << ",two_byte=" << hex(two_byte_lowered)
              << ",one_byte=" << hex(one_byte_lowered) << "\n";
    return ok;
}

// Task 680. `BC iw` in a 16-bit code object writes SP, not ESP. The lowering
// must therefore select R15W and preserve the upper bits of the guest state.
bool Probe16BitStackPointerImmediate()
{
    const std::vector<std::uint8_t> guest = {0xBCU, 0x00U, 0x20U};
    const std::vector<std::uint8_t> expected = {
        0x66U, 0x41U, 0xBFU, 0x00U, 0x20U};
    const auto verdict = ClassifyLongModeBytes(
        guest.data(), guest.size(), GuestCodeDefaultOperandSize::k16);
    std::uint8_t lowered_buffer[kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    std::size_t lowered_instructions = 0U;
    const bool lowered_ok = LowerLongModeBytes(
        guest.data(), guest.size(), lowered_buffer, &lowered_count,
        &lowered_instructions, GuestCodeDefaultOperandSize::k16);
    const std::vector<std::uint8_t> lowered(
        lowered_buffer, lowered_buffer + lowered_count);
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering !=
            LongModeLowering::k16BitStackPointerImmediateToR15 ||
        !lowered_ok || lowered != expected || lowered_instructions != 1U)
    {
        std::cout << "long_mode_lowering_16bit_stack_pointer=false,"
                     "reason=bytes\n";
        return false;
    }

    // Save the callee-saved R15, seed it with a value whose upper word is
    // visible, execute the lowered bytes, and return the resulting R15 value.
    std::vector<std::uint8_t> code = {
        0x41U, 0x57U,  // push r15
        0x49U, 0xBFU,  // mov r15, imm64
        0x01U, 0x00U, 0xCDU, 0xABU, 0x78U, 0x56U, 0x34U, 0x12U,
    };
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.insert(code.end(), {
        0x4CU, 0x89U, 0xF8U,  // mov rax, r15
        0x41U, 0x5FU,          // pop r15
        0xC3U,                 // ret
    });

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_16bit_stack_pointer=false,"
                     "reason=page\n";
        return false;
    }
    using Entry = std::uint64_t (*)();
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));
    const std::uint64_t observed = entry();
    ReleasePage(&page);

    const bool ok = observed == UINT64_C(0x12345678ABCD2000);
    std::cout << "long_mode_lowering_16bit_stack_pointer="
              << (ok ? "true" : "false") << ",observed=0x" << std::hex
              << observed << std::dec << "\n";
    return ok;
}

// Task 681. The first real object-3 boundary after MOV SP is a mode16 LEA with
// both address- and operand-size overrides. Execute the generic lowering and
// check the destination without reading memory; LEA is an address computation.
bool Probe16BitLea32()
{
    const std::vector<std::uint8_t> guest = {
        0x67U, 0x66U, 0x8DU, 0x8CU, 0x24U,
        0x00U, 0xE0U, 0xFFU, 0xFFU,
    };
    const std::vector<std::uint8_t> expected = {
        0x67U, 0x41U, 0x8DU, 0x8CU, 0x27U,
        0x00U, 0xE0U, 0xFFU, 0xFFU,
    };
    std::vector<std::uint8_t> lowered;
    const auto verdict = ClassifyLongModeBytes(
        guest.data(), guest.size(), GuestCodeDefaultOperandSize::k16);
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering != LongModeLowering::k16BitLea32ToGuestGprs ||
        !Lower(guest, &lowered, GuestCodeDefaultOperandSize::k16) ||
        lowered != expected)
    {
        std::cout << "long_mode_lowering_16bit_lea32=false,reason=bytes\n";
        return false;
    }

    std::vector<std::uint8_t> code = {
        0x41U, 0x57U,                         // push r15
        0x41U, 0xBFU, 0x00U, 0x20U, 0x00U, 0x30U, // mov r15d,0x30002000
    };
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.insert(code.end(), {
        0x89U, 0xC8U,  // mov eax, ecx
        0x41U, 0x5FU,  // pop r15
        0xC3U,         // ret
    });

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_16bit_lea32=false,reason=page\n";
        return false;
    }
    using Entry = std::uint32_t (*)();
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));
    const std::uint32_t observed = entry();
    ReleasePage(&page);

    const bool ok = observed == 0x30000000U;
    std::cout << "long_mode_lowering_16bit_lea32="
              << (ok ? "true" : "false") << ",observed=0x" << std::hex
              << observed << std::dec << "\n";
    return ok;
}

// Task 682. Lower the runtime frontier `LEA CX,[SI+disp16]`. The sequence
// must mask the source to its low word, preserve the destination upper word,
// wrap the effective address at 16 bits, and leave ZF unchanged.
bool Probe16BitLea16()
{
    const std::vector<std::uint8_t> guest = {
        0x8DU, 0x8CU, 0x24U, 0x00U,
    };
    const std::vector<std::uint8_t> expected = {
        0x44U, 0x0FU, 0xB7U, 0xF6U,
        0x67U, 0x66U, 0x41U, 0x8DU, 0x8EU,
        0x24U, 0x00U, 0x00U, 0x00U,
    };
    const auto verdict = ClassifyLongModeBytes(
        guest.data(), guest.size(), GuestCodeDefaultOperandSize::k16);
    std::vector<std::uint8_t> lowered;
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering != LongModeLowering::k16BitLea16ToGuestGprs ||
        !Lower(guest, &lowered, GuestCodeDefaultOperandSize::k16) ||
        lowered != expected)
    {
        std::cout << "long_mode_lowering_16bit_lea16=false,reason=bytes\n";
        return false;
    }

    std::vector<std::uint8_t> code = {
        0x41U, 0x56U,                         // push r14
        0x56U,                                // push rsi
        0x48U, 0x89U, 0xFEU,                 // mov rsi,rdi
        0xB9U, 0x00U, 0x00U, 0x34U, 0x12U,  // mov ecx,0x12340000
        0x31U, 0xC0U,                         // xor eax,eax (ZF=1)
    };
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.insert(code.end(), {
        0x0FU, 0x94U, 0xC2U,                 // sete dl
        0x0FU, 0xB6U, 0xD2U,                 // movzx edx,dl
        0x48U, 0xC1U, 0xE2U, 0x20U,          // shl rdx,32
        0x89U, 0xC8U,                         // mov eax,ecx
        0x48U, 0x09U, 0xD0U,                 // or rax,rdx
        0x5EU,                                // pop rsi
        0x41U, 0x5EU,                         // pop r14
        0xC3U,                                // ret
    });

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_16bit_lea16=false,reason=page\n";
        return false;
    }
    using Entry = std::uint64_t (*)(std::uint64_t);
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));
    const std::uint64_t observed = entry(UINT64_C(0xFFFF000000000010));
    ReleasePage(&page);

    const bool ok = observed == UINT64_C(0x112340034);
    std::cout << "long_mode_lowering_16bit_lea16="
              << (ok ? "true" : "false") << ",observed=0x" << std::hex
              << observed << std::dec << "\n";
    return ok;
}

// Task 685. Remove the mode16 operand-size override from register TEST, then
// execute the result to check flags and the untouched upper register state.
bool Probe16BitTest32()
{
    const std::vector<std::uint8_t> guest = {
        0x66U, 0x85U, 0xFFU,  // test edi,edi in mode16
    };
    const std::vector<std::uint8_t> expected = {0x85U, 0xFFU};
    const auto verdict = ClassifyLongModeBytes(
        guest.data(), guest.size(), GuestCodeDefaultOperandSize::k16);
    std::vector<std::uint8_t> lowered;
    std::uint8_t lowered_buffer[kMaxLoweredBytes] = {};
    std::size_t lowered_count = 0U;
    std::size_t lowered_instructions = 0U;
    const bool lowered_ok = LowerLongModeBytes(
        guest.data(), guest.size(), lowered_buffer, &lowered_count,
        &lowered_instructions, GuestCodeDefaultOperandSize::k16);
    if (lowered_ok)
    {
        lowered.assign(lowered_buffer, lowered_buffer + lowered_count);
    }
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering != LongModeLowering::k16BitTest32ToGuestGprs ||
        !lowered_ok || lowered != expected || lowered_instructions != 1U)
    {
        std::cout << "long_mode_lowering_16bit_test32=false,reason=bytes\n";
        return false;
    }

    // The function receives its test operand in RDI. Capture EFLAGS before
    // any result-building instruction changes them; the low result word keeps
    // the guest operand so the TEST destination register can be checked too.
    std::vector<std::uint8_t> flags_code = lowered;
    flags_code.insert(flags_code.end(), {
        0x9CU,                    // pushfq
        0x58U,                    // pop rax
        0x48U, 0x89U, 0xC2U,      // mov rdx,rax
        0x89U, 0xF8U,              // mov eax,edi
        0x48U, 0xC1U, 0xE2U, 0x20U,  // shl rdx,32
        0x48U, 0x09U, 0xD0U,      // or rax,rdx
        0xC3U,                    // ret
    });
    ExecutablePage flags_page;
    if (!AllocateCodePage(&flags_page) || !WriteAndArm(flags_page, flags_code))
    {
        ReleasePage(&flags_page);
        std::cout << "long_mode_lowering_16bit_test32=false,reason=flags_page\n";
        return false;
    }
    using FlagsEntry = std::uint64_t (*)(std::uint64_t);
    FlagsEntry flags_entry = nullptr;
    std::memcpy(&flags_entry, &flags_page.base, sizeof(flags_entry));
    const std::uint64_t zero_result = flags_entry(0U);
    const std::uint64_t nonzero_result = flags_entry(0x80000001U);
    ReleasePage(&flags_page);

    constexpr std::uint64_t kFlagsMask =
        UINT64_C(0x1) | UINT64_C(0x40) | UINT64_C(0x800);
    const std::uint64_t zero_flags = zero_result >> 32U;
    const std::uint64_t nonzero_flags = nonzero_result >> 32U;
    const bool flags_ok =
        static_cast<std::uint32_t>(zero_result) == 0U &&
        (zero_flags & kFlagsMask) == UINT64_C(0x40) &&
        static_cast<std::uint32_t>(nonzero_result) == 0x80000001U &&
        (nonzero_flags & kFlagsMask) == 0U;

    // A second invocation returns the complete RDI value, proving that the
    // 32-bit TEST did not accidentally narrow or rewrite the guest register.
    constexpr std::uint64_t kRegisterValue = UINT64_C(0xA5A5A5A512340000);
    std::vector<std::uint8_t> register_code = {
        0x48U, 0xBFU,  // mov rdi, imm64
    };
    for (std::size_t index = 0U; index < 8U; ++index)
    {
        register_code.push_back(static_cast<std::uint8_t>(
            (kRegisterValue >> (index * 8U)) & 0xFFU));
    }
    register_code.insert(register_code.end(), lowered.begin(), lowered.end());
    register_code.insert(register_code.end(), {
        0x48U, 0x89U, 0xF8U,  // mov rax,rdi
        0xC3U,                // ret
    });
    ExecutablePage register_page;
    if (!AllocateCodePage(&register_page) ||
        !WriteAndArm(register_page, register_code))
    {
        ReleasePage(&register_page);
        std::cout << "long_mode_lowering_16bit_test32=false,reason=register_page\n";
        return false;
    }
    using RegisterEntry = std::uint64_t (*)();
    RegisterEntry register_entry = nullptr;
    std::memcpy(&register_entry, &register_page.base,
                sizeof(register_entry));
    const std::uint64_t register_result = register_entry();
    ReleasePage(&register_page);

    const bool ok = flags_ok && register_result == kRegisterValue;
    std::cout << "long_mode_lowering_16bit_test32="
              << (ok ? "true" : "false")
              << ",flags=" << (flags_ok ? "true" : "false")
              << ",register="
              << (register_result == kRegisterValue ? "true" : "false")
              << "\n";
    return ok;
}

// Task 687. Prefix-free mode16 B8+r iw needs only an operand-size prefix in
// long mode. Execute it after seeding RAX to prove that the low word changes
// while the upper register bits remain intact.
bool Probe16BitMovImmediate()
{
    const std::vector<std::uint8_t> guest = {
        0xB8U, 0x07U, 0x00U,  // mov ax,7 in mode16
    };
    const std::vector<std::uint8_t> expected = {
        0x66U, 0xB8U, 0x07U, 0x00U,
    };
    const auto verdict = ClassifyLongModeBytes(
        guest.data(), guest.size(), GuestCodeDefaultOperandSize::k16);
    std::vector<std::uint8_t> lowered;
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering != LongModeLowering::k16BitMovImmediateToGuestGprs ||
        !Lower(guest, &lowered, GuestCodeDefaultOperandSize::k16) ||
        lowered != expected)
    {
        std::cout << "long_mode_lowering_16bit_mov_immediate=false,"
                     "reason=bytes\n";
        return false;
    }

    constexpr std::uint64_t kSeed = UINT64_C(0xA5A5A5A512340000);
    std::vector<std::uint8_t> code = {0x48U, 0xB8U};
    for (std::size_t index = 0U; index < 8U; ++index)
    {
        code.push_back(static_cast<std::uint8_t>(
            (kSeed >> (index * 8U)) & 0xFFU));
    }
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.insert(code.end(), {0xC3U});  // ret

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_16bit_mov_immediate=false,"
                     "reason=page\n";
        return false;
    }
    using Entry = std::uint64_t (*)();
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));
    const std::uint64_t observed = entry();
    ReleasePage(&page);

    const std::uint64_t expected_value = UINT64_C(0xA5A5A5A512340007);
    const bool ok = observed == expected_value;
    std::cout << "long_mode_lowering_16bit_mov_immediate="
              << (ok ? "true" : "false") << ",observed=0x" << std::hex
              << observed << std::dec << "\n";
    return ok;
}

// Task 688. Execute a mode16 register-register word MOV after seeding the
// destination with visible upper bits. The 66-prefixed form must change only
// the destination low word.
bool Probe16BitMovRegister()
{
    const std::vector<std::uint8_t> guest = {
        0x89U, 0xCAU,  // mov dx,cx in mode16
    };
    const std::vector<std::uint8_t> expected = {
        0x66U, 0x89U, 0xCAU,
    };
    const auto verdict = ClassifyLongModeBytes(
        guest.data(), guest.size(), GuestCodeDefaultOperandSize::k16);
    std::vector<std::uint8_t> lowered;
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering != LongModeLowering::k16BitMovRegisterToGuestGprs ||
        !Lower(guest, &lowered, GuestCodeDefaultOperandSize::k16) ||
        lowered != expected)
    {
        std::cout << "long_mode_lowering_16bit_mov_register=false,"
                     "reason=bytes\n";
        return false;
    }

    constexpr std::uint64_t kDestination = UINT64_C(0xA5A5A5A512340000);
    constexpr std::uint64_t kSource = UINT64_C(0x5A5A5A5A00000007);
    std::vector<std::uint8_t> code = {0x48U, 0xBAU};
    for (std::size_t index = 0U; index < 8U; ++index)
    {
        code.push_back(static_cast<std::uint8_t>(
            (kDestination >> (index * 8U)) & 0xFFU));
    }
    code.push_back(0x48U);
    code.push_back(0xB9U);
    for (std::size_t index = 0U; index < 8U; ++index)
    {
        code.push_back(static_cast<std::uint8_t>(
            (kSource >> (index * 8U)) & 0xFFU));
    }
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.insert(code.end(), {
        0x48U, 0x89U, 0xD0U,  // mov rax,rdx
        0xC3U,                 // ret
    });

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_16bit_mov_register=false,"
                     "reason=page\n";
        return false;
    }
    using Entry = std::uint64_t (*)();
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));
    const std::uint64_t observed = entry();
    ReleasePage(&page);

    const std::uint64_t expected_value = UINT64_C(0xA5A5A5A512340007);
    const bool ok = observed == expected_value;
    std::cout << "long_mode_lowering_16bit_mov_register="
              << (ok ? "true" : "false") << ",observed=0x" << std::hex
              << observed << std::dec << "\n";
    return ok;
}

// 2e. A REX changes AH/CH/DH/BH into SPL/BPL/SIL/DIL (Task 614). The
// high-byte source is materialised in R14B by exchanging the source low and
// high bytes around a REX-using move, and the original byte operation is then
// re-encoded against guest ESP in R15D.
bool ProbeStackPointerHighByteSource(const std::uint32_t* data)
{
    const std::vector<std::uint8_t> guest = {
        0x88U, 0x24U, 0x24U,  // mov byte ptr [esp], ah
    };
    const std::vector<std::uint8_t> expected = {
        0x86U, 0xC4U,          // xchg al,ah
        0x41U, 0x88U, 0xC6U,  // mov r14b,al
        0x86U, 0xC4U,          // xchg al,ah
        0x45U, 0x88U, 0x34U, 0x27U,  // mov [r15],r14b
    };
    std::vector<std::uint8_t> lowered;
    const auto verdict = ClassifyLongModeBytes(guest.data(), guest.size());
    if (verdict.compatibility != LongModeByteCompatibility::kNeedsReencode ||
        verdict.lowering != LongModeLowering::kStackPointerHighByteToR15 ||
        !Lower(guest, &lowered) || lowered != expected)
    {
        std::cout << "long_mode_lowering_high_byte=false,reason=bytes\n";
        return false;
    }

    // Task 676. A destination high byte now has a common lowering when the
    // memory source is based on ESP. It uses DL around the R15-based load,
    // because a REX prefix cannot name AH/CH/DH/BH.
    const std::uint8_t destination[] = {0x8AU, 0x24U, 0x24U};
    const std::uint8_t exchange[] = {0x86U, 0x24U, 0x24U};
    const auto destination_verdict = ClassifyLongModeBytes(
        destination, sizeof(destination));
    const auto exchange_verdict = ClassifyLongModeBytes(exchange,
                                                        sizeof(exchange));
    const std::uint8_t destination_expected[] = {
        0x44U, 0x88U, 0xF2U,
        0x41U, 0x8AU, 0x14U, 0x27U,
        0x8AU, 0xE2U,
        0x44U, 0x88U, 0xF2U};
    std::uint8_t destination_lowered[kMaxLoweredBytes] = {};
    std::size_t destination_count = 0U;
    const bool destination_lowered_ok =
        destination_verdict.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        destination_verdict.lowering ==
            LongModeLowering::kStackPointerHighByteDestinationToR15 &&
        LowerLongModeBytes(destination, sizeof(destination),
                            destination_lowered, &destination_count) &&
        destination_count == sizeof(destination_expected) &&
        std::memcmp(destination_lowered, destination_expected,
                    sizeof(destination_expected)) == 0;
    const bool refusals =
        destination_lowered_ok &&
        exchange_verdict.compatibility ==
            LongModeByteCompatibility::kUnsupported &&
        exchange_verdict.lowering == LongModeLowering::kNone;
    if (!refusals)
    {
        std::cout << "long_mode_lowering_high_byte=false,reason=refusal\n";
        return false;
    }

    // push r14; push r15; mov r15d,edi; mov eax,0xA1B2C3D4;
    // cmp ecx,ecx; <lowered>; sete byte [r15+4]; mov eax,[r15];
    // pop r15; pop r14; ret.
    //
    // The data page is below 4 GiB, so the uint32 argument is exactly the
    // guest ESP value. The byte at +4 records ZF after the lowered MOV; MOV
    // itself does not change flags, and neither do the scratch instructions.
    std::vector<std::uint8_t> code = {
        0x41U, 0x56U,                         // push r14
        0x41U, 0x57U,                         // push r15
        0x41U, 0x89U, 0xFFU,                 // mov r15d,edi
        0xB8U, 0xD4U, 0xC3U, 0xB2U, 0xA1U,  // mov eax,imm32
        0x39U, 0xC9U,                         // cmp ecx,ecx (ZF=1)
    };
    code.insert(code.end(), lowered.begin(), lowered.end());
    code.insert(code.end(), {
        0x41U, 0x0FU, 0x94U, 0x4FU, 0x04U,  // sete byte [r15+4]
        0x41U, 0x8BU, 0x07U,                // mov eax,[r15]
        0x41U, 0x5FU,                        // pop r15
        0x41U, 0x5EU,                        // pop r14
        0xC3U,                               // ret
    });

    auto* const bytes = const_cast<std::uint8_t*>(
        reinterpret_cast<const std::uint8_t*>(data));
    bytes[0] = 0U;
    bytes[1] = 0U;
    bytes[2] = 0U;
    bytes[3] = 0U;
    bytes[4] = 0U;

    ExecutablePage page;
    if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
    {
        ReleasePage(&page);
        std::cout << "long_mode_lowering_high_byte=false,reason=page\n";
        return false;
    }
    using Entry = std::uint32_t (*)(std::uint32_t);
    Entry entry = nullptr;
    std::memcpy(&entry, &page.base, sizeof(entry));
    const std::uint32_t observed = entry(static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(data)));
    ReleasePage(&page);

    const unsigned zf = static_cast<unsigned>(bytes[4]);
    const bool ok = observed == 0x000000C3U && zf == 1U;
    *const_cast<std::uint32_t*>(data) = kMarker;
    std::cout << "long_mode_lowering_high_byte=" << (ok ? "true" : "false")
              << ",stored=0x" << std::hex << observed << std::dec
              << ",zf=" << zf << "\n";
    return ok;
}

// Task 683. Execute the semantic body of the mode16 LOOPNZ cache slot. The
// direct E9 target is patched to a local label here because this lower-level
// probe has no translation-plan edge metadata; the emission probe checks that
// the real cache uses those fixups.
bool Probe16BitLoopNz()
{
    struct Case
    {
        std::uint16_t initial_cx;
        bool initial_zf;
        std::uint64_t expected;
    };
    bool ok = true;
    for (const Case& item : {
             Case{2U, false, UINT64_C(0x100000001)},
             Case{1U, false, UINT64_C(0x100000000)},
             Case{2U, true, UINT64_C(0x000000001)},
         })
    {
        std::vector<std::uint8_t> code = {
            0x41U, 0x56U,  // push r14
            0xB9U,
            static_cast<std::uint8_t>(item.initial_cx), 0x00U, 0x00U, 0x00U,
        };
        if (item.initial_zf)
        {
            code.insert(code.end(), {0x31U, 0xC0U});  // xor eax,eax
        }
        else
        {
            code.insert(code.end(),
                        {0xB8U, 0x01U, 0x00U, 0x00U, 0x00U,
                         0x85U, 0xC0U});  // mov eax,1; test eax,eax
        }

        const std::size_t branch_opcode_offset = code.size() + 21U;
        code.insert(code.end(), {
            0x9CU,                         // pushfq
            0x66U, 0xFFU, 0xC9U,           // dec cx
            0x66U, 0x85U, 0xC9U,           // test cx,cx
            0x74U, 0x11U,                  // jz restore
            0x4CU, 0x8BU, 0x34U, 0x24U,    // mov r14,[rsp]
            0x49U, 0x0FU, 0xBAU, 0xE6U, 0x06U, // bt r14,6
            0x72U, 0x06U,                  // jc restore
            0x9DU,                         // popfq
            0xE9U, 0x00U, 0x00U, 0x00U, 0x00U, // jmp target
            0x9DU,                         // restore: popfq
        });
        const std::size_t target_displacement_offset =
            branch_opcode_offset + 1U;

        const auto append_result = [&code] {
            code.insert(code.end(), {
                0x89U, 0xC8U,                  // mov eax,ecx
                0xBAU, 0x00U, 0x00U, 0x00U, 0x00U,
                0x0FU, 0x95U, 0xC2U,           // setnz dl
                0x48U, 0xC1U, 0xE2U, 0x20U,    // shl rdx,32
                0x48U, 0x09U, 0xD0U,           // or rax,rdx
                0x41U, 0x5EU,                  // pop r14
                0xC3U,                         // ret
            });
        };
        append_result();
        const std::size_t target_offset = code.size();
        append_result();
        const std::int64_t displacement =
            static_cast<std::int64_t>(target_offset) -
            static_cast<std::int64_t>(target_displacement_offset + 4U);
        if (displacement < std::numeric_limits<std::int32_t>::min() ||
            displacement > std::numeric_limits<std::int32_t>::max())
        {
            ok = false;
            continue;
        }
        for (std::size_t index = 0U; index < 4U; ++index)
        {
            code[target_displacement_offset + index] =
                static_cast<std::uint8_t>(
                    (static_cast<std::uint32_t>(displacement) >>
                     (index * 8U)) & 0xFFU);
        }

        ExecutablePage page;
        if (!AllocateCodePage(&page) || !WriteAndArm(page, code))
        {
            ReleasePage(&page);
            ok = false;
            continue;
        }
        using Entry = std::uint64_t (*)();
        Entry entry = nullptr;
        std::memcpy(&entry, &page.base, sizeof(entry));
        const std::uint64_t observed = entry();
        ReleasePage(&page);
        ok = ok && observed == item.expected;
        std::cout << "  long_mode_loopnz_case_cx=" << item.initial_cx
                  << ",zf=" << (item.initial_zf ? 1 : 0)
                  << ",observed=0x" << std::hex << observed << std::dec
                  << "\n";
    }
    std::cout << "long_mode_lowering_16bit_loopnz="
              << (ok ? "true" : "false") << "\n";
    return ok;
}

// The classifier and the rewrite must agree about which instructions have a
// lowering at all, and a segment override must still have none.
bool ProbeClassification()
{
    const std::uint8_t register_memory[] = {0x8BU, 0x03U};
    const std::uint8_t absolute[] = {0x8BU, 0x05U, 0x78U, 0x56U,
                                     0x34U, 0x12U};
    const std::uint8_t gs_override[] = {0x65U, 0x8BU, 0x03U};
    const std::uint8_t register_only[] = {0x31U, 0xC0U};
    const std::uint8_t sixteen_bit_memory[] = {0x67U, 0x32U, 0x00U};

    const auto memory_result =
        ClassifyLongModeBytes(register_memory, sizeof(register_memory));
    const auto absolute_result =
        ClassifyLongModeBytes(absolute, sizeof(absolute));
    const auto segment_result =
        ClassifyLongModeBytes(gs_override, sizeof(gs_override));
    const auto register_result =
        ClassifyLongModeBytes(register_only, sizeof(register_only));
    const auto sixteen_bit_result = ClassifyLongModeBytes(
        sixteen_bit_memory, sizeof(sixteen_bit_memory));

    std::cout << "long_mode_lowering_16bit_probe=compat="
              << static_cast<unsigned>(sixteen_bit_result.compatibility)
              << ",divergence="
              << static_cast<unsigned>(sixteen_bit_result.divergence)
              << ",lowering="
              << static_cast<unsigned>(sixteen_bit_result.lowering) << "\n";

    const bool ok =
        memory_result.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        memory_result.lowering == LongModeLowering::kAddressSizePrefix &&
        absolute_result.compatibility ==
            LongModeByteCompatibility::kNeedsReencode &&
        absolute_result.lowering == LongModeLowering::kAbsoluteToSib &&
        segment_result.compatibility ==
            LongModeByteCompatibility::kUnsupported &&
        segment_result.lowering == LongModeLowering::kNone &&
        register_result.compatibility ==
            LongModeByteCompatibility::kIdenticalBytes &&
        register_result.lowering == LongModeLowering::kNone;

    // A copied instruction has no lowering, and asking for one must be refused
    // rather than answered with its own bytes.
    std::uint8_t buffer[kMaxLoweredBytes] = {};
    std::size_t produced = 0U;
    const bool refuses_identical = !LowerLongModeBytes(
        register_only, sizeof(register_only), buffer, &produced);

    std::cout << "long_mode_lowering_classification="
              << (ok && refuses_identical ? "true" : "false") << "\n";
    return ok && refuses_identical;
}

}  // namespace

bool RunLongModeLoweringProbe()
{
    const bool classification_ok = ProbeClassification();

    // The data page is the one address that must be low, and it is requested
    // exactly rather than hinted: a different address would still be readable
    // and would quietly test nothing about placement.
    const MemoryReservation reserved = platform::ReserveMemory(
        reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(kDataPageAddress)),
        kPageBytes, true, MemoryProtection::kReadWrite);
    const bool placed = reserved.valid && reserved.base != nullptr &&
        reinterpret_cast<std::uintptr_t>(reserved.base) ==
            static_cast<std::uintptr_t>(kDataPageAddress);
    if (reserved.valid && reserved.base != nullptr && !placed)
    {
        platform::ReleaseMemory(reserved.base, kPageBytes);
    }
    if (!placed)
    {
        std::cout << "long_mode_lowering_data_page=false\n"
                     "long_mode_lowering_all=false\n";
        return false;
    }
    std::cout << "long_mode_lowering_data_page=true\n";

    auto* const data = static_cast<std::uint32_t*>(
        static_cast<void*>(static_cast<std::uint8_t*>(reserved.base) +
                           kMarkerOffset));
    *data = kMarker;

    const bool refusals_ok = ProbeAbsoluteRefusals();
    const bool two_byte_esp_ok = ProbeStackPointerTwoByteOpcode();
    const bool sixteen_bit_stack_ok = Probe16BitStackPointerImmediate();
    const bool sixteen_bit_lea_ok = Probe16BitLea32();
    const bool sixteen_bit_lea16_ok = Probe16BitLea16();
    const bool sixteen_bit_test_ok = Probe16BitTest32();
    const bool sixteen_bit_mov_ok = Probe16BitMovImmediate();
    const bool sixteen_bit_mov_register_ok = Probe16BitMovRegister();
    const bool sixteen_bit_loopnz_ok = Probe16BitLoopNz();
    const bool high_byte_ok = ProbeStackPointerHighByteSource(data);
    const bool prefix_ok = ProbeAddressSizePrefix(data);
    const bool absolute_ok = ProbeAbsoluteToSib(data);
    const bool absolute_imm_ok = ProbeAbsoluteToSibImmediate(data);
    // After the ones above, because it writes through `data` and they read a
    // marker from it.
    const bool moffs_ok = ProbeMoffs(data);
    platform::ReleaseMemory(reserved.base, kPageBytes);

    const bool all = classification_ok && refusals_ok &&
        two_byte_esp_ok && sixteen_bit_stack_ok && sixteen_bit_lea_ok &&
        sixteen_bit_lea16_ok && sixteen_bit_test_ok && sixteen_bit_mov_ok &&
        sixteen_bit_mov_register_ok &&
        sixteen_bit_loopnz_ok &&
        high_byte_ok &&
        prefix_ok && absolute_ok &&
        absolute_imm_ok && moffs_ok;
    std::cout << "long_mode_lowering_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
