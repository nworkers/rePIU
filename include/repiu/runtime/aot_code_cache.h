#ifndef REPIU_RUNTIME_AOT_CODE_CACHE_H_
#define REPIU_RUNTIME_AOT_CODE_CACHE_H_

#include "repiu/runtime/aot_direct_return_table.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

constexpr std::uint32_t kDefaultAotIndirectInlineCacheEntryCount = 4U;

// Task 499. Emitted ahead of the existing return miss sequence; a miss falls
// straight through, so the bytes that follow move but never change. Exposed so
// the probe suite can emit the identical sequence standalone and execute it.
// `pop_bytes` is the original RET's total stack effect, four for `C3`.
bool EmitAotDirectReturnProbe(std::vector<std::uint8_t>* bytes,
                              std::uint32_t guest_source,
                              std::uint32_t pop_bytes,
                              struct AotDirectReturnProbeSite* site);

// Writes the absolute table address, mask, and hit-counter address into an
// emitted probe. `key_address` is the address of entry zero; the value operand
// is derived from it.
bool PatchAotDirectReturnProbe(std::uint8_t* bytes,
                               std::size_t byte_count,
                               const struct AotDirectReturnProbeSite& site,
                               std::uint32_t key_address,
                               std::uint32_t mask,
                               std::uint32_t hit_counter_address);

struct AotCodeCacheBuildOptions
{
    std::uint32_t indirect_inline_cache_entry_count =
        kDefaultAotIndirectInlineCacheEntryCount;
    bool enable_dbt_return_miss_dispatch = false;
    bool enable_dbt_indirect_miss_dispatch = false;
    // Route a static direct edge whose target was not emitted through a
    // fail-closed AOT-DBT runtime-dispatch stub.
    bool enable_dbt_direct_edge_dispatch = false;
    // Task 283 call/jump split probe. When the master
    // `enable_dbt_indirect_miss_dispatch` is set, these gate the host-dispatch
    // tail per instruction kind so a live run can bisect the Task 282 crash by
    // whether the CALL path's guest-stack return-address write is involved.
    // Both default true, so the master flag alone still emits both layouts
    // byte-for-byte as before.
    bool enable_dbt_indirect_dispatch_calls = true;
    bool enable_dbt_indirect_dispatch_jumps = true;
    bool enable_guarded_segment_pop = false;
    bool enable_guarded_segment_read = false;
    bool enable_guarded_segment_load = false;
    // Task 308 architecture probe. Ordinary planner-HLE records use a normal
    // host-dispatch slot whose fail-closed continuation retains INT3.
    bool enable_dbt_hle_dispatch = false;
    // Task 385. Reuse the fail-closed host dispatch slot for kPortIo only.
    bool enable_dbt_port_io_dispatch = false;
    // Task 391. Reuse it for kSegmentOverrideMem only when explicitly enabled.
    bool enable_dbt_segment_override_dispatch = false;
    // Task 348. Cooperative interrupt rendezvous emitted before direct
    // backward branches so an AOT-native busy loop can reach the existing
    // pending timer-interrupt injection path without cross-thread TF changes.
    bool enable_timer_safe_points = false;
    // Task 499. Probe a shared guest-to-cache memo table on the return miss
    // path before crossing to the host. Off by default, and while off nothing
    // is emitted, so the cache bytes are identical to a build without it.
    bool enable_direct_return_table = false;
    std::uint32_t direct_return_table_bits =
        kDefaultAotDirectReturnTableBits;
    // Task 553. Emit an image for a long-mode host: the guest's copied bytes go
    // through Task 550's classifier and Task 552's lowering, and everything the
    // emitter cannot yet produce for x86-64 reaches the existing INT3 boundary.
    //
    // Deliberately an option rather than a host `#ifdef`. Emission is pure
    // computation -- it produces bytes and executes nothing -- so the answer for
    // a given plan has to be the same on every host, and an `#ifdef` would make
    // that answer unobservable on Windows, which is where this project's test
    // loop actually runs.
    //
    // The name says image rather than copy on purpose. With it on, only `kCopy`
    // is emitted; every other kind falls to the boundary, because the slots the
    // emitter writes for them are hand-built 32-bit sequences and long mode
    // changes several of them without raising anything (`68 imm32` pushes eight
    // bytes there). Lowering the copies while emitting those would recreate one
    // layer up the silent divergence the classifier exists to prevent.
    //
    // Off by default, so the i386 path emits the same bytes it always has.
    bool enable_long_mode_emission = false;
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

// Task 562. Whether this host has the return-dispatch thunk an emitted `ret`
// jumps to.
//
// The first thing in long-mode emission that differs by host rather than by
// instruction: everything before it was a judgement about bytes and answered
// the same everywhere. A host without the thunk emits the boundary it emitted
// before. Exposed so the census asks the emitter this question instead of
// mirroring the `#if`, which is how the two would drift.
[[nodiscard]] bool LongModeReturnDispatchAvailable();

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
    // Task 499. When a direct-return probe precedes the miss tail, guards jump
    // here instead so the probe runs before the host crossing. Zero means no
    // probe was emitted and `miss_cache_offset` is the guard target, which is
    // what every build without the feature produces. `miss_cache_offset` itself
    // is deliberately unchanged: it is the key the reentry address, the site
    // lookup, and the fallback-distance constant all share.
    std::uint32_t miss_probe_cache_offset = 0;
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

// Task 499. The offset every inline-cache guard must jump to on a miss.
[[nodiscard]] inline std::uint32_t AotInlineCacheGuardTargetOffset(
    const AotIndirectInlineCacheSite& site)
{
    return site.miss_probe_cache_offset != 0U ? site.miss_probe_cache_offset
                                              : site.miss_cache_offset;
}

struct AotDbtReturnDispatchSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t miss_cache_offset = 0;
    std::uint32_t miss_address_immediate_offset = 0;
    std::uint32_t thunk_displacement_offset = 0;
    std::uint32_t fallback_cache_offset = 0;
    std::uint32_t success_cache_offset = 0;
};

