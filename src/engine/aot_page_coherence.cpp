#include "repiu/engine/aot_page_coherence.h"
#include "repiu/engine/aot_code_cache.h"
#include "repiu/platform/atomic_ops.h"
#include "repiu/platform/virtual_memory.h"

#include <Zydis.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <limits>

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kGuestPageSize = 4096U;
constexpr std::uint32_t kGuestPageMask = ~(kGuestPageSize - 1U);

auto FindGuestPageState(AotCodeCachePlacement* placement,
                        std::uint32_t guest_page)
{
    return std::lower_bound(
        placement->guest_pages.begin(), placement->guest_pages.end(),
        guest_page,
        [](const AotGuestPageState& state, std::uint32_t page) {
            return state.guest_page < page;
        });
}

auto FindGuestPageState(const AotCodeCachePlacement& placement,
                        std::uint32_t guest_page)
{
    return std::lower_bound(
        placement.guest_pages.begin(), placement.guest_pages.end(),
        guest_page,
        [](const AotGuestPageState& state, std::uint32_t page) {
            return state.guest_page < page;
        });
}

AotGuestPageState* EnsureGuestPageState(
    AotCodeCachePlacement* placement,
    std::uint32_t guest_page)
{
    auto found = FindGuestPageState(placement, guest_page);
    if (found == placement->guest_pages.end() ||
        found->guest_page != guest_page)
    {
        found = placement->guest_pages.insert(
            found, AotGuestPageState{guest_page});
    }
    return &*found;
}

bool RangesOverlap(std::uint32_t left_address,
                   std::uint32_t left_size,
                   std::uint32_t right_address,
                   std::uint32_t right_size)
{
    const std::uint64_t left_end =
        static_cast<std::uint64_t>(left_address) + left_size;
    const std::uint64_t right_end =
        static_cast<std::uint64_t>(right_address) + right_size;
    return left_address < right_end && right_address < left_end;
}

void InsertRetiredGuestAddress(AotCodeCachePlacement* placement,
                               std::uint32_t guest_address)
{
    auto address = std::lower_bound(
        placement->retired_guest_addresses.begin(),
        placement->retired_guest_addresses.end(), guest_address);
    if (address == placement->retired_guest_addresses.end() ||
        *address != guest_address)
    {
        placement->retired_guest_addresses.insert(address, guest_address);
    }
}

void RecordInactiveMap(AotCodeCachePlacement* placement,
                       std::uint32_t map_index)
{
    if (placement == nullptr || map_index >= placement->address_map.size())
    {
        return;
    }
    placement->inactive_map_indices.push_back(map_index);
    const runtime::AotAddressMapEntry& entry =
        placement->address_map[map_index];
    placement->inactive_map_indices_by_guest_address[entry.guest_address]
        .push_back(map_index);
    placement->inactive_map_index_by_cache_offset[entry.cache_offset] =
        map_index;
    InsertRetiredGuestAddress(placement, entry.guest_address);
}

void InsertGuestPage(std::uint32_t page,
                     std::vector<std::uint32_t>* pages)
{
    if (pages == nullptr)
    {
        return;
    }
    const auto position = std::lower_bound(pages->begin(), pages->end(), page);
    if (position == pages->end() || *position != page)
    {
        pages->insert(position, page);
    }
}

