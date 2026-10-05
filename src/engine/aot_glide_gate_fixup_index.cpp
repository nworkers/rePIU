#include "repiu/engine/aot_glide_gate_fixup_index.h"

#include <cstring>
#include <limits>

namespace repiu::engine
{

bool IsGlideGateRelinkFixupKind(runtime::AotFixupKind kind)
{
    return kind == runtime::AotFixupKind::kDirectCall ||
        kind == runtime::AotFixupKind::kDirectJump ||
        kind == runtime::AotFixupKind::kBlockFallthrough;
}

void EnsureAotGlideGateFixupIndex(
    const std::vector<runtime::AotCodeCacheFixup>& fixups,
    std::uint32_t minimum_target,
    AotGlideGateFixupIndex* index)
{
    if (index == nullptr)
    {
        return;
    }
    if (index->indexed_fixup_count > fixups.size() ||
        index->minimum_target != minimum_target)
    {
        index->patch_offsets_by_target.clear();
        index->indexed_fixup_count = 0U;
        index->minimum_target = minimum_target;
        ++index->rebuild_count;
    }
    for (std::size_t position = index->indexed_fixup_count;
         position < fixups.size(); ++position)
    {
        const runtime::AotCodeCacheFixup& fixup = fixups[position];
        if (fixup.guest_target >= minimum_target &&
            IsGlideGateRelinkFixupKind(fixup.kind))
        {
            index->patch_offsets_by_target[fixup.guest_target].push_back(
                fixup.cache_patch_offset);
        }
    }
    index->indexed_fixup_count = static_cast<std::uint32_t>(fixups.size());
}

const std::vector<std::uint32_t>* FindAotGlideGateFixupOffsets(
    const AotGlideGateFixupIndex& index,
    std::uint32_t target)
{
    const auto found = index.patch_offsets_by_target.find(target);
    return found != index.patch_offsets_by_target.end() ? &found->second
                                                        : nullptr;
}

void CollectGlideGateFixupWrites(
    const std::uint8_t* cache_bytes,
    std::uint32_t base_address,
    const std::vector<std::uint32_t>& offsets,
    std::uint32_t direct_target,
    bool wraps_at_32_bits,
    std::vector<GlideGateFixupWrite>* writes)
{
    if (writes == nullptr)
    {
        return;
    }
    writes->clear();
    if (cache_bytes == nullptr)
    {
        return;
    }
    for (const std::uint32_t offset : offsets)
    {
        const std::int64_t relative =
            static_cast<std::int64_t>(direct_target) -
            static_cast<std::int64_t>(
                static_cast<std::uint64_t>(base_address) + offset + 4U);
        if (!wraps_at_32_bits &&
            (relative < std::numeric_limits<std::int32_t>::min() ||
             relative > std::numeric_limits<std::int32_t>::max()))
        {
            continue;
        }
        // Truncation is the wrap: the low 32 bits are the displacement a
        // 32-bit instruction pointer needs.
        const std::int32_t displacement = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(relative)));
        std::int32_t current = 0;
        std::memcpy(&current, cache_bytes + offset, sizeof(current));
        if (current != displacement)
        {
            writes->push_back({offset, displacement});
        }
    }
}

}  // namespace repiu::engine
