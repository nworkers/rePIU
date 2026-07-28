#include "repiu/platform/win32/aot_cache_address_index.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"

#include <algorithm>

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint32_t kMinimumBucketCount = 64U;

// Same multiplicative hash the single-step hotspot profile uses. Guest
// addresses are dense and page-aligned in places, so the multiply matters.
std::uint32_t HashGuestAddress(std::uint32_t guest_address,
                               std::uint32_t bucket_count)
{
    return (guest_address * 2654435761U) & (bucket_count - 1U);
}

std::uint32_t RoundUpToPowerOfTwo(std::uint32_t value)
{
    std::uint32_t result = kMinimumBucketCount;
    while (result < value && result < 0x40000000U)
    {
        result <<= 1;
    }
    return result;
}

void LinkEntry(Win32AotCacheAddressIndex* index,
               const std::vector<runtime::AotAddressMapEntry>& address_map,
               std::uint32_t map_index)
{
    const std::uint32_t bucket_count =
        static_cast<std::uint32_t>(index->buckets.size());
    const std::uint32_t bucket = HashGuestAddress(
        address_map[map_index].guest_address, bucket_count);
    // Newest-first: the new entry becomes the head and points at the previous
    // head, which is the next-older entry in this bucket.
    index->next_in_bucket[map_index] = index->buckets[bucket];
    index->buckets[bucket] = map_index;
}

}  // namespace

namespace
{

// Task 334: maintained alongside the hash chains so the reverse lookup can
// binary-search. Sortedness is a property of how entries are produced, so it is
// checked rather than trusted.
void ObserveCacheOffsetOrder(Win32AotCacheAddressIndex* index,
                             const std::vector<runtime::AotAddressMapEntry>&
                                 address_map,
                             std::uint32_t map_index)
{
    const runtime::AotAddressMapEntry& entry = address_map[map_index];
    if (map_index != 0U &&
        entry.cache_offset < address_map[map_index - 1U].cache_offset)
    {
        index->cache_offset_sorted = false;
    }
    index->max_emitted_length = std::max<std::uint32_t>(
        index->max_emitted_length, entry.emitted_length);
}

}  // namespace

void RebuildAotCacheAddressIndex(Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    Win32AotCacheAddressIndex& index = placement->cache_address_index;
    const std::uint32_t entry_count =
        static_cast<std::uint32_t>(placement->address_map.size());

    index.buckets.assign(
        RoundUpToPowerOfTwo(entry_count),
        Win32AotCacheAddressIndex::kInvalidIndex);
    index.next_in_bucket.assign(
        entry_count, Win32AotCacheAddressIndex::kInvalidIndex);
    index.cache_offset_sorted = true;
    index.max_emitted_length = 0;

    // Link oldest to newest so each bucket chain ends up newest-first.
    for (std::uint32_t map_index = 0; map_index < entry_count; ++map_index)
    {
        LinkEntry(&index, placement->address_map, map_index);
        ObserveCacheOffsetOrder(&index, placement->address_map, map_index);
    }
    index.indexed_entry_count = entry_count;
}

void EnsureAotCacheAddressIndex(Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    if (placement->cache_address_index.indexed_entry_count !=
        static_cast<std::uint32_t>(placement->address_map.size()))
    {
        RebuildAotCacheAddressIndex(placement);
    }
}

void InvalidateAotCacheAddressIndex(Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    Win32AotCacheAddressIndex& index = placement->cache_address_index;
    index.indexed_entry_count = 0;
    index.buckets.clear();
    index.next_in_bucket.clear();
}

