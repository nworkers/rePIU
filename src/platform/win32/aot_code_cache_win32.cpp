#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <cstring>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::platform::win32
{
namespace
{

// Jump-table slots carry image-relative offsets; once the image bytes have a
// final absolute base the displacement and every table entry become absolute
// cache addresses. Targets missing from the image fall back to the slot's
// INT3 so the dispatcher re-executes the original guest branch.
void ResolveWin32AotJumpTables(const runtime::AotCodeCacheImage& image,
                               std::uint8_t* image_bytes,
                               std::uint32_t image_absolute_base)
{
    if (image.jump_table_sites.empty() || image_bytes == nullptr)
    {
        return;
    }
    std::unordered_map<std::uint32_t, std::uint32_t> guest_to_cache;
    guest_to_cache.reserve(image.address_map.size());
    for (const runtime::AotAddressMapEntry& entry : image.address_map)
    {
        guest_to_cache.emplace(entry.guest_address, entry.cache_offset);
    }
    for (const runtime::AotJumpTableSite& site : image.jump_table_sites)
    {
        const std::uint32_t table_address =
            image_absolute_base + site.table_cache_offset;
        std::memcpy(image_bytes + site.displacement_patch_offset,
                    &table_address, sizeof(table_address));
        for (std::size_t index = 0;
             index < site.guest_targets.size(); ++index)
        {
            const auto target =
                guest_to_cache.find(site.guest_targets[index]);
            const std::uint32_t entry_address = image_absolute_base +
                (target != guest_to_cache.end() ? target->second
                                                : site.fallback_offset);
            std::memcpy(image_bytes + site.table_cache_offset + index * 4U,
                        &entry_address, sizeof(entry_address));
        }
    }
}

}  // namespace

bool PlaceWin32AotCodeCache(const runtime::AotCodeCacheImage& image,
                            Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return false;
    }
    *placement = Win32AotCodeCachePlacement{};
    if (!image.valid || image.executable || image.bytes.empty())
    {
        placement->message = "AOT byte image is not ready for placement";
        return false;
    }
#if !defined(_WIN32)
    placement->message = "AOT code cache placement requires Win32";
    return false;
#else
    if (image.bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        placement->message = "AOT code cache exceeds Win32 address size";
        return false;
    }
    constexpr std::uint32_t kDynamicCacheCapacity = 16U * 1024U * 1024U;
    const std::size_t capacity = std::max<std::size_t>(
        image.bytes.size(), kDynamicCacheCapacity);
    void* memory = VirtualAlloc(nullptr, capacity,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    placement->valid = true;
    if (memory == nullptr)
    {
        placement->windows_error = GetLastError();
        placement->message = "VirtualAlloc for AOT code cache failed";
        return true;
    }
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(memory);
    if (base > std::numeric_limits<std::uint32_t>::max())
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "AOT code cache is outside the x86 address range";
        return true;
    }
    std::memcpy(memory, image.bytes.data(), image.bytes.size());
    ResolveWin32AotJumpTables(image, static_cast<std::uint8_t*>(memory),
                              static_cast<std::uint32_t>(base));
    DWORD old_protection = 0;
    if (VirtualProtect(memory, capacity, PAGE_EXECUTE_READ,
                       &old_protection) == 0)
    {
        placement->windows_error = GetLastError();
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "VirtualProtect for AOT code cache failed";
        return true;
    }
    FlushInstructionCache(GetCurrentProcess(), memory, image.bytes.size());
    placement->base_address = static_cast<std::uint32_t>(base);
    placement->size = static_cast<std::uint32_t>(image.bytes.size());
    placement->capacity = static_cast<std::uint32_t>(capacity);
    placement->entry_address = placement->base_address +
                               image.entry_cache_offset;
    placement->address_map = image.address_map;
    InitializeWin32AotPageCoherence(placement, 1U);
    placement->fixups = image.fixups;
    placement->indirect_inline_cache_sites =
        image.indirect_inline_cache_sites;
    placement->placed = true;
    placement->message = "AOT code cache placed as Win32 execute-read memory";
    return true;
#endif
}

bool AppendWin32DynamicAotTranslation(
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    Win32AotPageWriteWatchSet* write_watch_set,
    Win32AotCodeCachePlacement* placement,
    Win32AotDynamicAppendResult* result)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = Win32AotDynamicAppendResult{};
    result->attempted = true;
    result->guest_entry = guest_entry;
#if !defined(_WIN32)
    result->message = "dynamic AOT translation requires Win32";
    return true;