// Task 308. A planner HLE boundary pushes the absolute slot address and guest
// source, then jumps to a Win32 host-stack thunk. The resolver rewrites the
// saved return slot to either fallback_cache_offset or success_cache_offset.
// Success pops the resolved cache target; fallback discards the metadata and
// reaches the existing provenance-aware INT3.
struct AotDbtHleDispatchSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t dispatch_cache_offset = 0;
    std::uint32_t dispatch_address_immediate_offset = 0;
    std::uint32_t thunk_displacement_offset = 0;
    std::uint32_t fallback_cache_offset = 0;
    std::uint32_t success_cache_offset = 0;
};

// An unresolved static direct edge uses a tail stub with the same two-slot
// host-stack ABI as HLE dispatch. The resolver either publishes a real cache
// target or reaches fallback_cache_offset, whose INT3 resumes guest_target.
struct AotDbtDirectEdgeDispatchSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t guest_target = 0;
    std::uint32_t dispatch_cache_offset = 0;
    std::uint32_t dispatch_address_immediate_offset = 0;
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
    // Companion HLE slot for Task 392 hybrid routing; zero when disabled.
    std::uint32_t dispatch_cache_offset = 0;
    // The displacement before the segment base is folded in, so the base can be
    // re-applied idempotently when the segment is re-resolved (Task 264).
    std::int32_t original_displacement = 0;
    // 0=ES,2=SS,3=DS,4=FS,5=GS.
    std::uint8_t segment_register = 0xFFU;
};
// Task 291. A guarded segment-pop slot reads the physical segment selector and
// compares it with both the original guest stack word and this shadow word.
// Success consumes the stack dword without changing selector state; mismatch
// restores the exact entry state and reaches fallback_offset (INT3).
struct AotGuardedSegmentPopSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t shadow_address_offset = 0;
    std::uint32_t success_counter_address_offset = 0;
    std::uint32_t fallback_counter_address_offset = 0;
    std::uint32_t fallback_offset = 0;
    std::uint8_t segment_register = 0xFFU;
};

// A guarded register-source MOV Sreg,r16 slot. Success is a semantic no-op
// when source, physical, and shadow selectors already match.
struct AotGuardedSegmentLoadSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t shadow_address_offset = 0;
    std::uint32_t success_counter_address_offset = 0;
    std::uint32_t fallback_counter_address_offset = 0;
    std::uint32_t fallback_offset = 0;
    std::uint8_t segment_register = 0xFFU;
    std::uint8_t gpr_register = 0xFFU;
};

