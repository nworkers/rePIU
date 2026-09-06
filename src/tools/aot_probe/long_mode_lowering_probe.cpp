#include "long_mode_lowering_probe.h"

#include "repiu/platform/virtual_memory.h"
#include "repiu/runtime/aot_long_mode_compatibility.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::platform::MemoryProtection;
using repiu::platform::MemoryReservation;
using repiu::runtime::ClassifyLongModeBytes;
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
           std::vector<std::uint8_t>* lowered)
{
    std::uint8_t buffer[kMaxLoweredBytes] = {};
    std::size_t produced = 0U;
    if (!LowerLongModeBytes(guest.data(), guest.size(), buffer, &produced))
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

    const std::uint8_t destination[] = {0x8AU, 0x24U, 0x24U};
    const std::uint8_t exchange[] = {0x86U, 0x24U, 0x24U};
    const auto destination_verdict = ClassifyLongModeBytes(
        destination, sizeof(destination));
    const auto exchange_verdict = ClassifyLongModeBytes(exchange,
                                                        sizeof(exchange));
    const bool refusals =
        destination_verdict.compatibility ==
            LongModeByteCompatibility::kUnsupported &&
        destination_verdict.lowering == LongModeLowering::kNone &&
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

// The classifier and the rewrite must agree about which instructions have a
// lowering at all, and a segment override must still have none.
bool ProbeClassification()
{
    const std::uint8_t register_memory[] = {0x8BU, 0x03U};
    const std::uint8_t absolute[] = {0x8BU, 0x05U, 0x78U, 0x56U,
                                     0x34U, 0x12U};
    const std::uint8_t gs_override[] = {0x65U, 0x8BU, 0x03U};
    const std::uint8_t register_only[] = {0x31U, 0xC0U};

    const auto memory_result =
        ClassifyLongModeBytes(register_memory, sizeof(register_memory));
    const auto absolute_result =
        ClassifyLongModeBytes(absolute, sizeof(absolute));
    const auto segment_result =
        ClassifyLongModeBytes(gs_override, sizeof(gs_override));
    const auto register_result =
        ClassifyLongModeBytes(register_only, sizeof(register_only));

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
    const bool high_byte_ok = ProbeStackPointerHighByteSource(data);
    const bool prefix_ok = ProbeAddressSizePrefix(data);
    const bool absolute_ok = ProbeAbsoluteToSib(data);
    const bool absolute_imm_ok = ProbeAbsoluteToSibImmediate(data);
    // After the ones above, because it writes through `data` and they read a
    // marker from it.
    const bool moffs_ok = ProbeMoffs(data);
    platform::ReleaseMemory(reserved.base, kPageBytes);

    const bool all = classification_ok && refusals_ok &&
        two_byte_esp_ok && high_byte_ok && prefix_ok && absolute_ok &&
        absolute_imm_ok && moffs_ok;
    std::cout << "long_mode_lowering_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