#else
    if (!placement->placed || runtime_size == 0U ||
        guest_entry < runtime_base || guest_entry - runtime_base >= runtime_size)
    {
        result->message = "dynamic AOT target is outside the guest arena";
        return true;
    }
    runtime::RelocatedRuntimeImage snapshot;
    snapshot.valid = true;
    snapshot.relocated_image_base = runtime_base;
    snapshot.relocated_entry_linear_address = guest_entry;
    runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = runtime_base;
    object.virtual_size = runtime_size;
    object.memory.resize(runtime_size);
    SIZE_T bytes_read = 0;
    if (ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(
                              static_cast<std::uintptr_t>(runtime_base)),
                          object.memory.data(), runtime_size,
                          &bytes_read) == 0 || bytes_read != runtime_size)
    {
        result->message = "failed to snapshot live guest arena";
        return true;
    }
    snapshot.objects.push_back(std::move(object));
    runtime::AotTranslationPlan plan;
    runtime::AotCodeCacheImage image;
    if (!runtime::BuildAotTranslationPlanFromEntry(
            snapshot, guest_entry, excluded_ranges, &plan) ||
        !runtime::BuildAotCodeCacheImage(plan, &image))
    {
        result->message = "failed to translate dynamic guest target";
        return true;
    }
    if (image.bytes.size() > placement->capacity - placement->size)
    {
        result->message = "dynamic AOT cache capacity is exhausted";
        return true;
    }
    const std::uint32_t append_offset = placement->size;
    const std::uint32_t requested_page = Win32AotGuestPage(guest_entry);
    std::vector<bool> image_active(image.address_map.size(), true);
    std::vector<bool> image_tracks_guest_bytes(
        image.address_map.size(), true);
    std::vector<std::uint32_t> candidate_active_pages;
    std::size_t entry_index = image.address_map.size();
    for (std::size_t index = 0; index < image.address_map.size(); ++index)
    {
        runtime::AotAddressMapEntry& entry = image.address_map[index];
        image_tracks_guest_bytes[index] =
            Win32AotAddressMapTracksGuestBytes(entry, excluded_ranges);
        if (!CanActivateWin32AotAddressMapEntry(
                *placement, entry, requested_page))
        {
            image_active[index] = false;
            image.bytes[entry.cache_offset] = 0xCCU;
        }
        if (image_active[index] && entry.guest_address == guest_entry)
        {
            entry_index = index;
        }
        if (!image_active[index] || !image_tracks_guest_bytes[index] ||
            entry.guest_length == 0U)
        {
            continue;
        }
        const std::uint64_t last =
            static_cast<std::uint64_t>(entry.guest_address) +
            entry.guest_length - 1U;
        if (last > std::numeric_limits<std::uint32_t>::max())
        {
            result->message = "dynamic AOT mapping crosses address space";
            return true;
        }
        const std::uint32_t first_page =
            Win32AotGuestPage(entry.guest_address);
        const std::uint32_t last_page = Win32AotGuestPage(
            static_cast<std::uint32_t>(last));
        for (std::uint32_t page = first_page;; page += 0x1000U)
        {
            const auto position = std::lower_bound(
                candidate_active_pages.begin(),
                candidate_active_pages.end(), page);
            if (position == candidate_active_pages.end() ||
                *position != page)
            {
                candidate_active_pages.insert(position, page);
            }
            if (page == last_page || page > 0xFFFFEFFFU)
            {
                break;
            }
        }
    }
    if (entry_index == image.address_map.size())
    {
        result->message = "dynamic AOT entry was not active in the new image";
        return true;
    }
    if (write_watch_set != nullptr &&
        !InstallWin32AotGuestPageWriteWatches(
            *placement, &candidate_active_pages, write_watch_set))
    {
        result->message =
            "failed to install write watches before AOT publication";
        return true;
    }
    const std::uint32_t generation =
        AllocateWin32AotGeneration(placement);
    DWORD old_protection = 0;
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        result->message = "failed to make AOT cache writable";
        return true;
    }
    std::memcpy(static_cast<std::uint8_t*>(cache) + append_offset,
                image.bytes.data(), image.bytes.size());
    ResolveWin32AotJumpTables(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        placement->base_address + append_offset);
    std::vector<std::uint32_t> relinked_cache_offsets;
    for (std::size_t image_index = 0;
         image_index < image.address_map.size(); ++image_index)
    {
        const runtime::AotAddressMapEntry& fresh =
            image.address_map[image_index];
        if (!image_active[image_index])
        {
            continue;
        }
        const auto inactive =
            placement->inactive_map_indices_by_guest_address.find(
                fresh.guest_address);
        if (inactive ==
            placement->inactive_map_indices_by_guest_address.end())
        {
            continue;
        }
        const std::uint32_t fresh_cache_offset =
            append_offset + fresh.cache_offset;
        for (std::uint32_t old_index : inactive->second)
        {
            if (old_index >= placement->address_map.size())
            {
                continue;
            }
            const runtime::AotAddressMapEntry& stale =
                placement->address_map[old_index];
            if (placement->address_map_states[old_index].active ||
                stale.emitted_length < 5U)
            {
                continue;
            }
            const std::int64_t relative =
                static_cast<std::int64_t>(fresh_cache_offset) -
                (static_cast<std::int64_t>(stale.cache_offset) + 5U);
            if (relative < std::numeric_limits<std::int32_t>::min() ||
                relative > std::numeric_limits<std::int32_t>::max())
            {
                continue;
            }
            auto* cache_bytes = static_cast<std::uint8_t*>(cache);
            cache_bytes[stale.cache_offset] = 0xE9U;
            const std::int32_t displacement =
                static_cast<std::int32_t>(relative);
            std::memcpy(cache_bytes + stale.cache_offset + 1U,
                        &displacement, sizeof(displacement));
            relinked_cache_offsets.push_back(stale.cache_offset);
            ++result->relinked_entry_count;
        }
    }
    DWORD ignored = 0;
    const bool protected_rx = VirtualProtect(
        cache, placement->capacity, PAGE_EXECUTE_READ, &ignored) != 0;
    FlushInstructionCache(
        GetCurrentProcess(),
        static_cast<std::uint8_t*>(cache) + append_offset,
        image.bytes.size());
    for (std::uint32_t cache_offset : relinked_cache_offsets)
    {
        FlushInstructionCache(
            GetCurrentProcess(),
            static_cast<std::uint8_t*>(cache) + cache_offset, 5U);
    }
    if (!protected_rx)
    {
        result->unsafe_failure = true;
        result->message = "failed to restore AOT cache execute protection";
        return true;
    }

    const std::size_t previous_map_count = placement->address_map.size();
    for (std::size_t image_index = 0;
         image_index < image.address_map.size(); ++image_index)
    {
        runtime::AotAddressMapEntry entry = image.address_map[image_index];
        entry.cache_offset += append_offset;
        placement->address_map.push_back(entry);
        const std::uint32_t map_index = static_cast<std::uint32_t>(
            previous_map_count + image_index);
        RegisterWin32AotAddressMap(
            placement, map_index, generation, image_active[image_index],
            image_tracks_guest_bytes[image_index], requested_page,
            &result->active_guest_pages);
    }
    for (runtime::AotCodeCacheFixup fixup : image.fixups)
    {
        fixup.cache_patch_offset += append_offset;
        placement->fixups.push_back(fixup);
    }
    for (runtime::AotIndirectInlineCacheSite site :
         image.indirect_inline_cache_sites)
    {
        site.cache_offset += append_offset;
        site.miss_cache_offset += append_offset;
        site.target_immediate_offset += append_offset;
        site.guard_offset += append_offset;
        site.jump_displacement_offset += append_offset;
        placement->indirect_inline_cache_sites.push_back(site);
    }
    placement->size += static_cast<std::uint32_t>(image.bytes.size());
    result->cache_entry = placement->base_address + append_offset +
                          image.address_map[entry_index].cache_offset;
    result->generation = generation;
    result->added_bytes = static_cast<std::uint32_t>(image.bytes.size());
    result->added_mappings = static_cast<std::uint32_t>(
        image.address_map.size());
    result->appended = true;
    result->message = "dynamic AOT translation appended";
    return true;