void RegisterAddressMapPages(AotCodeCachePlacement* placement,
                             const runtime::AotAddressMapEntry& entry,
                             std::uint32_t map_index,
                             std::vector<std::uint32_t>* active_guest_pages)
{
    if (entry.guest_length == 0U)
    {
        return;
    }
    const std::uint64_t last =
        static_cast<std::uint64_t>(entry.guest_address) +
        entry.guest_length - 1U;
    if (last > std::numeric_limits<std::uint32_t>::max())
    {
        return;
    }
    const std::uint32_t first_page = AotGuestPage(entry.guest_address);
    const std::uint32_t last_page = AotGuestPage(
        static_cast<std::uint32_t>(last));
    for (std::uint32_t page = first_page;; page += kGuestPageSize)
    {
        AotGuestPageState* state =
            EnsureGuestPageState(placement, page);
        state->map_indices.push_back(map_index);
        const AotAddressMapState& map_state =
            placement->address_map_states[map_index];
        if (map_state.active)
        {
            state->latest_generation = std::max(
                state->latest_generation, map_state.generation);
            InsertGuestPage(page, active_guest_pages);
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
}

struct DecodedGuestWrite
{
    std::uint32_t byte_count = 1U;
    bool conservative_page = true;
};

DecodedGuestWrite DecodeGuestWrite(std::uint32_t execution_address)
{
    DecodedGuestWrite result;
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
            ZYDIS_STACK_WIDTH_32)))
    {
        return result;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(execution_address)),
            ZYDIS_MAX_INSTRUCTION_LENGTH,
            &instruction,
            operands)))
    {
        return result;
    }
    std::uint32_t write_operand_count = 0U;
    for (std::uint8_t index = 0;
         index < instruction.operand_count; ++index)
    {
        if (operands[index].type == ZYDIS_OPERAND_TYPE_MEMORY &&
            (operands[index].actions &
             ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0U)
        {
            ++write_operand_count;
            result.byte_count = std::max<std::uint32_t>(
                result.byte_count,
                (static_cast<std::uint32_t>(operands[index].size) + 7U) /
                    8U);
        }
    }
    const bool repeated =
        (instruction.attributes &
         (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE |
          ZYDIS_ATTRIB_HAS_REPNE)) != 0U;
    result.conservative_page = write_operand_count != 1U || repeated ||
        instruction.meta.category == ZYDIS_CATEGORY_STRINGOP ||
        instruction.mnemonic == ZYDIS_MNEMONIC_ENTER ||
        instruction.mnemonic == ZYDIS_MNEMONIC_PUSHA ||
        instruction.mnemonic == ZYDIS_MNEMONIC_PUSHAD;
    return result;
}

// A learned inline-cache guard whose guest target lies on the retired page
// must fall back to its miss tail (design 238): the retired entry start is
// now INT3, so a surviving hit would trap on every transfer without ever
// reaching the miss tail that drives repatching. Restores the initial
// `E9 rel32 -> miss` + `90` form, keeping the immediate and displacement
// for the dispatcher's normal repatch protocol. Caller holds the cache
// writable and flushes the returned offsets afterwards.
std::uint32_t ResetInlineCacheGuardsTargetingPage(
    AotCodeCachePlacement* placement,
    std::uint8_t* bytes,
    std::uint32_t guest_page,
    std::vector<std::uint32_t>* reset_guard_offsets)
{
    std::uint32_t reset_count = 0;
    for (runtime::AotIndirectInlineCacheSite& site :
         placement->indirect_inline_cache_sites)
    {
        const std::size_t slot_count =
            site.entries.empty() ? 1U : site.entries.size();
        for (std::size_t index = 0; index < slot_count; ++index)
        {
            const std::uint32_t guard_offset = site.entries.empty()
                ? site.guard_offset : site.entries[index].guard_offset;
            const std::uint32_t immediate_offset = site.entries.empty()
                ? site.target_immediate_offset
                : site.entries[index].target_immediate_offset;
            // Task 499: a site carrying a direct-return probe must keep
            // reaching the probe first, so the reset target is the guard target
            // rather than the miss tail. Without a probe the two are the same.
            const std::uint32_t guard_target =
                runtime::AotInlineCacheGuardTargetOffset(site);
            if (guard_offset + 6U > placement->size ||
                immediate_offset + 4U > placement->size ||
                guard_target >= placement->size)
            {
                continue;
            }
            if (bytes[guard_offset] != 0x0FU ||
                bytes[guard_offset + 1U] != 0x85U)
            {
                continue;
            }
            std::uint32_t guest_target = 0;
            std::memcpy(&guest_target, bytes + immediate_offset,
                        sizeof(guest_target));
            if ((guest_target & kGuestPageMask) != guest_page)
            {
                continue;
            }
            const std::int32_t miss_displacement =
                static_cast<std::int32_t>(guard_target) -
                (static_cast<std::int32_t>(guard_offset) + 5);
            bytes[guard_offset] = 0xE9U;
            std::memcpy(bytes + guard_offset + 1U, &miss_displacement,
                        sizeof(miss_displacement));
            bytes[guard_offset + 5U] = 0x90U;
            if (reset_guard_offsets != nullptr)
            {
                reset_guard_offsets->push_back(guard_offset);
            }
            ++reset_count;
        }
    }
    return reset_count;
}

