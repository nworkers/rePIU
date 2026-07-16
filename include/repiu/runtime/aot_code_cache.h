#ifndef REPIU_RUNTIME_AOT_CODE_CACHE_H_
#define REPIU_RUNTIME_AOT_CODE_CACHE_H_

#include "repiu/runtime/aot_translation_plan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

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
    // Return thunks chain several compare/hit blocks so a helper returning
    // to a handful of call sites (four in the observed decode loop) does not
    // thrash a single predictor slot (Task 220). `entries` holds every
    // block, entry 0 duplicating the legacy fields above; entry i's patched
    // JNE falls to entry i+1's compare, the last one to the miss tail.
    // Empty for indirect call/jmp sites, which keep the single-slot layout.
    std::vector<AotInlineCacheEntry> entries;
    // Patcher-side round-robin replacement cursor (worker thread only).
    std::uint32_t replace_cursor = 0;
    bool is_call = false;
    bool is_return = false;
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

struct AotCodeCacheImage
{
    bool valid = false;
    bool executable = false;
    std::uint32_t entry_cache_offset = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<AotAddressMapEntry> address_map;
    std::vector<AotCodeCacheFixup> fixups;
    std::vector<AotIndirectInlineCacheSite> indirect_inline_cache_sites;
    std::vector<AotJumpTableSite> jump_table_sites;
    std::uint32_t resolved_fixup_count = 0;
    std::uint32_t external_fixup_count = 0;
    std::uint32_t unsupported_branch_count = 0;
    std::uint32_t decode_failure_count = 0;
    std::uint64_t elapsed_microseconds = 0;
    std::string message;
};

bool BuildAotCodeCacheImage(const AotTranslationPlan& plan,
                            AotCodeCacheImage* image);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_CODE_CACHE_H_