#endif
}

bool PatchWin32AotIndirectInlineCache(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t cache_miss_address,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    Win32AotInlineCachePatchResult* result)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = Win32AotInlineCachePatchResult{};
    result->attempted = true;
    result->cache_miss_address = cache_miss_address;
    result->guest_target = guest_target;
    result->cache_target = cache_target;
#if !defined(_WIN32)
    result->message = "AOT inline-cache patching requires Win32";
    return true;
#else
    if (!placement->placed ||
        cache_miss_address < placement->base_address)
    {
        result->message = "AOT inline-cache placement is unavailable";
        return true;
    }
    const std::uint32_t miss_offset =
        cache_miss_address - placement->base_address;
    const runtime::AotIndirectInlineCacheSite* selected = nullptr;
    for (const auto& site : placement->indirect_inline_cache_sites)
    {
        if (miss_offset == site.miss_cache_offset ||
            miss_offset == site.miss_cache_offset + 1U)
        {
            selected = &site;
            break;
        }
    }
    if (selected == nullptr ||
        selected->jump_displacement_offset + 4U > placement->size ||
        selected->target_immediate_offset + 4U > placement->size ||
        selected->guard_offset + 6U > placement->size)
    {
        result->message = "AOT inline-cache miss site was not found";
        return true;
    }
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    DWORD old_protection = 0;
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        result->windows_error = GetLastError();
        result->message = "failed to make AOT inline cache writable";
        return true;
    }
    auto* bytes = static_cast<std::uint8_t*>(cache);
    std::memcpy(bytes + selected->target_immediate_offset,
                &guest_target, sizeof(guest_target));
    const std::int64_t relative =
        static_cast<std::int64_t>(cache_target) -
        (static_cast<std::int64_t>(placement->base_address) +
         selected->jump_displacement_offset + 4U);
    if (relative < std::numeric_limits<std::int32_t>::min() ||
        relative > std::numeric_limits<std::int32_t>::max())
    {
        DWORD ignored = 0;
        VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored);
        result->message = "AOT inline-cache target is outside rel32 range";
        return true;
    }
    const std::int32_t displacement = static_cast<std::int32_t>(relative);
    std::memcpy(bytes + selected->jump_displacement_offset,
                &displacement, sizeof(displacement));
    const std::int32_t miss_displacement = static_cast<std::int32_t>(
        selected->miss_cache_offset - (selected->guard_offset + 6U));
    bytes[selected->guard_offset] = 0x0FU;
    bytes[selected->guard_offset + 1U] = 0x85U;
    std::memcpy(bytes + selected->guard_offset + 2U,
                &miss_displacement, sizeof(miss_displacement));
    DWORD ignored = 0;
    if (VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored) == 0)
    {
        result->windows_error = GetLastError();
        result->message = "failed to restore AOT inline-cache RX protection";
        return true;
    }
    FlushInstructionCache(GetCurrentProcess(),
                          bytes + selected->cache_offset,
                          selected->miss_cache_offset + 2U -
                              selected->cache_offset);
    result->patched = true;
    result->message = "AOT indirect inline cache patched";
    return true;
