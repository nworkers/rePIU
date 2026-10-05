#include "glide_gate_fixup_index_probe.h"

#include "repiu/engine/aot_glide_gate_fixup_index.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace repiu::tools
{

// Task 773. The index behind ActivateGlideGateDirectTarget and the decision
// of which slots to write. The addresses are shaped like pumpit1's on Linux
// i386: gate code from 0x02800000, the cache at 0xE8021000.
bool RunGlideGateFixupIndexProbe()
{
    using engine::AotGlideGateFixupIndex;
    using engine::CollectGlideGateFixupWrites;
    using engine::EnsureAotGlideGateFixupIndex;
    using engine::FindAotGlideGateFixupOffsets;
    using engine::GlideGateFixupWrite;
    using runtime::AotCodeCacheFixup;
    using runtime::AotFixupKind;

    constexpr std::uint32_t kGateBase = 0x02800000U;
    constexpr std::uint32_t kGateA = kGateBase + 0x40U;
    constexpr std::uint32_t kGateB = kGateBase + 0x48U;
    constexpr std::uint32_t kGuestCode = 0x0104C97AU;

    std::vector<AotCodeCacheFixup> fixups = {
        {AotFixupKind::kDirectCall, kGuestCode, kGateA, 0x10U, true},
        // Below the bound: ordinary guest code, never a gate.
        {AotFixupKind::kDirectCall, kGuestCode, kGuestCode + 0x100U, 0x20U,
         true},
        // Kinds the activation does not rewrite.
        {AotFixupKind::kHleBoundary, kGuestCode, kGateA, 0x30U, false},
        {AotFixupKind::kConditionalBranch, kGuestCode, kGateA, 0x38U, true},
        {AotFixupKind::kIndirectExit, kGuestCode, kGateA, 0x3CU, false},
        {AotFixupKind::kDirectJump, kGuestCode, kGateB, 0x40U, true},
        {AotFixupKind::kBlockFallthrough, kGuestCode, kGateA, 0x50U, true},
    };

    AotGlideGateFixupIndex index;
    EnsureAotGlideGateFixupIndex(fixups, kGateBase, &index);
    const std::vector<std::uint32_t>* gate_a =
        FindAotGlideGateFixupOffsets(index, kGateA);
    const std::vector<std::uint32_t>* gate_b =
        FindAotGlideGateFixupOffsets(index, kGateB);
    // Only the three rewritten kinds at or above the bound, in array order.
    const bool filter_ok = index.indexed_fixup_count == fixups.size() &&
        index.patch_offsets_by_target.size() == 2U &&
        gate_a != nullptr && gate_a->size() == 2U &&
        (*gate_a)[0] == 0x10U && (*gate_a)[1] == 0x50U &&
        gate_b != nullptr && gate_b->size() == 1U && (*gate_b)[0] == 0x40U &&
        FindAotGlideGateFixupOffsets(index, kGuestCode + 0x100U) == nullptr &&
        FindAotGlideGateFixupOffsets(index, kGateBase + 0x50U) == nullptr;

    // Appended fixups are indexed on the next call, without a rebuild.
    const std::uint32_t rebuilds_before = index.rebuild_count;
    fixups.push_back(
        {AotFixupKind::kDirectCall, kGuestCode, kGateA, 0x60U, true});
    fixups.push_back(
        {AotFixupKind::kDirectCall, kGuestCode, kGuestCode, 0x70U, true});
    EnsureAotGlideGateFixupIndex(fixups, kGateBase, &index);
    gate_a = FindAotGlideGateFixupOffsets(index, kGateA);
    const bool append_ok = index.rebuild_count == rebuilds_before &&
        index.indexed_fixup_count == fixups.size() &&
        gate_a != nullptr && gate_a->size() == 3U &&
        (*gate_a)[2] == 0x60U;

    // An unchanged array costs nothing and changes nothing.
    EnsureAotGlideGateFixupIndex(fixups, kGateBase, &index);
    gate_a = FindAotGlideGateFixupOffsets(index, kGateA);
    const bool idle_ok = index.rebuild_count == rebuilds_before &&
        gate_a != nullptr && gate_a->size() == 3U;

    // A shorter array is a different array: rebuilt, nothing left over.
    std::vector<AotCodeCacheFixup> shorter(fixups.begin(),
                                           fixups.begin() + 1);
    EnsureAotGlideGateFixupIndex(shorter, kGateBase, &index);
    gate_a = FindAotGlideGateFixupOffsets(index, kGateA);
    const bool shrink_ok = index.rebuild_count == rebuilds_before + 1U &&
        index.indexed_fixup_count == 1U && gate_a != nullptr &&
        gate_a->size() == 1U &&
        FindAotGlideGateFixupOffsets(index, kGateB) == nullptr;

    // A different bound re-filters everything.
    EnsureAotGlideGateFixupIndex(fixups, kGateA + 4U, &index);
    const bool bound_ok = index.rebuild_count == rebuilds_before + 2U &&
        FindAotGlideGateFixupOffsets(index, kGateA) == nullptr &&
        FindAotGlideGateFixupOffsets(index, kGateB) != nullptr;

    // Which slots to write. The cache sits high, as on Linux i386, and the
    // gate low: more than 2 GiB apart, which a 32-bit instruction pointer
    // covers by wrapping. The displacement is the low 32 bits of the
    // difference.
    constexpr std::uint32_t kCacheBase = 0xE8021000U;
    std::vector<std::uint8_t> cache(0x100U, 0xCCU);
    const std::vector<std::uint32_t> offsets = {0x10U, 0x50U, 0x60U};
    const auto expected = [&](std::uint32_t offset) {
        return static_cast<std::int32_t>(
            kGateA - (kCacheBase + offset + 4U));
    };
    std::vector<GlideGateFixupWrite> writes;
    CollectGlideGateFixupWrites(cache.data(), kCacheBase, offsets, kGateA,
                                true, &writes);
    const bool first_ok = writes.size() == 3U &&
        writes[0].cache_patch_offset == 0x10U &&
        writes[0].displacement == expected(0x10U) &&
        writes[1].cache_patch_offset == 0x50U &&
        writes[2].cache_patch_offset == 0x60U &&
        writes[2].displacement == expected(0x60U);

    // Written once, nothing is left to write: the steady state.
    for (const GlideGateFixupWrite& write : writes)
    {
        std::memcpy(cache.data() + write.cache_patch_offset,
                    &write.displacement, sizeof(write.displacement));
    }
    CollectGlideGateFixupWrites(cache.data(), kCacheBase, offsets, kGateA,
                                true, &writes);
    const bool steady_ok = writes.empty();

    // One slot overwritten since (an append relinking a stale entry, say):
    // that slot alone is written again.
    const std::int32_t other = 0x12345678;
    std::memcpy(cache.data() + 0x50U, &other, sizeof(other));
    CollectGlideGateFixupWrites(cache.data(), kCacheBase, offsets, kGateA,
                                true, &writes);
    const bool reverted_ok = writes.size() == 1U &&
        writes[0].cache_patch_offset == 0x50U &&
        writes[0].displacement == expected(0x50U);

    // The wrapped slot really lands on the gate.
    const bool wrap_ok = kCacheBase + 0x10U + 4U +
        static_cast<std::uint32_t>(expected(0x10U)) == kGateA;

    // Without the wrap (a 64-bit instruction pointer) the same slots are out
    // of reach and left alone, while a near target is still written.
    std::vector<std::uint8_t> far_cache(0x100U, 0xCCU);
    CollectGlideGateFixupWrites(far_cache.data(), kCacheBase, offsets, kGateA,
                                false, &writes);
    const bool far_ok = writes.empty();
    CollectGlideGateFixupWrites(far_cache.data(), kCacheBase, offsets,
                                kCacheBase + 0x1000U, false, &writes);
    const bool range_ok = far_ok && writes.size() == 3U &&
        writes[0].displacement ==
            static_cast<std::int32_t>(0x1000U - 0x10U - 4U);

    // Null arguments are refused quietly.
    writes.push_back({1U, 1});
    CollectGlideGateFixupWrites(nullptr, kCacheBase, offsets, kGateA, true,
                                &writes);
    const bool null_ok = writes.empty();
    EnsureAotGlideGateFixupIndex(fixups, kGateBase, nullptr);
    CollectGlideGateFixupWrites(cache.data(), kCacheBase, offsets, kGateA,
                                true, nullptr);

    const bool all = filter_ok && append_ok && idle_ok && shrink_ok &&
        bound_ok && first_ok && steady_ok && reverted_ok && wrap_ok &&
        range_ok && null_ok;
    std::cout << "glide_gate_fixup_index_filter="
              << (filter_ok ? "true" : "false")
              << "\nglide_gate_fixup_index_append="
              << (append_ok ? "true" : "false")
              << "\nglide_gate_fixup_index_idle="
              << (idle_ok ? "true" : "false")
              << "\nglide_gate_fixup_index_shrink="
              << (shrink_ok ? "true" : "false")
              << "\nglide_gate_fixup_index_bound="
              << (bound_ok ? "true" : "false")
              << "\nglide_gate_fixup_writes_first="
              << (first_ok ? "true" : "false")
              << "\nglide_gate_fixup_writes_steady="
              << (steady_ok ? "true" : "false")
              << "\nglide_gate_fixup_writes_reverted="
              << (reverted_ok ? "true" : "false")
              << "\nglide_gate_fixup_writes_wrap="
              << (wrap_ok ? "true" : "false")
              << "\nglide_gate_fixup_writes_range="
              << (range_ok ? "true" : "false")
              << "\nglide_gate_fixup_writes_null="
              << (null_ok ? "true" : "false")
              << "\nglide_gate_fixup_index_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