// A guarded MOV r32,Sreg slot. It compares the physical selector with the
// shadow before writing the destination and restores entry state at fallback.
struct AotGuardedSegmentReadSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t shadow_address_offset = 0;
    std::uint32_t load_shadow_address_offset = 0;
    std::uint32_t fallback_offset = 0;
    std::uint8_t segment_register = 0xFFU;
    std::uint8_t gpr_register = 0xFFU;
};

struct AotTimerSafePointSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t request_address_offset = 0;
    std::uint32_t breakpoint_offset = 0;
};

// Task 499. Absolute-address and mask operands the direct-return probe needs,
// patched at placement exactly as AotTimerSafePointSite::request_address_offset
// is and re-offset the same way on dynamic append.
struct AotDirectReturnProbeSite
{
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t mask_immediate_offset = 0;
    std::uint32_t key_address_offset = 0;
    std::uint32_t target_address_offset = 0;
    std::uint32_t hit_counter_address_offset = 0;
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
    std::vector<AotDbtHleDispatchSite> dbt_hle_dispatch_sites;
    std::vector<AotDbtIndirectDispatchSite> dbt_indirect_dispatch_sites;
    std::vector<AotDbtDirectEdgeDispatchSite>
        dbt_direct_edge_dispatch_sites;
    std::vector<AotJumpTableSite> jump_table_sites;
    std::vector<AotGuardedSegmentPopSite> guarded_segment_pop_sites;
    std::vector<AotGuardedSegmentReadSite> guarded_segment_read_sites;
    std::vector<AotGuardedSegmentLoadSite> guarded_segment_load_sites;
    std::vector<AotSegmentOverrideSite> segment_override_sites;
    std::vector<AotTimerSafePointSite> timer_safe_point_sites;
    std::vector<AotDirectReturnProbeSite> direct_return_probe_sites;
    // Carried into platform placement so every later dynamic append uses the
    // same indirect call/jump layout as the initial image (Task 274).
    std::uint32_t indirect_inline_cache_entry_count =
        kDefaultAotIndirectInlineCacheEntryCount;
    bool dbt_return_miss_dispatch_enabled = false;
    // Task 499. Carried on the image so later dynamic appends emit the
    // same shape the initial placement did.
    bool direct_return_table_enabled = false;
    std::uint32_t direct_return_table_bits =
        kDefaultAotDirectReturnTableBits;
    bool dbt_hle_dispatch_enabled = false;
    bool dbt_port_io_dispatch_enabled = false;
    bool dbt_segment_override_dispatch_enabled = false;
    bool guarded_segment_pop_enabled = false;
    bool guarded_segment_read_enabled = false;
    bool guarded_segment_load_enabled = false;
    bool dbt_indirect_miss_dispatch_enabled = false;
    bool dbt_direct_edge_dispatch_enabled = false;
    bool timer_safe_points_enabled = false;
    // Task 553. What the long-mode emission actually managed, counted rather
    // than asserted. Without these "wired" is one yes-or-no and nobody can say
    // what fraction of a real plan an x86-64 host could emit. All zero when
    // `long_mode_emission_enabled` is false.
    bool long_mode_emission_enabled = false;
    std::uint32_t long_mode_copied_count = 0;
    std::uint32_t long_mode_lowered_count = 0;
    std::uint32_t long_mode_refused_count = 0;
    // Task 560. Direct branches emitted, and how many of those had to become a
    // boundary after all because their target fell outside the cache. Counted
    // separately from `long_mode_refused_count` because they are refusals of a
    // different thing: not "these bytes cannot be emitted" but "this edge has
    // nowhere in this image to land", which is a property of what got
    // translated rather than of the instruction.
    std::uint32_t long_mode_branch_count = 0;
    std::uint32_t long_mode_unresolved_branch_count = 0;
    // Task 562. Return slots emitted. Counted apart from branches because a
    // return is the first edge whose target is not known until the guest runs:
    // a branch resolves at build time or becomes a boundary, while every one of
    // these reaches a resolver.
    std::uint32_t long_mode_return_count = 0;
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

// Verify that every planner HLE record in a complete generated CFG is either
// an actual INT3 boundary or a structurally complete segment-selector guard
// whose mismatch path reaches INT3. Used before publishing arbitrary-entry
// dynamic translations.
bool ValidateAotCodeCacheHleCoverage(
    const AotTranslationPlan& plan,
    const AotCodeCacheImage& image,
    std::uint32_t* failure_guest_address = nullptr);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_CODE_CACHE_H_
