#include "repiu/runtime/aot_direct_return_table.h"

#include <algorithm>

namespace repiu::runtime
{

std::uint32_t AotDirectReturnTableIndex(const std::uint32_t guest_target,
                                        const std::uint32_t mask)
{
    // shr ecx,13 / xor ecx,eax / and ecx,mask in the emitted probe.
    return (guest_target ^ (guest_target >> 13U)) & mask;
}

std::uint32_t ResolveAotDirectReturnTableBits(const char* setting)
{
    if (setting == nullptr || setting[0] == 0)
    {
        return kDefaultAotDirectReturnTableBits;
    }
    std::uint32_t parsed = 0;
    for (const char* cursor = setting; *cursor != 0; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
        {
            return kDefaultAotDirectReturnTableBits;
        }
        parsed = parsed * 10U + static_cast<std::uint32_t>(*cursor - '0');
        if (parsed > kAotDirectReturnTableMaximumBits)
        {
            return kAotDirectReturnTableMaximumBits;
        }
    }
    return std::clamp(parsed, kAotDirectReturnTableMinimumBits,
                      kAotDirectReturnTableMaximumBits);
}

void ResetAotDirectReturnTable(AotDirectReturnTable* table,
                               const std::uint32_t bits)
{
    if (table == nullptr)
    {
        return;
    }
    const std::uint32_t clamped =
        std::clamp(bits, kAotDirectReturnTableMinimumBits,
                   kAotDirectReturnTableMaximumBits);
    const std::size_t count = std::size_t{1} << clamped;
    table->entries.assign(count, AotDirectReturnEntry{});
    table->mask = static_cast<std::uint32_t>(count - 1U);
    table->insert_count = 0;
    table->overwrite_count = 0;
    table->clear_count = 0;
    table->cleared_entry_count = 0;
    table->hit_count = 0;
}

bool InsertAotDirectReturnEntry(AotDirectReturnTable* table,
                                const std::uint32_t guest_target,
                                const std::uint32_t cache_target)
{
    if (table == nullptr || table->entries.empty() || guest_target == 0U ||
        cache_target == 0U)
    {
        return false;
    }
    const std::uint32_t index =
        AotDirectReturnTableIndex(guest_target, table->mask);
    if (index >= table->entries.size())
    {
        return false;
    }
    AotDirectReturnEntry& entry = table->entries[index];
    if (entry.guest_key != 0U && entry.guest_key != guest_target)
    {
        ++table->overwrite_count;
    }
    entry.guest_key = guest_target;
    entry.cache_target = cache_target;
    ++table->insert_count;
    return true;
}

bool LookupAotDirectReturnEntry(const AotDirectReturnTable& table,
                                const std::uint32_t guest_target,
                                std::uint32_t* cache_target)
{
    if (cache_target == nullptr || table.entries.empty() || guest_target == 0U)
    {
        return false;
    }
    const std::uint32_t index =
        AotDirectReturnTableIndex(guest_target, table.mask);
    if (index >= table.entries.size())
    {
        return false;
    }
    const AotDirectReturnEntry& entry = table.entries[index];
    if (entry.guest_key != guest_target)
    {
        return false;
    }
    *cache_target = entry.cache_target;
    return true;
}

void ClearAotDirectReturnTable(AotDirectReturnTable* table)
{
    if (table == nullptr || table->entries.empty())
    {
        return;
    }
    ++table->clear_count;
    table->cleared_entry_count += table->entries.size();
    std::fill(table->entries.begin(), table->entries.end(),
              AotDirectReturnEntry{});
}

}  // namespace repiu::runtime
