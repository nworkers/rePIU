#include "aot_direct_return_table_probe.h"

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_direct_return_table.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace repiu::tools
{
namespace
{

using repiu::runtime::AotDirectReturnEntry;
using repiu::runtime::AotDirectReturnProbeSite;
using repiu::runtime::AotDirectReturnTable;
using repiu::runtime::AotDirectReturnTableIndex;
using repiu::runtime::ClearAotDirectReturnTable;
using repiu::runtime::EmitAotDirectReturnProbe;
using repiu::runtime::InsertAotDirectReturnEntry;
using repiu::runtime::LookupAotDirectReturnEntry;
using repiu::runtime::PatchAotDirectReturnProbe;
using repiu::runtime::ResetAotDirectReturnTable;
using repiu::runtime::ResolveAotDirectReturnTableBits;
using repiu::runtime::kAotDirectReturnTableMaximumBits;
using repiu::runtime::kAotDirectReturnTableMinimumBits;
using repiu::runtime::kDefaultAotDirectReturnTableBits;

constexpr std::uint32_t kGuestPage = 0x04010000U;

runtime::AotInstructionRecord MakeReturnInstruction(
    const std::uint32_t guest_address)
{
    runtime::AotInstructionRecord instruction;
    instruction.guest_address = guest_address;
    instruction.kind = runtime::AotInstructionKind::kReturn;
    instruction.length = 1U;
    instruction.bytes = {0xC3U};
    return instruction;
}

runtime::AotTranslationPlan MakeReturnPlan()
{
    runtime::AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestPage;
    runtime::AotBasicBlock block;
    block.guest_address = kGuestPage;
    block.instructions.push_back(MakeReturnInstruction(kGuestPage));
    plan.blocks.push_back(std::move(block));
    return plan;
}

bool ProbeBitsResolution()
{
    return ResolveAotDirectReturnTableBits(nullptr) ==
            kDefaultAotDirectReturnTableBits &&
        ResolveAotDirectReturnTableBits("") ==
            kDefaultAotDirectReturnTableBits &&
        ResolveAotDirectReturnTableBits("nonsense") ==
            kDefaultAotDirectReturnTableBits &&
        ResolveAotDirectReturnTableBits("12x") ==
            kDefaultAotDirectReturnTableBits &&
        ResolveAotDirectReturnTableBits("0") ==
            kAotDirectReturnTableMinimumBits &&
        ResolveAotDirectReturnTableBits("99") ==
            kAotDirectReturnTableMaximumBits &&
        ResolveAotDirectReturnTableBits("10") == 10U;
}

bool ProbeTableAccounting()
{
    AotDirectReturnTable table;
    ResetAotDirectReturnTable(&table, 8U);
    bool ok = table.entries.size() == 256U && table.mask == 255U;

    // A zero key or target never enters, so the empty-slot convention holds.
    ok = ok && !InsertAotDirectReturnEntry(&table, 0U, 0x0E000000U) &&
        !InsertAotDirectReturnEntry(&table, 0x04001000U, 0U) &&
        table.insert_count == 0U;

    ok = ok && InsertAotDirectReturnEntry(&table, 0x04001000U, 0x0E000100U) &&
        table.insert_count == 1U && table.overwrite_count == 0U;
    std::uint32_t resolved = 0;
    ok = ok && LookupAotDirectReturnEntry(table, 0x04001000U, &resolved) &&
        resolved == 0x0E000100U;
    ok = ok && !LookupAotDirectReturnEntry(table, 0x04002000U, &resolved);
    ok = ok && !LookupAotDirectReturnEntry(table, 0U, &resolved);

    // Refreshing the same key is not an overwrite; a colliding key is.
    ok = ok && InsertAotDirectReturnEntry(&table, 0x04001000U, 0x0E000200U) &&
        table.overwrite_count == 0U;
    const std::uint32_t index =
        AotDirectReturnTableIndex(0x04001000U, table.mask);
    std::uint32_t colliding = 0;
    for (std::uint32_t candidate = 0x04001001U;
         candidate < 0x04001001U + 0x40000U; ++candidate)
    {
        if (AotDirectReturnTableIndex(candidate, table.mask) == index)
        {
            colliding = candidate;
            break;
        }
    }
    ok = ok && colliding != 0U &&
        InsertAotDirectReturnEntry(&table, colliding, 0x0E000300U) &&
        table.overwrite_count == 1U &&
        !LookupAotDirectReturnEntry(table, 0x04001000U, &resolved) &&
        LookupAotDirectReturnEntry(table, colliding, &resolved) &&
        resolved == 0x0E000300U;

    ClearAotDirectReturnTable(&table);
    ok = ok && table.clear_count == 1U &&
        table.cleared_entry_count == 256U &&
        !LookupAotDirectReturnEntry(table, colliding, &resolved) &&
        table.entries.size() == 256U;

    // Reset clamps the requested size into the supported range.
    ResetAotDirectReturnTable(&table, 2U);
    ok = ok && table.entries.size() ==
        (std::size_t{1} << kAotDirectReturnTableMinimumBits);
    ResetAotDirectReturnTable(&table, 99U);
    return ok && table.entries.size() ==
        (std::size_t{1} << kAotDirectReturnTableMaximumBits);
}

bool ProbeEmissionGate()
{
    const runtime::AotTranslationPlan plan = MakeReturnPlan();
    runtime::AotCodeCacheBuildOptions off_options;
    off_options.enable_dbt_return_miss_dispatch = true;
    runtime::AotCodeCacheImage off_image;
    if (!runtime::BuildAotCodeCacheImage(plan, off_options, &off_image))
    {
        return false;
    }
    runtime::AotCodeCacheBuildOptions on_options = off_options;
    on_options.enable_direct_return_table = true;
    runtime::AotCodeCacheImage on_image;
    if (!runtime::BuildAotCodeCacheImage(plan, on_options, &on_image))
    {
        return false;
    }
    // Disabled emits nothing at all, so the control side of an A/B is a
    // byte-for-byte control.
    if (!off_image.direct_return_probe_sites.empty() ||
        off_image.indirect_inline_cache_sites.size() != 1U ||
        off_image.indirect_inline_cache_sites[0].miss_probe_cache_offset != 0U)
    {
        return false;
    }
    if (on_image.direct_return_probe_sites.size() != 1U ||
        on_image.bytes.size() <= off_image.bytes.size())
    {
        return false;
    }
    const runtime::AotIndirectInlineCacheSite& site =
        on_image.indirect_inline_cache_sites[0];
    const runtime::AotDirectReturnProbeSite& probe =
        on_image.direct_return_probe_sites[0];
    // Guards must reach the probe, the probe must sit immediately before the
    // miss tail, and the miss tail itself must be unchanged: popfd first, and
    // the dispatch site keyed on that same offset.
    return site.miss_probe_cache_offset == probe.cache_offset &&
        probe.cache_offset < site.miss_cache_offset &&
        on_image.bytes[site.miss_cache_offset] == 0x9DU &&
        on_image.dbt_return_dispatch_sites.size() == 1U &&
        on_image.dbt_return_dispatch_sites[0].miss_cache_offset ==
            site.miss_cache_offset &&
        runtime::AotInlineCacheGuardTargetOffset(site) ==
            probe.cache_offset;
}

bool ProbePatchOperands()
{
    std::vector<std::uint8_t> bytes;
    AotDirectReturnProbeSite site;
    if (!EmitAotDirectReturnProbe(&bytes, kGuestPage, 4U, &site))
    {
        return false;
    }
    constexpr std::uint32_t kKeyAddress = 0x30000000U;
    constexpr std::uint32_t kMask = 0x1FFFU;
    constexpr std::uint32_t kCounter = 0x30100000U;
    if (!PatchAotDirectReturnProbe(bytes.data(), bytes.size(), site,
                                   kKeyAddress, kMask, kCounter))
    {
        return false;
    }
    auto read = [&bytes](const std::uint32_t offset) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    };
    const bool operands = read(site.mask_immediate_offset) == kMask &&
        read(site.key_address_offset) == kKeyAddress &&
        read(site.target_address_offset) == kKeyAddress + 4U &&
        read(site.hit_counter_address_offset) == kCounter;
    // A truncated buffer must be refused rather than written past.
    const bool bounds = !PatchAotDirectReturnProbe(
        bytes.data(), site.hit_counter_address_offset + 1U, site, kKeyAddress,
        kMask, kCounter);
    std::vector<std::uint8_t> wide;
    AotDirectReturnProbeSite wide_site;
    const bool wide_ok =
        EmitAotDirectReturnProbe(&wide, kGuestPage, 12U, &wide_site) &&
        wide.size() == bytes.size() + 2U;
    return operands && bounds && wide_ok;
}

#if defined(_WIN32) && defined(_M_IX86)

// The emitted probe resolves a return by overwriting the guest return slot and
// executing the original RET, so the only complete test is to run it.
std::uint32_t g_landing_esp = 0;
std::uint32_t g_landing_eax = 0;
std::uint32_t g_landing_ecx = 0;
std::uint32_t g_entry_esp = 0;
std::uint32_t g_miss_esp = 0;
std::uint32_t g_probe_address = 0;

// The probe's RET lands here with the harness's callee saves back on top of
// the stack, so this pad both records the observation and unwinds the harness.
extern "C" void __declspec(naked) DirectReturnProbeLanding()
{
    __asm
    {
        mov g_landing_esp, esp
        mov g_landing_eax, eax
        mov g_landing_ecx, ecx
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

// Builds the guest-side frame the probe expects -- an argument word for the
// `ret imm16` case, the return target, then the site's pushfd -- and enters it.
extern "C" void __declspec(naked) DirectReturnProbeHarness(
    std::uint32_t /*target*/, std::uint32_t /*argument_words*/)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov g_entry_esp, esp
        mov ecx, dword ptr [ebp + 12]  // argument_words
        test ecx, ecx
        jz no_argument
        push 0xA5A5A5A5
    no_argument:
        mov eax, dword ptr [ebp + 8]   // guest return target
        push eax
        mov eax, 0x11111111
        mov ecx, 0x22222222
        pushfd
        jmp dword ptr [g_probe_address]
    }
}

extern "C" void __declspec(naked) DirectReturnProbeMissTail()
{
    __asm
    {
        // Reached only when the probe falls through, with flags and the return
        // target still on the stack exactly as the real miss tail expects.
        // Restoring ESP from the recorded entry value abandons that frame
        // whatever its size, so one tail serves both RET forms.
        mov g_miss_esp, esp
        mov esp, g_entry_esp
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

bool ProbeEmittedExecution()
{
    AotDirectReturnTable table;
    ResetAotDirectReturnTable(&table, 10U);
    bool ok = true;
    for (const std::uint32_t pop_bytes : {4U, 8U})
    {
        std::vector<std::uint8_t> bytes;
        AotDirectReturnProbeSite site;
        if (!EmitAotDirectReturnProbe(&bytes, kGuestPage, pop_bytes, &site))
        {
            return false;
        }
        // The miss tail jumps to the harness's own continuation.
        bytes.push_back(0xE9U);
        const std::size_t miss_jump_offset = bytes.size();
        bytes.insert(bytes.end(), 4U, 0U);
        auto* code = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, bytes.size() + 16U, MEM_COMMIT | MEM_RESERVE,
                         PAGE_EXECUTE_READWRITE));
        if (code == nullptr)
        {
            return false;
        }
        std::memcpy(code, bytes.data(), bytes.size());
        const auto key_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(table.entries.data()));
        const auto counter_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&table.hit_count));
        if (!PatchAotDirectReturnProbe(code, bytes.size(), site, key_address,
                                       table.mask, counter_address))
        {
            VirtualFree(code, 0, MEM_RELEASE);
            return false;
        }
        const auto miss_target = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(&DirectReturnProbeMissTail) -
            (reinterpret_cast<std::uintptr_t>(code) + miss_jump_offset + 4U));
        std::memcpy(code + miss_jump_offset, &miss_target,
                    sizeof(miss_target));
        FlushInstructionCache(GetCurrentProcess(), code, bytes.size());
        g_probe_address =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code));

        // Miss: the table is empty, so control must reach the miss tail with
        // flags and the return target still on the stack.
        ClearAotDirectReturnTable(&table);
        g_miss_esp = 0;
        g_landing_esp = 0;
        const std::uint32_t argument_words = pop_bytes == 4U ? 0U : 1U;
        DirectReturnProbeHarness(kGuestPage, argument_words);
        ok = ok && g_miss_esp != 0U && g_landing_esp == 0U &&
            table.hit_count == 0U;

        // Hit: the landing pad must be reached with the stack unwound exactly
        // as the original RET would leave it, and with eax and ecx restored.
        const auto landing = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&DirectReturnProbeLanding));
        InsertAotDirectReturnEntry(&table, kGuestPage, landing);
        g_miss_esp = 0;
        g_landing_esp = 0;
        g_landing_eax = 0;
        g_landing_ecx = 0;
        DirectReturnProbeHarness(kGuestPage, argument_words);
        ok = ok && g_miss_esp == 0U && g_landing_esp == g_entry_esp &&
            g_landing_eax == 0x11111111U && g_landing_ecx == 0x22222222U &&
            table.hit_count == 1U;
        table.hit_count = 0;
        VirtualFree(code, 0, MEM_RELEASE);
    }
    return ok;
}

#else

bool ProbeEmittedExecution()
{
    return true;
}

#endif

}  // namespace

bool RunAotDirectReturnTableProbe()
{
    const bool bits_ok = ProbeBitsResolution();
    const bool accounting_ok = ProbeTableAccounting();
    const bool gate_ok = ProbeEmissionGate();
    const bool operands_ok = ProbePatchOperands();
    const bool execution_ok = ProbeEmittedExecution();
    const bool all = bits_ok && accounting_ok && gate_ok && operands_ok &&
        execution_ok;
    std::cout << "direct_return_bits=" << (bits_ok ? "true" : "false")
              << "\ndirect_return_accounting="
              << (accounting_ok ? "true" : "false")
              << "\ndirect_return_emission_gate="
              << (gate_ok ? "true" : "false")
              << "\ndirect_return_operands="
              << (operands_ok ? "true" : "false")
              << "\ndirect_return_execution="
              << (execution_ok ? "true" : "false")
              << "\ndirect_return_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