void AppendAotCacheAddressIndexEntry(Win32AotCodeCachePlacement* placement,
                                     std::uint32_t map_index)
{
    if (placement == nullptr ||
        map_index >= placement->address_map.size())
    {
        return;
    }
    Win32AotCacheAddressIndex& index = placement->cache_address_index;
    const std::uint32_t entry_count =
        static_cast<std::uint32_t>(placement->address_map.size());

    // Only a contiguous append links incrementally.
    if (map_index != index.indexed_entry_count ||
        index.next_in_bucket.size() != index.indexed_entry_count)
    {
        InvalidateAotCacheAddressIndex(placement);
        return;
    }

    // First entry after an invalidation sizes the table for the map as it
    // already stands, which is the full map during initial registration, so the
    // whole init loop links in O(n) with no rebuild.
    if (index.buckets.empty())
    {
        index.buckets.assign(
            RoundUpToPowerOfTwo(entry_count),
            Win32AotCacheAddressIndex::kInvalidIndex);
    }
    else if (entry_count > static_cast<std::uint32_t>(index.buckets.size()))
    {
        // Growing past load factor one: rebuild at double width. Amortized
        // O(1) across appends.
        RebuildAotCacheAddressIndex(placement);
        return;
    }

    index.next_in_bucket.push_back(
        Win32AotCacheAddressIndex::kInvalidIndex);
    LinkEntry(&index, placement->address_map, map_index);
    ObserveCacheOffsetOrder(&index, placement->address_map, map_index);
    index.indexed_entry_count = map_index + 1U;
}

AotGuestAddressLookup LookupAotGuestAddressIndex(
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t cache_offset)
{
    AotGuestAddressLookup result;
    const Win32AotCacheAddressIndex& index = placement.cache_address_index;
    if (!index.cache_offset_sorted ||
        index.indexed_entry_count !=
            static_cast<std::uint32_t>(placement.address_map.size()) ||
        placement.address_map.empty())
    {
        return result;
    }
    result.usable = true;

    // First entry starting after the offset; every candidate lies before it.
    const auto upper = std::upper_bound(
        placement.address_map.begin(), placement.address_map.end(),
        cache_offset,
        [](std::uint32_t value, const runtime::AotAddressMapEntry& entry) {
            return value < entry.cache_offset;
        });
    if (upper == placement.address_map.begin())
    {
        return result;
    }

    // Walk back while an earlier, longer entry could still cover the offset,
    // keeping the lowest matching index so the answer equals the scan's first
    // match even if two ranges overlap.
    std::size_t position = static_cast<std::size_t>(
        upper - placement.address_map.begin());
    bool found = false;
    std::uint32_t match = 0;
    while (position-- > 0U)
    {
        const runtime::AotAddressMapEntry& entry =
            placement.address_map[position];
        if (cache_offset >= entry.cache_offset &&
            cache_offset < entry.cache_offset + entry.emitted_length)
        {
            found = true;
            match = static_cast<std::uint32_t>(position);
        }
        if (entry.cache_offset + index.max_emitted_length <= cache_offset)
        {
            break;
        }
    }
    result.found = found;
    result.map_index = match;
    return result;
}

bool LookupAotCacheAddressIndex(const Win32AotCodeCachePlacement& placement,
                                std::uint32_t guest_address,
                                bool newest_active,
                                std::uint32_t* map_index)
{
    const Win32AotCacheAddressIndex& index = placement.cache_address_index;
    if (map_index == nullptr || index.buckets.empty() ||
        index.indexed_entry_count !=
            static_cast<std::uint32_t>(placement.address_map.size()) ||
        index.next_in_bucket.size() != index.indexed_entry_count)
    {
        return false;
    }

    const std::uint32_t bucket_count =
        static_cast<std::uint32_t>(index.buckets.size());
    std::uint32_t candidate = index.buckets[
        HashGuestAddress(guest_address, bucket_count)];
    std::uint32_t oldest_match = Win32AotCacheAddressIndex::kInvalidIndex;
    // Bounded so a corrupted chain cannot spin forever.
    std::uint32_t visited = 0;
    while (candidate != Win32AotCacheAddressIndex::kInvalidIndex &&
           candidate < index.indexed_entry_count &&
           visited <= index.indexed_entry_count)
    {
        // Collisions put unrelated guest addresses in the same chain.
        if (placement.address_map[candidate].guest_address == guest_address)
        {
            if (newest_active)
            {
                // Chain runs newest-first, so the first active match is the
                // newest active entry.
                if (candidate < placement.address_map_states.size() &&
                    placement.address_map_states[candidate].active)
                {
                    *map_index = candidate;
                    return true;
                }
            }
            else
            {
                // Keep going; the last match in the chain is the oldest entry.
                oldest_match = candidate;
            }
        }
        candidate = index.next_in_bucket[candidate];
        ++visited;
    }

    if (!newest_active &&
        oldest_match != Win32AotCacheAddressIndex::kInvalidIndex)
    {
        *map_index = oldest_match;
        return true;
    }
    return false;
}

}  // namespace repiu::platform::win32
