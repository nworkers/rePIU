#include "repiu/platform/win32/aot_cache_address_index.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"

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

    // Link oldest to newest so each bucket chain ends up newest-first.
    for (std::uint32_t map_index = 0; map_index < entry_count; ++map_index)
    {
        LinkEntry(&index, placement->address_map, map_index);
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
    index.indexed_entry_count = map_index + 1U;
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