#endif
}

void ReleaseWin32AotCodeCache(Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
#if defined(_WIN32)
    if (placement->placed && placement->base_address != 0U)
    {
        VirtualFree(reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>(placement->base_address)),
                    0, MEM_RELEASE);
    }
#endif
    *placement = Win32AotCodeCachePlacement{};
}

bool FindAotGuestAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t cache_address,
                         std::uint32_t* guest_address)
{
    if (!placement.placed || guest_address == nullptr ||
        cache_address < placement.base_address)
    {
        return false;
    }
    const std::uint32_t offset = cache_address - placement.base_address;
    for (const runtime::AotAddressMapEntry& entry : placement.address_map)
    {
        if (offset >= entry.cache_offset &&
            offset < entry.cache_offset + entry.emitted_length)
        {
            *guest_address = entry.guest_address;
            return true;
        }
    }
    return false;
}

bool FindAotCacheAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t guest_address,
                         std::uint32_t* cache_address)
{
    if (!placement.placed || cache_address == nullptr)
    {
        return false;
    }
    if (placement.retired_guest_addresses.empty())
    {
        for (const runtime::AotAddressMapEntry& entry :
             placement.address_map)
        {
            if (entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
        return false;
    }
    const bool has_retired_generation = std::binary_search(
        placement.retired_guest_addresses.begin(),
        placement.retired_guest_addresses.end(), guest_address);
    if (has_retired_generation)
    {
        for (std::size_t index = placement.address_map.size();
             index-- > 0U;)
        {
            const runtime::AotAddressMapEntry& entry =
                placement.address_map[index];
            if (placement.address_map_states[index].active &&
                entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
    }
    else
    {
        for (std::size_t index = 0;
             index < placement.address_map.size(); ++index)
        {
            const runtime::AotAddressMapEntry& entry =
                placement.address_map[index];
            if (entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
    }
    return false;
}

bool InstallWin32AotProbeSentinel(Win32AotCodeCachePlacement* placement,
                                  std::uint32_t guest_address)
{
    if (placement == nullptr || !placement->placed)
    {
        return false;
    }
#if !defined(_WIN32)
    return false;
#else
    std::uint32_t cache_address = 0;
    if (!FindAotCacheAddress(*placement, guest_address, &cache_address))
    {
        return false;
    }
    DWORD old_protection = 0;
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        return false;
    }
    *reinterpret_cast<std::uint8_t*>(
        static_cast<std::uintptr_t>(cache_address)) = 0xCCU;
    DWORD ignored = 0;
    const bool restored = VirtualProtect(cache, placement->capacity,
                                         PAGE_EXECUTE_READ, &ignored) != 0;
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(
                              static_cast<std::uintptr_t>(cache_address)), 1);
    return restored;
#endif
}

}  // namespace repiu::platform::win32