auto FindWriteWatch(AotPageWriteWatchSet* watch_set,
                    std::uint32_t guest_page)
{
    return std::lower_bound(
        watch_set->watches.begin(), watch_set->watches.end(), guest_page,
        [](const AotGuestPageWriteWatch& watch, std::uint32_t page) {
            return watch.guest_page < page;
        });
}

bool EnsureWriteWatch(AotPageWriteWatchSet* watch_set,
                      std::uint32_t guest_address)
{
    const std::uint32_t page = AotGuestPage(guest_address);
    auto found = FindWriteWatch(watch_set, page);
    if (found != watch_set->watches.end() && found->guest_page == page)
    {
        return true;
    }
    const repiu::platform::MemoryRegion memory =
        repiu::platform::QueryMemory(reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(page)));
    if (!memory.valid || !memory.committed)
    {
        return false;
    }
    const repiu::platform::MemoryProtection original = memory.protection;
    if (original != repiu::platform::MemoryProtection::kExecuteRead)
    {
        if (!repiu::platform::ProtectMemory(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(page)),
                kGuestPageSize,
                repiu::platform::MemoryProtection::kExecuteRead, nullptr))
        {
            return false;
        }
    }
    watch_set->watches.insert(found, {page, original});
    return true;
}

void ResetPendingWrite(AotPageWriteWatchSet* watch_set)
{
    watch_set->pending = false;
    watch_set->pending_from_guest = false;
    watch_set->pending_keep_single_step = false;
    watch_set->pending_page_count = 0U;
    watch_set->pending_source = 0U;
    watch_set->pending_destination = 0U;
    watch_set->pending_byte_count = 0U;
}

bool PendingContainsPage(const AotPageWriteWatchSet& watch_set,
                         std::uint32_t page)
{
    for (std::uint32_t index = 0;
         index < watch_set.pending_page_count; ++index)
    {
        if (watch_set.pending_pages[index] == page)
        {
            return true;
        }
    }
    return false;
}

void RestorePendingPagesToExecuteRead(
    AotPageWriteWatchSet* watch_set)
{
    for (std::uint32_t index = 0;
         index < watch_set->pending_page_count; ++index)
    {
        repiu::platform::ProtectMemory(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                watch_set->pending_pages[index])),
            kGuestPageSize,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
    }
}

}  // namespace

std::uint32_t AotGuestPage(std::uint32_t guest_address)
{
    return guest_address & kGuestPageMask;
}

void InitializeAotPageCoherence(
    AotCodeCachePlacement* placement,
    std::uint32_t initial_generation)
{
    if (placement == nullptr)
    {
        return;
    }
    // Task 324: drop the index before re-registering. The loop below links each
    // entry incrementally, and the trailing Ensure covers any shape the
    // incremental path declined.
    InvalidateAotCacheAddressIndex(placement);
    placement->address_map_states.clear();
    placement->guest_pages.clear();
    placement->retired_guest_addresses.clear();
    placement->inactive_map_indices.clear();
    placement->inactive_map_indices_by_guest_address.clear();
    placement->inactive_map_index_by_cache_offset.clear();
    placement->next_generation = initial_generation + 1U;
    for (std::uint32_t index = 0;
         index < placement->address_map.size(); ++index)
    {
        RegisterAotAddressMap(
            placement, index, initial_generation, true, true,
            AotGuestPage(placement->address_map[index].guest_address),
            nullptr);
    }
    EnsureAotCacheAddressIndex(placement);
}

std::uint32_t AllocateAotGeneration(
    AotCodeCachePlacement* placement)
{
    return placement != nullptr ? placement->next_generation++ : 0U;
}

