#include "repiu/exe/dos4gw_loader.h"
#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/target/target_profile.h"

#include <Zydis.h>

#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{

bool ReadFile(const std::filesystem::path& path,
              std::vector<std::uint8_t>* bytes)
{
    if (bytes == nullptr)
    {
        return false;
    }
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        return false;
    }
    const std::streamoff size = stream.tellg();
    if (size <= 0)
    {
        return false;
    }
    bytes->resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    return static_cast<bool>(stream.read(
        reinterpret_cast<char*>(bytes->data()), size));
}

void PrintPlan(const repiu::runtime::AotTranslationPlan& plan)
{
    std::cout << "valid=" << (plan.valid ? "true" : "false") << "\n"
              << "entry=0x" << std::hex << plan.entry_address << std::dec
              << "\nblocks=" << plan.block_count
              << "\ninstructions=" << plan.instruction_count
              << "\nsource_bytes=" << plan.source_code_bytes
              << "\nestimated_emitted_bytes="
              << plan.estimated_emitted_bytes
              << "\ncopy_instructions=" << plan.copy_instruction_count
              << "\ndirect_calls=" << plan.direct_call_count
              << "\ndirect_jumps=" << plan.direct_jump_count
              << "\nconditional_branches="
              << plan.conditional_branch_count
              << "\nreturns=" << plan.return_count
              << "\nhle_boundaries=" << plan.hle_boundary_count
              << "\nindirect_exits=" << plan.indirect_exit_count
              << "\njump_tables=" << plan.jump_table_count
              << "\njump_table_targets=" << plan.jump_table_target_count
              << "\noutside_targets=" << plan.outside_image_target_count
              << "\ndecode_failures=" << plan.decode_failure_count
              << "\nanalysis_limits=" << plan.analysis_limit_count
              << "\nelapsed_us=" << plan.elapsed_microseconds
              << "\nmessage=" << plan.message << "\n";
}

void PrintCache(const repiu::runtime::AotCodeCacheImage& image)
{
    std::cout << "cache_valid=" << (image.valid ? "true" : "false")
              << "\ncache_executable="
              << (image.executable ? "true" : "false")
              << "\ncache_entry_offset=" << image.entry_cache_offset
              << "\ncache_bytes=" << image.bytes.size()
              << "\ncache_map_entries=" << image.address_map.size()
              << "\ncache_fixups=" << image.fixups.size()
              << "\ncache_resolved_fixups=" << image.resolved_fixup_count
              << "\ncache_external_fixups=" << image.external_fixup_count
              << "\ncache_unsupported_branches="
              << image.unsupported_branch_count
              << "\ncache_jump_table_sites=" << image.jump_table_sites.size()
              << "\ncache_decode_failures=" << image.decode_failure_count
              << "\ncache_elapsed_us=" << image.elapsed_microseconds
              << "\ncache_message=" << image.message << "\n";
}

const std::uint8_t* FindImageBytes(
    const repiu::runtime::RelocatedRuntimeImage& image,
    std::uint32_t address, std::size_t* available)
{
    if (available == nullptr)
    {
        return nullptr;
    }
    for (const auto& object : image.objects)
    {
        if (address < object.relocated_base_address)
        {
            continue;
        }
        const std::uint64_t offset =
            static_cast<std::uint64_t>(address) - object.relocated_base_address;
        if (offset < object.memory.size())
        {
            *available = object.memory.size() - static_cast<std::size_t>(offset);
            return object.memory.data() + offset;
        }
    }
    return nullptr;
}

void PrintLinearDisassembly(const repiu::runtime::RelocatedRuntimeImage& image,
                            std::uint32_t address)
{
    for (std::uint32_t count = 0; count < 24U; ++count)
    {
        std::size_t available = 0;
        const std::uint8_t* bytes = FindImageBytes(image, address, &available);
        ZydisDisassembledInstruction instruction{};
        if (bytes == nullptr || !ZYAN_SUCCESS(ZydisDisassembleIntel(
                ZYDIS_MACHINE_MODE_LEGACY_32, address, bytes, available,
                &instruction)))
        {
            break;
        }
        std::cout << "query_linear=0x" << std::hex << address << ",text="
                  << instruction.text << std::dec << "\n";
        address += instruction.info.length;
    }
}

