#ifndef REPIU_RUNTIME_AOT_CODE_CACHE_H_
#define REPIU_RUNTIME_AOT_CODE_CACHE_H_

#include "repiu/runtime/aot_translation_plan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

constexpr std::uint32_t kDefaultAotIndirectInlineCacheEntryCount = 4U;

struct AotCodeCacheBuildOptions
{
    std::uint32_t indirect_inline_cache_entry_count =
        kDefaultAotIndirectInlineCacheEntryCount;
    bool enable_dbt_return_miss_dispatch = false;
    bool enable_dbt_indirect_miss_dispatch = false;
    // Task 283 call/jump split probe. When the master
    // `enable_dbt_indirect_miss_dispatch` is set, these gate the host-dispatch
    // tail per instruction kind so a live run can bisect the Task 282 crash by
    // whether the CALL path's guest-stack return-address write is involved.
    // Both default true, so the master flag alone still emits both layouts
    // byte-for-byte as before.
    bool enable_dbt_indirect_dispatch_calls = true;
    bool enable_dbt_indirect_dispatch_jumps = true;
};

enum class AotFixupKind
{
    kDirectCall,
    kDirectJump,
    kBlockFallthrough,
    kConditionalBranch,
    kHleBoundary,
    kIndirectExit,
};

struct AotAddressMapEntry
{
    std::uint32_t guest_address = 0;
    std::uint32_t cache_offset = 0;
    std::uint8_t guest_length = 0;
    std::uint8_t emitted_length = 0;
};

struct AotCodeCacheFixup
{
    AotFixupKind kind = AotFixupKind::kDirectJump;
    std::uint32_t guest_source = 0;
    std::uint32_t guest_target = 0;
    std::uint32_t cache_patch_offset = 0;
    bool resolved = false;
};

struct AotInlineCacheEntry
{
    std::uint32_t compare_offset = 0;
    std::uint32_t target_immediate_offset = 0;
    std::uint32_t guard_offset = 0;
    std::uint32_t jump_displacement_offset = 0;
};

struct AotIndirectInlineCacheSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t miss_cache_offset = 0;
    std::uint32_t target_immediate_offset = 0;
    std::uint32_t guard_offset = 0;
    std::uint32_t jump_displacement_offset = 0;
    // Polymorphic indirect call/jump and return thunks chain several
    // compare/hit blocks so a small target set does not thrash one predictor
    // slot (Tasks 220 and 273). `entries` holds every block, with entry 0
    // duplicating the legacy fields above; entry i's patched JNE falls to
    // entry i+1's compare, and the last one falls to the miss tail. Empty is
    // retained as the metadata representation of a legacy single-slot site.
    std::vector<AotInlineCacheEntry> entries;
    // Patcher-side round-robin replacement cursor (worker thread only).
    std::uint32_t replace_cursor = 0;
    bool is_call = false;
    bool is_return = false;
};

struct AotDbtReturnDispatchSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t miss_cache_offset = 0;
    std::uint32_t miss_address_immediate_offset = 0;
    std::uint32_t thunk_displacement_offset = 0;
    std::uint32_t fallback_cache_offset = 0;
    std::uint32_t success_cache_offset = 0;
};

// Task 282. The `FF /2` / `FF /4` inline-cache miss tail of an `aot-dbt` image
// pushes a fixed three-slot frame (call return address, miss address, guest
// source) and jumps to the Win32 host-stack thunk. A jump pushes an unused first
// slot so both kinds share one frame depth, and the emitted continuations decide
// the final ESP: `C3` for a call leaves the pushed return address at `[esp]`,
// `C2 04 00` for a jump restores the original ESP, and the fallback continuation
// discards both remaining slots before the existing provenance `INT3`.
struct AotDbtIndirectDispatchSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t miss_cache_offset = 0;
    std::uint32_t miss_address_immediate_offset = 0;
    std::uint32_t thunk_displacement_offset = 0;
    std::uint32_t fallback_cache_offset = 0;
    std::uint32_t success_cache_offset = 0;
    bool is_call = false;
};

// A translated bounded switch: `jmp [reg*4 + disp32]` reading a native
// pointer table emitted inline. Absolute addresses are resolved after the
// cache is placed; unresolved entries point at fallback_offset (INT3).
struct AotJumpTableSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t displacement_patch_offset = 0;
    std::uint32_t fallback_offset = 0;
    std::uint32_t table_cache_offset = 0;
    std::vector<std::uint32_t> guest_targets;
};

// A natively-translated segment-override memory access (Task 264 Phase 3a). The
// emitted sequence is: pushfd; cmp word [shadow selector], S; je do_access;
// popfd; int3 (fallback single-steps the original); do_access: popfd; <access
// with the prefix stripped and the displacement widened to disp32>. The Win32
// translation path fills selector S, the shadow address, and folds the segment
// base into the displacement, all from the live selector table. Offsets are
// image-relative until placement.
struct AotSegmentOverrideSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    // disp32 field of the emitted access; the segment base is added here.
    std::uint32_t displacement_offset = 0;
    // abs32 field of the guard's `cmp word [abs], imm16` (the shadow address).
    std::uint32_t guard_address_offset = 0;
    // imm16 field of the guard (the translation-time selector value S).
    std::uint32_t guard_selector_offset = 0;
    // The displacement before the segment base is folded in, so the base can be
    // re-applied idempotently when the segment is re-resolved (Task 264).
    std::int32_t original_displacement = 0;
    // 0=ES,2=SS,3=DS,4=FS,5=GS.
    std::uint8_t segment_register = 0xFFU;
};

struct AotCodeCacheImage
{
    bool valid = false;
    bool executable = false;
    std::uint32_t entry_cache_offset = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<AotAddressMapEntry> address_map;
    std::vector<AotCodeCacheFixup> fixups;
    std::vector<AotIndirectInlineCacheSite> indirect_inline_cache_sites;
    std::vector<AotDbtReturnDispatchSite> dbt_return_dispatch_sites;
    std::vector<AotDbtIndirectDispatchSite> dbt_indirect_dispatch_sites;
    std::vector<AotJumpTableSite> jump_table_sites;
    std::vector<AotSegmentOverrideSite> segment_override_sites;
    // Carried into platform placement so every later dynamic append uses the
    // same indirect call/jump layout as the initial image (Task 274).
    std::uint32_t indirect_inline_cache_entry_count =
        kDefaultAotIndirectInlineCacheEntryCount;
    bool dbt_return_miss_dispatch_enabled = false;
    bool dbt_indirect_miss_dispatch_enabled = false;
    std::uint32_t resolved_fixup_count = 0;
    std::uint32_t external_fixup_count = 0;
    std::uint32_t unsupported_branch_count = 0;
    std::uint32_t decode_failure_count = 0;
    std::uint64_t elapsed_microseconds = 0;
    std::string message;
};

bool BuildAotCodeCacheImage(const AotTranslationPlan& plan,
                            AotCodeCacheImage* image);
bool BuildAotCodeCacheImage(const AotTranslationPlan& plan,
                            const AotCodeCacheBuildOptions& options,
                            AotCodeCacheImage* image);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_CODE_CACHE_H_