bool CanActivateAotAddressMapEntry(
    const AotCodeCachePlacement& placement,
    const runtime::AotAddressMapEntry& entry,
    std::uint32_t requested_page)
{
    if (entry.guest_length == 0U)
    {
        return false;
    }
    const std::uint64_t last =
        static_cast<std::uint64_t>(entry.guest_address) +
        entry.guest_length - 1U;
    if (last > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t first_page = AotGuestPage(entry.guest_address);
    const std::uint32_t last_page = AotGuestPage(
        static_cast<std::uint32_t>(last));
    for (std::uint32_t page = first_page;; page += kGuestPageSize)
    {
        const auto state = FindGuestPageState(placement, page);
        if (state != placement.guest_pages.end() &&
            state->guest_page == page &&
            (state->quarantined ||
             (state->retired && page != requested_page)))
        {
            return false;
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    return true;
}

bool AotAddressMapTracksGuestBytes(
    const runtime::AotAddressMapEntry& entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges)
{
    for (const runtime::AotExcludedGuestRange& range : excluded_ranges)
    {
        if (range.byte_count != 0U &&
            RangesOverlap(entry.guest_address, entry.guest_length,
                          range.guest_address, range.byte_count))
        {
            return false;
        }
    }
    return true;
}

void RegisterAotAddressMap(
    AotCodeCachePlacement* placement,
    std::uint32_t map_index,
    std::uint32_t generation,
    bool active,
    bool tracks_guest_bytes,
    std::uint32_t requested_page,
    std::vector<std::uint32_t>* active_guest_pages)
{
    if (placement == nullptr || map_index >= placement->address_map.size())
    {
        return;
    }
    if (placement->address_map_states.size() <= map_index)
    {
        placement->address_map_states.resize(map_index + 1U);
    }
    placement->address_map_states[map_index] = {
        generation, active, tracks_guest_bytes};
    const runtime::AotAddressMapEntry& entry =
        placement->address_map[map_index];
    if (tracks_guest_bytes)
    {
        RegisterAddressMapPages(
            placement, entry, map_index,
            active ? active_guest_pages : nullptr);
        if (active && AotGuestPage(entry.guest_address) == requested_page)
        {
            AotGuestPageState* state =
                EnsureGuestPageState(placement, requested_page);
            state->retired = false;
            state->latest_generation = generation;
        }
    }
    if (!active)
    {
        RecordInactiveMap(placement, map_index);
    }
    // Task 324: the canonical hook for "one address_map entry became known".
    // Only the structure is indexed; the active flag is read at query time, so
    // later activation changes need no index update.
    AppendAotCacheAddressIndexEntry(placement, map_index);
}

bool AotGuestRangeHasActiveTranslation(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address,
    std::uint32_t byte_count)
{
    if (!placement.placed || byte_count == 0U)
    {
        return false;
    }
    const std::uint64_t end =
        static_cast<std::uint64_t>(guest_address) + byte_count;
    if (end > static_cast<std::uint64_t>(
                  std::numeric_limits<std::uint32_t>::max()) + 1U)
    {
        return false;
    }
    const std::uint32_t first_page = AotGuestPage(guest_address);
    const std::uint32_t last_page = AotGuestPage(
        static_cast<std::uint32_t>(end - 1U));
    for (std::uint32_t page = first_page;; page += kGuestPageSize)
    {
        const auto state = FindGuestPageState(placement, page);
        if (state != placement.guest_pages.end() &&
            state->guest_page == page)
        {
            for (std::uint32_t index : state->map_indices)
            {
                if (index >= placement.address_map.size())
                {
                    continue;
                }
                const AotAddressMapState& map_state =
                    placement.address_map_states[index];
                const runtime::AotAddressMapEntry& entry =
                    placement.address_map[index];
                if (map_state.active && map_state.tracks_guest_bytes &&
                    RangesOverlap(guest_address, byte_count,
                                  entry.guest_address,
                                  entry.guest_length))
                {
                    return true;
                }
            }
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    return false;
}

bool QueryAotActiveGuestPageGeneration(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address,
    std::uint32_t* generation)
{
    if (!placement.placed || generation == nullptr)
    {
        return false;
    }
    const std::uint32_t page = AotGuestPage(guest_address);
    const auto state = FindGuestPageState(placement, page);
    if (state == placement.guest_pages.end() ||
        state->guest_page != page ||
        state->latest_generation == 0U ||
        state->retired ||
        state->quarantined)
    {
        return false;
    }
    *generation = state->latest_generation;
    return true;
}

bool IsAotGuestPageRetired(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address)
{
    const std::uint32_t page = AotGuestPage(guest_address);
    const auto state = FindGuestPageState(placement, page);
    return state != placement.guest_pages.end() &&
           state->guest_page == page && state->retired;
}

bool IsAotGuestPageQuarantined(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address)
{
    const std::uint32_t page = AotGuestPage(guest_address);
    const auto state = FindGuestPageState(placement, page);
    return state != placement.guest_pages.end() &&
           state->guest_page == page && state->quarantined;
}

bool HasAotRetiredGuestAddress(
    const AotCodeCachePlacement& placement,
    std::uint32_t guest_address)
{
    return std::binary_search(
        placement.retired_guest_addresses.begin(),
        placement.retired_guest_addresses.end(), guest_address);
}

bool IsAotCacheAddressRetired(
    const AotCodeCachePlacement& placement,
    std::uint32_t cache_address)
{
    if (!placement.placed || cache_address < placement.base_address)
    {
        return false;
    }
    const std::uint32_t offset = cache_address - placement.base_address;
    const auto found =
        placement.inactive_map_index_by_cache_offset.find(offset);
    return found != placement.inactive_map_index_by_cache_offset.end() &&
        found->second < placement.address_map_states.size() &&
        !placement.address_map_states[found->second].active;
}

bool RetireAotGuestPage(
    AotCodeCachePlacement* placement,
    std::uint32_t guest_address,
    bool quarantine,
    AotGuestPageRetireResult* result)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = AotGuestPageRetireResult{};
    result->attempted = true;
    result->guest_page = AotGuestPage(guest_address);
    result->quarantined = quarantine;
    if (!placement->placed)
    {
        result->message = "AOT code cache placement is unavailable";
        return true;
    }
    auto state_position = FindGuestPageState(
        placement, result->guest_page);
    if (state_position == placement->guest_pages.end() ||
        state_position->guest_page != result->guest_page)
    {
        result->message = "guest page has no AOT provenance";
        return true;
    }
    std::vector<std::uint32_t> selected;
    for (std::uint32_t map_index : state_position->map_indices)
    {
        if (map_index >= placement->address_map.size())
        {
            continue;
        }
        const runtime::AotAddressMapEntry& entry =
            placement->address_map[map_index];
        const AotAddressMapState& map_state =
            placement->address_map_states[map_index];
        if (map_state.active && map_state.tracks_guest_bytes &&
            RangesOverlap(result->guest_page, kGuestPageSize,
                          entry.guest_address, entry.guest_length))
        {
            selected.push_back(map_index);
        }
    }
    if (selected.empty())
    {
        state_position->retired = true;
        state_position->quarantined =
            state_position->quarantined || quarantine;
        result->retired = true;
        result->quarantined = state_position->quarantined;
        result->message = quarantine
            ? "AOT guest page quarantined without active entries"
            : "AOT guest page was already retired";
        return true;
    }
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (!repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kReadWrite, nullptr))
    {
        result->windows_error = static_cast<std::uint32_t>(errno);
        result->message = "failed to make AOT cache writable for retirement";
        return true;
    }
    auto* bytes = static_cast<std::uint8_t*>(cache);
    for (std::uint32_t index : selected)
    {
        bytes[placement->address_map[index].cache_offset] = 0xCCU;
    }
    std::vector<std::uint32_t> reset_guard_offsets;
    result->guard_reset_count = ResetInlineCacheGuardsTargetingPage(
        placement, bytes, result->guest_page, &reset_guard_offsets);
    // Task 499. This is the one point where a guest-to-cache mapping can
    // change, and the guest thread is blocked in
    // RequestAotGuestPageRetirement for the duration, so clearing the memo
    // table here needs no lock and no generation stamp. Clearing everything
    // rather than the affected range keeps the rule one sentence long, and
    // retirements are rare.
    runtime::ClearAotDirectReturnTable(&placement->direct_return_table);
    if (!repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr))
    {
        result->windows_error = static_cast<std::uint32_t>(errno);
        result->message = "failed to restore AOT cache after retirement";
        return true;
    }
    for (std::uint32_t index : selected)
    {
        repiu::platform::FlushInstructionCacheRange(
            bytes + placement->address_map[index].cache_offset, 1U);
        placement->address_map_states[index].active = false;
        RecordInactiveMap(placement, index);
    }
    for (std::uint32_t guard_offset : reset_guard_offsets)
    {
        repiu::platform::FlushInstructionCacheRange(
            bytes + guard_offset, 6U);
    }
    if (result->guard_reset_count != 0U)
    {
        static long guard_reset_events = 0;
        const long event_index = repiu::platform::AtomicIncrement(&guard_reset_events);
        if (event_index <= 16 || (event_index & 0xFFF) == 0)
        {
            fprintf(stderr,
                    "[repiu-live-debug] retire guard reset #%ld"
                    " page=0x%08X guards=%u entries=%u\n",
                    event_index, result->guest_page,
                    result->guard_reset_count,
                    static_cast<unsigned>(selected.size()));
        }
    }
    state_position = FindGuestPageState(placement, result->guest_page);
    state_position->retired = true;
    state_position->quarantined =
        state_position->quarantined || quarantine;
    result->retired_entry_count =
        static_cast<std::uint32_t>(selected.size());
    result->retired = true;
    result->quarantined = state_position->quarantined;
    result->message = quarantine
        ? "AOT guest page retired and quarantined"
        : "AOT guest page retired for a new generation";
    return true;
}

bool InstallAotGuestPageWriteWatches(
    const AotCodeCachePlacement& placement,
    const std::vector<std::uint32_t>* selected_pages,
    AotPageWriteWatchSet* watch_set)
{
    if (watch_set == nullptr)
    {
        return false;
    }
    if (selected_pages != nullptr)
    {
        for (std::uint32_t page : *selected_pages)
        {
            if (!EnsureWriteWatch(watch_set, page))
            {
                return false;
            }
        }
        return true;
    }
    watch_set->watches.reserve(placement.guest_pages.size());
    for (const AotGuestPageState& page : placement.guest_pages)
    {
        if (!EnsureWriteWatch(watch_set, page.guest_page))
        {
            return false;
        }
    }
    return true;
}

void RestoreAotGuestPageWriteWatches(
    AotPageWriteWatchSet* watch_set)
{
    if (watch_set == nullptr)
    {
        return;
    }
    for (const AotGuestPageWriteWatch& watch : watch_set->watches)
    {
        repiu::platform::ProtectMemory(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                watch.guest_page)),
            kGuestPageSize, watch.original_protection, nullptr);
    }
    watch_set->watches.clear();
    watch_set->pending = false;
    watch_set->pending_page_count = 0U;
}

