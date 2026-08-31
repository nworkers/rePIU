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

    const bool prefix_ok = ProbeAddressSizePrefix(data);
    const bool absolute_ok = ProbeAbsoluteToSib(data);
    platform::ReleaseMemory(reserved.base, kPageBytes);

    const bool all = classification_ok && prefix_ok && absolute_ok;
    std::cout << "long_mode_lowering_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
