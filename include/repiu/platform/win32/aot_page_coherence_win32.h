#ifndef REPIU_PLATFORM_WIN32_AOT_PAGE_COHERENCE_WIN32_H_
#define REPIU_PLATFORM_WIN32_AOT_PAGE_COHERENCE_WIN32_H_

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace repiu::platform::win32
{

struct Win32AotCodeCachePlacement;

struct Win32AotGuestPageState
{
    std::uint32_t guest_page = 0;
    std::uint32_t latest_generation = 0;
    bool retired = false;
    bool quarantined = false;
    std::vector<std::uint32_t> map_indices;
};

struct Win32AotAddressMapState
{
    std::uint32_t generation = 0;
    bool active = true;
    bool tracks_guest_bytes = true;
};

struct Win32AotGuestPageRetireResult
{
    bool attempted = false;
    bool retired = false;
    bool quarantined = false;
    std::uint32_t guest_page = 0;
    std::uint32_t retired_entry_count = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

struct Win32AotGuestPageWriteWatch
{
    std::uint32_t guest_page = 0;
    std::uint32_t original_protection = 0;
};

struct Win32AotGuestWriteCompletion
{
    bool from_guest = false;
    bool keep_single_step = false;
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    std::uint32_t byte_count = 0;
};

struct Win32AotPageWriteWatchSet
{
    static constexpr std::uint32_t kPendingPageCapacity = 16U;
    std::vector<Win32AotGuestPageWriteWatch> watches;
    bool pending = false;
    bool pending_from_guest = false;
    bool pending_keep_single_step = false;
    std::array<std::uint32_t, kPendingPageCapacity> pending_pages = {};
    std::uint32_t pending_page_count = 0;
    std::uint32_t pending_source = 0;
    std::uint32_t pending_destination = 0;
    std::uint32_t pending_byte_count = 0;
};

std::uint32_t Win32AotGuestPage(std::uint32_t guest_address);
void InitializeWin32AotPageCoherence(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t initial_generation);
std::uint32_t AllocateWin32AotGeneration(
    Win32AotCodeCachePlacement* placement);
bool CanActivateWin32AotAddressMapEntry(
    const Win32AotCodeCachePlacement& placement,
    const runtime::AotAddressMapEntry& entry,
    std::uint32_t requested_page);
bool Win32AotAddressMapTracksGuestBytes(
    const runtime::AotAddressMapEntry& entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges);
void RegisterWin32AotAddressMap(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t map_index,
    std::uint32_t generation,
    bool active,
    bool tracks_guest_bytes,
    std::uint32_t requested_page,
    std::vector<std::uint32_t>* active_guest_pages);

bool Win32AotGuestRangeHasActiveTranslation(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t guest_address,
    std::uint32_t byte_count);
bool IsWin32AotGuestPageRetired(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t guest_address);
bool IsWin32AotGuestPageQuarantined(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t guest_address);
bool HasWin32AotRetiredGuestAddress(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t guest_address);
bool IsWin32AotCacheAddressRetired(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t cache_address);
bool RetireWin32AotGuestPage(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t guest_address,
    bool quarantine,
    Win32AotGuestPageRetireResult* result);

bool InstallWin32AotGuestPageWriteWatches(
    const Win32AotCodeCachePlacement& placement,
    const std::vector<std::uint32_t>* selected_pages,
    Win32AotPageWriteWatchSet* watch_set);
void RestoreWin32AotGuestPageWriteWatches(
    Win32AotPageWriteWatchSet* watch_set);
bool IsWin32AotGuestPageWriteWatched(
    const Win32AotPageWriteWatchSet& watch_set,
    std::uint32_t guest_address);
void RemoveWin32AotPageWriteWatch(
    Win32AotPageWriteWatchSet* watch_set,
    std::uint32_t guest_address);
bool HasPendingWin32AotGuestWrite(
    const Win32AotPageWriteWatchSet& watch_set);
bool BeginWin32AotGuestWrite(
    Win32AotPageWriteWatchSet* watch_set,
    std::uint32_t execution_address,
    std::uint32_t fault_address,
    bool from_guest,
    bool keep_single_step,
    std::uint32_t guest_source);
bool CompleteWin32AotGuestWrite(
    Win32AotPageWriteWatchSet* watch_set,
    Win32AotGuestWriteCompletion* completion);

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_AOT_PAGE_COHERENCE_WIN32_H_