bool IsAotGuestPageWriteWatched(
    const AotPageWriteWatchSet& watch_set,
    std::uint32_t guest_address)
{
    const std::uint32_t guest_page = AotGuestPage(guest_address);
    const auto watch = std::lower_bound(
        watch_set.watches.begin(), watch_set.watches.end(), guest_page,
        [](const AotGuestPageWriteWatch& value,
           std::uint32_t page) {
            return value.guest_page < page;
        });
    return watch != watch_set.watches.end() &&
           watch->guest_page == guest_page;
}

void RemoveAotPageWriteWatch(
    AotPageWriteWatchSet* watch_set,
    std::uint32_t guest_address)
{
    if (watch_set == nullptr)
    {
        return;
    }
    const std::uint32_t guest_page = AotGuestPage(guest_address);
    auto watch = std::lower_bound(
        watch_set->watches.begin(), watch_set->watches.end(), guest_page,
        [](const AotGuestPageWriteWatch& value, std::uint32_t page) {
            return value.guest_page < page;
        });
    if (watch != watch_set->watches.end() && watch->guest_page == guest_page)
    {
        repiu::platform::ProtectMemory(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                watch->guest_page)),
            kGuestPageSize,
            repiu::platform::MemoryProtection::kExecuteReadWrite, nullptr);
        watch_set->watches.erase(watch);
    }
}

