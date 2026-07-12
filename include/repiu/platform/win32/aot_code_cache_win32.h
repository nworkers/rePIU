#ifndef REPIU_PLATFORM_WIN32_AOT_CODE_CACHE_WIN32_H_
#define REPIU_PLATFORM_WIN32_AOT_CODE_CACHE_WIN32_H_

#include "repiu/platform/win32/aot_page_coherence_win32.h"
#include "repiu/runtime/aot_code_cache.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace repiu::platform::win32
{

struct Win32AotCodeCachePlacement
{
    bool valid = false;
    bool placed = false;
    std::uint32_t base_address = 0;
    std::uint32_t size = 0;
    std::uint32_t capacity = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t windows_error = 0;
    std::vector<runtime::AotAddressMapEntry> address_map;
    std::vector<Win32AotAddressMapState> address_map_states;
    std::vector<runtime::AotCodeCacheFixup> fixups;
    std::vector<runtime::AotIndirectInlineCacheSite>
        indirect_inline_cache_sites;
    std::vector<Win32AotGuestPageState> guest_pages;
    std::vector<std::uint32_t> retired_guest_addresses;
    std::vector<std::uint32_t> inactive_map_indices;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>
        inactive_map_indices_by_guest_address;
    std::unordered_map<std::uint32_t, std::uint32_t>
        inactive_map_index_by_cache_offset;
    std::uint32_t next_generation = 1;
    std::string message;
};

struct Win32AotDynamicAppendResult
{
    bool attempted = false;
    bool appended = false;
    bool unsafe_failure = false;
    std::uint32_t guest_entry = 0;
    std::uint32_t cache_entry = 0;
    std::uint32_t generation = 0;
    std::uint32_t added_bytes = 0;
    std::uint32_t added_mappings = 0;
    std::uint32_t relinked_entry_count = 0;
    std::vector<std::uint32_t> active_guest_pages;
    std::string message;
};

struct Win32AotInlineCachePatchResult
{
    bool attempted = false;
    bool patched = false;
    std::uint32_t cache_miss_address = 0;
    std::uint32_t guest_target = 0;
    std::uint32_t cache_target = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

bool PlaceWin32AotCodeCache(const runtime::AotCodeCacheImage& image,
                            Win32AotCodeCachePlacement* placement);
void ReleaseWin32AotCodeCache(Win32AotCodeCachePlacement* placement);
bool FindAotGuestAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t cache_address,
                         std::uint32_t* guest_address);
bool FindAotCacheAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t guest_address,
                         std::uint32_t* cache_address);
bool InstallWin32AotProbeSentinel(Win32AotCodeCachePlacement* placement,
                                  std::uint32_t guest_address);
bool AppendWin32DynamicAotTranslation(
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    Win32AotPageWriteWatchSet* write_watch_set,
    Win32AotCodeCachePlacement* placement,
    Win32AotDynamicAppendResult* result);
bool PatchWin32AotIndirectInlineCache(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t cache_miss_address,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    Win32AotInlineCachePatchResult* result);
}  // namespace repiu::platform::win32

#endif
