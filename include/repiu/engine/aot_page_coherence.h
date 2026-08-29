#ifndef REPIU_ENGINE_AOT_PAGE_COHERENCE_H_
#define REPIU_ENGINE_AOT_PAGE_COHERENCE_H_

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/platform/virtual_memory.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace repiu::engine
{

struct AotCodeCachePlacement;

struct AotGuestPageState
{
    std::uint32_t guest_page = 0;
    std::uint32_t latest_generation = 0;
    bool retired = false;
    bool quarantined = false;
    std::vector<std::uint32_t> map_indices;
};

struct AotAddressMapState
{
    std::uint32_t generation = 0;
    bool active = true;
    bool tracks_guest_bytes = true;
};

struct AotGuestPageRetireResult
{
    bool attempted = false;
    bool retired = false;
    bool quarantined = false;
    std::uint32_t guest_page = 0;
    std::uint32_t retired_entry_count = 0;
    std::uint32_t guard_reset_count = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

struct AotGuestPageWriteWatch
{
    std::uint32_t guest_page = 0;
    repiu::platform::MemoryProtection original_protection =
        repiu::platform::MemoryProtection::kNoAccess;
};

struct AotGuestWriteCompletion
{
    bool from_guest = false;
    bool keep_single_step = false;
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    std::uint32_t byte_count = 0;
};

struct AotPageWriteWatchSet
{
    static constexpr std::uint32_t kPendingPageCapacity = 16U;
    std::vector<AotGuestPageWriteWatch> watches;
    bool pending = false;
    bool pending_from_guest = false;
    bool pending_keep_single_step = false;
    std::array<std::uint32_t, kPendingPageCapacity> pending_pages = {};
    std::uint32_t pending_page_count = 0;
    std::uint32_t pending_source = 0;
    std::uint32_t pending_destination = 0;
    std::uint32_t pending_byte_count = 0;
};

std::uint32_t AotGuestPage(std::uint32_t guest_address);
void InitializeAotPageCoherence(
    AotCodeCachePlacement* placement,
    std::uint32_t initial_generation);
std::uint32_t AllocateAotGeneration(
    AotCodeCachePlacement* placement);
bool CanActivateAotAddressMapEntry(
    const AotCodeCachePlacement& placement,
    const runtime::AotAddressMapEntry& entry,
    std::uint32_t requested_page);
bool AotAddressMapTracksGuestBytes(
    const runtime::AotAddressMapEntry& entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges);
void RegisterAotAddressMap(
    AotCodeCachePlacement* placement,
    std::uint32_t map_index,
    std::uint32_t generation,
    bool active,
    bool tracks_guest_bytes,
    std::uint32_t requested_page,
    std::vector<std::uint32_t>* active_guest_pages);

bool AotGuestRangeHasActiveTranslation(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address,
    std::uint32_t byte_count);
bool QueryAotActiveGuestPageGeneration(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address,
    std::uint32_t* generation);
bool IsAotGuestPageRetired(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address);
bool IsAotGuestPageQuarantined(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address);
bool HasAotRetiredGuestAddress(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address);
bool IsAotCacheAddressRetired(
    const AotCodeCachePlacement& placement,
    std::uint32_t cache_address);
bool RetireAotGuestPage(
    AotCodeCachePlacement* placement,
    std::uint32_t guest_address,
    bool quarantine,
    AotGuestPageRetireResult* result);

bool InstallAotGuestPageWriteWatches(
    const AotCodeCachePlacement& placement,
    const std::vector<std::uint32_t>* selected_pages,
    AotPageWriteWatchSet* watch_set);
void RestoreAotGuestPageWriteWatches(
    AotPageWriteWatchSet* watch_set);
bool IsAotGuestPageWriteWatched(
    const AotPageWriteWatchSet& watch_set,
    std::uint32_t guest_address);
void RemoveAotPageWriteWatch(
    AotPageWriteWatchSet* watch_set,
    std::uint32_t guest_address);
bool HasPendingAotGuestWrite(
    const AotPageWriteWatchSet& watch_set);
bool BeginAotGuestWrite(
    AotPageWriteWatchSet* watch_set,
    std::uint32_t execution_address,
    std::uint32_t fault_address,
    bool from_guest,
    bool keep_single_step,
    std::uint32_t guest_source);
bool CompleteAotGuestWrite(
    AotPageWriteWatchSet* watch_set,
    AotGuestWriteCompletion* completion);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_AOT_PAGE_COHERENCE_H_