bool HasPendingAotGuestWrite(
    const AotPageWriteWatchSet& watch_set)
{
    return watch_set.pending;
}

bool BeginAotGuestWrite(
    AotPageWriteWatchSet* watch_set,
    std::uint32_t execution_address,
    std::uint32_t fault_address,
    bool from_guest,
    bool keep_single_step,
    std::uint32_t guest_source)
{
    if (watch_set == nullptr)
    {
        return false;
    }
    const std::uint32_t fault_page = AotGuestPage(fault_address);
    const auto fault_watch = FindWriteWatch(watch_set, fault_page);
    if (fault_watch == watch_set->watches.end() ||
        fault_watch->guest_page != fault_page)
    {
        return false;
    }
    const DecodedGuestWrite decoded = DecodeGuestWrite(execution_address);
    const std::uint32_t range_begin = decoded.conservative_page
        ? fault_page : fault_address;
    const std::uint32_t range_size = decoded.conservative_page
        ? kGuestPageSize : decoded.byte_count;
    const std::uint64_t range_end =
        static_cast<std::uint64_t>(range_begin) + range_size;
    if (range_end > static_cast<std::uint64_t>(
                        std::numeric_limits<std::uint32_t>::max()) + 1U)
    {
        return false;
    }

    if (!watch_set->pending)
    {
        watch_set->pending = true;
        watch_set->pending_from_guest = from_guest;
        watch_set->pending_keep_single_step = keep_single_step;
        watch_set->pending_source = guest_source;
        watch_set->pending_destination = range_begin;
        watch_set->pending_byte_count = range_size;
    }
    else
    {
        const std::uint32_t combined_begin = std::min(
            watch_set->pending_destination, range_begin);
        const std::uint64_t current_end =
            static_cast<std::uint64_t>(
                watch_set->pending_destination) +
            watch_set->pending_byte_count;
        const std::uint64_t combined_end = std::max(current_end, range_end);
        watch_set->pending_destination = combined_begin;
        watch_set->pending_byte_count = static_cast<std::uint32_t>(
            combined_end - combined_begin);
        watch_set->pending_keep_single_step =
            watch_set->pending_keep_single_step || keep_single_step;
        if (watch_set->pending_source == 0U)
        {
            watch_set->pending_source = guest_source;
        }
    }

    const std::uint32_t first_page = AotGuestPage(range_begin);
    const std::uint32_t last_page = AotGuestPage(
        static_cast<std::uint32_t>(range_end - 1U));
    for (std::uint32_t page = first_page;; page += kGuestPageSize)
    {
        const auto watch = FindWriteWatch(watch_set, page);
        if (watch != watch_set->watches.end() &&
            watch->guest_page == page &&
            !PendingContainsPage(*watch_set, page))
        {
            if (watch_set->pending_page_count >=
                AotPageWriteWatchSet::kPendingPageCapacity)
            {
                RestorePendingPagesToExecuteRead(watch_set);
                ResetPendingWrite(watch_set);
                return false;
            }
            if (!repiu::platform::ProtectMemory(
                    reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>(page)),
                    kGuestPageSize,
                    repiu::platform::MemoryProtection::kExecuteReadWrite,
                    nullptr))
            {
                RestorePendingPagesToExecuteRead(watch_set);
                ResetPendingWrite(watch_set);
                return false;
            }
            watch_set->pending_pages[watch_set->pending_page_count++] = page;
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    return watch_set->pending_page_count != 0U;
}

bool CompleteAotGuestWrite(
    AotPageWriteWatchSet* watch_set,
    AotGuestWriteCompletion* completion)
{
    if (watch_set == nullptr || completion == nullptr ||
        !watch_set->pending || watch_set->pending_page_count == 0U)
    {
        return false;
    }
    bool restored = true;
    for (std::uint32_t index = 0;
         index < watch_set->pending_page_count; ++index)
    {
        restored = repiu::platform::ProtectMemory(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                watch_set->pending_pages[index])),
            kGuestPageSize,
            repiu::platform::MemoryProtection::kExecuteRead,
            nullptr) && restored;
    }
    if (!restored)
    {
        return false;
    }
    completion->from_guest = watch_set->pending_from_guest;
    completion->keep_single_step = watch_set->pending_keep_single_step;
    completion->source = watch_set->pending_source;
    completion->destination = watch_set->pending_destination;
    completion->byte_count = watch_set->pending_byte_count;
    ResetPendingWrite(watch_set);
    return true;
}

}  // namespace repiu::engine