#if defined(_WIN32)
bool HasPageProtection(const void* address, DWORD expected_protection)
{
    MEMORY_BASIC_INFORMATION memory = {};
    return VirtualQuery(address, &memory, sizeof(memory)) != 0 &&
           (memory.Protect & 0xFFU) == expected_protection;
}

bool RunCoherenceProbe()
{
    constexpr std::uint32_t kPageSize = 0x1000U;
    auto* guest = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 2U * kPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (guest == nullptr ||
        reinterpret_cast<std::uintptr_t>(guest) >
            std::numeric_limits<std::uint32_t>::max())
    {
        if (guest != nullptr)
        {
            VirtualFree(guest, 0, MEM_RELEASE);
        }
        std::cout << "coherence_all=false\n";
        return false;
    }
    const std::uint8_t original[] = {
        0xB8U, 0x78U, 0x56U, 0x34U, 0x12U, 0xC3U};
    std::memcpy(guest, original, sizeof(original));
    const std::uint8_t synthetic_store[] = {0xC6U, 0x07U, 0xE9U};
    std::memcpy(guest + 0x20U, synthetic_store, sizeof(synthetic_store));
    const std::uint32_t guest_address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(guest));

    repiu::runtime::RelocatedRuntimeImage runtime;
    runtime.valid = true;
    runtime.relocated_image_base = guest_address;
    runtime.relocated_entry_linear_address = guest_address;
    repiu::runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = guest_address;
    object.virtual_size = 2U * kPageSize;
    object.memory.resize(2U * kPageSize);
    std::memcpy(object.memory.data(), guest, 2U * kPageSize);
    runtime.objects.push_back(std::move(object));

    repiu::runtime::AotTranslationPlan plan;
    repiu::runtime::AotCodeCacheImage cache;
    repiu::platform::win32::Win32AotCodeCachePlacement placement;
    const bool built =
        repiu::runtime::BuildAotTranslationPlanFromEntry(
            runtime, guest_address, &plan) &&
        repiu::runtime::BuildAotCodeCacheImage(plan, &cache) &&
        repiu::platform::win32::PlaceWin32AotCodeCache(
            cache, &placement) && placement.placed;
    if (!built)
    {
        repiu::platform::win32::ReleaseWin32AotCodeCache(&placement);
        VirtualFree(guest, 0, MEM_RELEASE);
        std::cout << "coherence_all=false\n";
        return false;
    }

    std::uint32_t old_cache = 0U;
    const bool initial =
        repiu::platform::win32::FindAotCacheAddress(
            placement, guest_address, &old_cache) &&
        repiu::platform::win32::Win32AotGuestRangeHasActiveTranslation(
            placement, guest_address, sizeof(original));
    repiu::platform::win32::Win32AotGuestPageRetireResult retirement;
    const bool retired =
        repiu::platform::win32::RetireWin32AotGuestPage(
            &placement, guest_address, false, &retirement) &&
        retirement.retired && !retirement.quarantined &&
        retirement.retired_entry_count >= 1U;
    std::uint32_t lookup_after_retire = 0U;
    std::uint32_t provenance_guest = 0U;
    const bool provenance = retired &&
        !repiu::platform::win32::FindAotCacheAddress(
            placement, guest_address, &lookup_after_retire) &&
        repiu::platform::win32::FindAotGuestAddress(
            placement, old_cache, &provenance_guest) &&
        provenance_guest == guest_address &&
        repiu::platform::win32::IsWin32AotCacheAddressRetired(
            placement, old_cache) &&
        *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(old_cache)) == 0xCCU;

    guest[4] = 0x13U;
    repiu::platform::win32::Win32AotPageWriteWatchSet watches;
    repiu::platform::win32::Win32AotDynamicAppendResult generation;
    const bool appended =
        repiu::platform::win32::AppendWin32DynamicAotTranslation(
            guest_address, 2U * kPageSize, guest_address, {}, &watches,
            &placement, &generation) && generation.appended;
    std::uint32_t new_cache = 0U;
    const std::uint8_t expected[] = {
        0xB8U, 0x78U, 0x56U, 0x34U, 0x13U};
    const bool live_snapshot = appended && generation.generation == 2U &&
        generation.cache_entry != old_cache &&
        repiu::platform::win32::FindAotCacheAddress(
            placement, guest_address, &new_cache) &&
        new_cache == generation.cache_entry &&
        std::memcmp(reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(new_cache)),
                    expected, sizeof(expected)) == 0 &&
        !repiu::platform::win32::IsWin32AotGuestPageRetired(
            placement, guest_address) &&
        !repiu::platform::win32::IsWin32AotGuestPageQuarantined(
            placement, guest_address) &&
         repiu::platform::win32::Win32AotGuestRangeHasActiveTranslation(
             placement, guest_address, sizeof(original));

    const std::uint32_t watched_write = guest_address + 0x30U;
    const bool write_watch_began =
        repiu::platform::win32::BeginWin32AotGuestWrite(
            &watches, guest_address + 0x20U, watched_write, false, false,
            guest_address + 0x20U);
    const bool write_watch_writable = write_watch_began && HasPageProtection(
        guest, PAGE_EXECUTE_READWRITE);
    if (write_watch_began)
    {
        guest[0x30U] = 0xA5U;
    }
    repiu::platform::win32::Win32AotGuestWriteCompletion completion;
    const bool write_watch_completed = write_watch_began &&
        repiu::platform::win32::CompleteWin32AotGuestWrite(
            &watches, &completion);
    const bool write_watch = write_watch_writable && write_watch_completed &&
        completion.destination == watched_write && completion.byte_count == 1U &&
        HasPageProtection(guest, PAGE_EXECUTE_READ);

    std::int32_t displacement = 0;
    std::memcpy(&displacement,
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(old_cache + 1U)),
                sizeof(displacement));
    const std::uint32_t relink_target = static_cast<std::uint32_t>(
        static_cast<std::int64_t>(old_cache) + 5U + displacement);
    const bool relink = live_snapshot &&
        generation.relinked_entry_count >= 1U &&
        *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(old_cache)) == 0xE9U &&
        relink_target == new_cache;

    repiu::platform::win32::Win32AotGuestPageRetireResult second_retirement;
    const bool repeat_retirement =
        repiu::platform::win32::RetireWin32AotGuestPage(
            &placement, guest_address, false, &second_retirement) &&
        second_retirement.retired &&
        second_retirement.retired_entry_count >= 1U;

    const std::uint32_t excluded_target = guest_address + kPageSize;
    const std::int32_t jump_displacement = static_cast<std::int32_t>(
        excluded_target - (guest_address + 5U));
    const bool excluded_write_began =
        repiu::platform::win32::BeginWin32AotGuestWrite(
            &watches, guest_address + 0x20U, guest_address, false, false,
            guest_address + 0x20U);
    if (excluded_write_began)
    {
        guest[0] = 0xE9U;
        std::memcpy(guest + 1U, &jump_displacement, sizeof(jump_displacement));
        guest[kPageSize] = 0x0FU;
        guest[kPageSize + 1U] = 0x0BU;
    }
    repiu::platform::win32::Win32AotGuestWriteCompletion excluded_completion;
    const bool excluded_write_completed = excluded_write_began &&
        repiu::platform::win32::CompleteWin32AotGuestWrite(
            &watches, &excluded_completion);
    std::memcpy(runtime.objects[0].memory.data(), guest, 2U * kPageSize);
    repiu::runtime::AotTranslationPlan excluded_plan;
    repiu::runtime::AotCodeCacheImage excluded_cache;
    const bool excluded_boundary =
        repiu::runtime::BuildAotTranslationPlanFromEntry(
            runtime, guest_address,
            {{excluded_target, 8U}}, &excluded_plan) &&
        repiu::runtime::BuildAotCodeCacheImage(
            excluded_plan, &excluded_cache) &&
        [&excluded_cache, excluded_target]() {
            for (const auto& entry : excluded_cache.address_map)
            {
                if (entry.guest_address == excluded_target)
                {
                    return entry.emitted_length == 1U &&
                        excluded_cache.bytes[entry.cache_offset] == 0xCCU;
                }
            }
            return false;
        }();
    repiu::platform::win32::Win32AotDynamicAppendResult excluded_generation;
    const bool excluded_appended = excluded_write_completed &&
        repiu::platform::win32::AppendWin32DynamicAotTranslation(
            guest_address, 2U * kPageSize, guest_address,
            {{excluded_target, 8U}}, &watches, &placement,
            &excluded_generation) && excluded_generation.appended;
    const bool excluded_unwatched = excluded_appended &&
        !repiu::platform::win32::IsWin32AotGuestPageWriteWatched(
            watches, excluded_target) &&
        [&placement, excluded_target]() {
            for (std::size_t index = 0;
                 index < placement.address_map.size(); ++index)
            {
                if (placement.address_map[index].guest_address == excluded_target &&
                    index < placement.address_map_states.size())
                {
                    return !placement.address_map_states[index].tracks_guest_bytes;
                }
            }
            return false;
        }();
    repiu::platform::win32::RestoreWin32AotGuestPageWriteWatches(&watches);
    const bool watch_cleanup = HasPageProtection(guest, PAGE_READWRITE) &&
        HasPageProtection(guest + kPageSize, PAGE_READWRITE);

    const bool all = initial && retired && provenance && live_snapshot &&
        relink && write_watch && repeat_retirement && excluded_boundary &&
        excluded_unwatched && watch_cleanup;
    std::cout << "coherence_initial=" << (initial ? "true" : "false")
              << "\ncoherence_retirement="
              << (retired ? "true" : "false")
              << "\ncoherence_provenance="
              << (provenance ? "true" : "false")
              << "\ncoherence_live_snapshot="
              << (live_snapshot ? "true" : "false")
              << "\ncoherence_generation="
              << (appended ? "true" : "false")
              << "\ncoherence_relink=" << (relink ? "true" : "false")
              << "\ncoherence_write_watch="
              << (write_watch ? "true" : "false")
              << "\ncoherence_repeat_retirement="
              << (repeat_retirement ? "true" : "false")
              << "\ncoherence_excluded_boundary="
              << (excluded_boundary ? "true" : "false")
              << "\ncoherence_excluded_unwatched="
              << (excluded_unwatched ? "true" : "false")
              << "\ncoherence_watch_cleanup="
              << (watch_cleanup ? "true" : "false")
              << "\ncoherence_generation_id=" << generation.generation
              << "\ncoherence_all=" << (all ? "true" : "false") << "\n";

    repiu::platform::win32::ReleaseWin32AotCodeCache(&placement);
    VirtualFree(guest, 0, MEM_RELEASE);
    return all;
}
#endif

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
    {
        std::cerr << "usage: repiu_aot_probe <DOS4GW.EXE> [guest-address]\n";
        return 2;
    }
    const std::filesystem::path path = argv[1];
    std::vector<std::uint8_t> bytes;
    if (!ReadFile(path, &bytes))
    {
        std::cerr << "failed to read input executable\n";
        return 1;
    }
    const repiu::target::TargetProfile profile{
        "aot_probe", "AOT Probe", path, path.parent_path(),
        path.parent_path(),
        repiu::target::ExecutableFormatHint::kDos4gwLe,
        "dos4gw_console_sample",
        {true, 0x00010000U, 0x04000000U}};
    repiu::exe::ParseError error;
    repiu::exe::Dos4gwLoadResult load;
    if (!repiu::exe::LoadDos4gwExecutable(bytes, profile, &load, &error))
    {
        std::cerr << "DOS/4GW load failed: " << error.message << "\n";
        return 1;
    }
    repiu::runtime::RelocatableRuntimeImagePlan relocation;
    if (!repiu::runtime::BuildRelocatableRuntimeImagePlan(
            load, 0x01000000U, &relocation, &error))
    {
        std::cerr << "relocation plan failed: " << error.message << "\n";
        return 1;
    }
    repiu::runtime::RelocatedRuntimeImage image;
    if (!repiu::runtime::BuildRelocatedRuntimeImage(
            load, relocation, &image, &error))
    {
        std::cerr << "relocated image failed: " << error.message << "\n";
        return 1;
    }
    repiu::runtime::AotTranslationPlan plan;
    if (!repiu::runtime::BuildAotTranslationPlan(image, &plan))
    {
        PrintPlan(plan);
        return 1;
    }
    PrintPlan(plan);
    if (argc == 3)
    {
        char* end = nullptr;
        const unsigned long query = std::strtoul(argv[2], &end, 0);
        if (end == argv[2] || *end != '\0' || query > UINT32_MAX)
        {
            std::cerr << "invalid guest address query\n";
            return 2;
        }
        for (const auto& block : plan.blocks)
        {
            bool block_matches = false;
            for (const auto& instruction : block.instructions)
            {
                if (instruction.guest_address == query)
                {
                    block_matches = true;
                    break;
                }
            }
            if (block_matches)
            {
                std::cout << "query_block=0x" << std::hex
                          << block.guest_address << std::dec << "\n";
                for (const auto& instruction : block.instructions)
                {
                    std::cout << "query_block_instruction=0x" << std::hex
                              << instruction.guest_address << ",kind="
                              << std::dec
                              << static_cast<unsigned>(instruction.kind)
                              << ",target=0x" << std::hex
                              << instruction.direct_target
                              << ",fallthrough=0x"
                              << instruction.fallthrough_target << ",bytes=";
                    for (std::uint8_t byte : instruction.bytes)
                    {
                        std::cout << std::setw(2) << std::setfill('0')
                                  << static_cast<unsigned>(byte);
                    }
                    std::cout << std::dec << "\n";
                }
            }
            for (const auto& instruction : block.instructions)
            {
                if (instruction.guest_address != query &&
                    instruction.direct_target != query &&
                    instruction.fallthrough_target != query)
                {
                    continue;
                }
                std::cout << "query_instruction=0x" << std::hex
                          << instruction.guest_address
                          << ",kind=" << std::dec
                          << static_cast<unsigned>(instruction.kind)
                          << ",target=0x" << std::hex
                          << instruction.direct_target
                          << ",fallthrough=0x"
                          << instruction.fallthrough_target << ",bytes=";
                for (std::uint8_t byte : instruction.bytes)
                {
                    std::cout << std::setw(2) << std::setfill('0')
                              << static_cast<unsigned>(byte);
                }
                std::cout << std::dec << "\n";
            }
        }
        PrintLinearDisassembly(image, static_cast<std::uint32_t>(query));
    }
    repiu::runtime::AotCodeCacheImage cache;
    if (!repiu::runtime::BuildAotCodeCacheImage(plan, &cache))
    {
        PrintCache(cache);
        return 1;
    }
    PrintCache(cache);
    if (argc == 3)
    {
        const unsigned long query = std::strtoul(argv[2], nullptr, 0);
        for (const auto& entry : cache.address_map)
        {
            if (entry.guest_address != query)
            {
                continue;
            }
            std::cout << "query_cache=0x" << std::hex
                      << entry.guest_address << ",offset=0x"
                      << entry.cache_offset << ",guest_length=" << std::dec
                      << static_cast<unsigned>(entry.guest_length)
                      << ",emitted_length="
                      << static_cast<unsigned>(entry.emitted_length)
                      << ",bytes=";
            for (std::uint32_t index = 0;
                 index < entry.emitted_length &&
                 entry.cache_offset + index < cache.bytes.size(); ++index)
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(
                              cache.bytes[entry.cache_offset + index]);
            }
            std::cout << std::dec << "\n";
        }
    }
#if defined(_WIN32)
    repiu::platform::win32::Win32AotCodeCachePlacement placement;
    if (!repiu::platform::win32::PlaceWin32AotCodeCache(cache, &placement) ||
        !placement.placed)
    {
        std::cout << "cache_placement=false\ncache_placement_message="
                  << placement.message << "\n";
        return 1;
    }
    std::uint32_t mapped_guest = 0;
    std::uint32_t mapped_cache = 0;
    const bool round_trip = repiu::platform::win32::FindAotGuestAddress(
                                placement, placement.entry_address,
                                &mapped_guest) &&
        repiu::platform::win32::FindAotCacheAddress(
            placement, mapped_guest, &mapped_cache) &&
        mapped_guest == plan.entry_address &&
        mapped_cache == placement.entry_address;
    std::cout << "cache_placement=true\ncache_round_trip="
              << (round_trip ? "true" : "false") << "\n";
    repiu::platform::win32::ReleaseWin32AotCodeCache(&placement);
    if (!round_trip)
    {
        return 1;
    }
    if (!RunCoherenceProbe())
    {
        return 1;
    }
#endif
    return 0;
}
