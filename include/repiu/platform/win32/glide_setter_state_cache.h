#pragma once

#include "repiu/platform/win32/glide_setter_state_model.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace repiu::platform::win32
{

// Task 365: records which Glide render state was last applied *successfully on the
// host*, so an exact repeat can skip the host rendezvous. Task 364 measured that
// 90.71% of setter calls are such repeats.
//
// The gate entry, the ABI validations, the stdcall cleanup, the return, and the
// call order are all unaffected -- only `InvokeOnHostThread` and the redundant
// OpenGL application are skipped. The rules come from
// `glide_setter_state_model.h`, shared with the census that measured the ceiling.
constexpr std::size_t kWin32GlideSetterStateCacheCapacity = 256U;

struct Win32GlideSetterStateCacheEntry
{
    bool applied_valid = false;
    Win32GlideSetterStateKey applied_key;
    std::uint32_t elided_count = 0;
    std::uint32_t applied_count = 0;
};

struct Win32GlideSetterStateCache
{
    bool enabled = false;
    std::array<Win32GlideSetterStateCacheEntry,
               kWin32GlideSetterStateCacheCapacity> entries = {};
    std::uint32_t texture_generation = 0;
    std::uint32_t elided_count = 0;
    std::uint32_t applied_count = 0;
    std::uint32_t voided_count = 0;
    std::uint32_t invalidation_count = 0;
    std::uint32_t ordinal_overflow_count = 0;
};

// Aggregates only, for the same stack-size reason as the census snapshot.
struct Win32GlideSetterStateCacheSnapshot
{
    bool enabled = false;
    // Task 437: which batch the counters below were produced under, so an A/B log
    // says for itself which configuration it is.
    bool texture_state = false;
    std::uint32_t active_entry_count = 0;
    std::uint32_t texture_generation = 0;
    std::uint32_t elided_count = 0;
    std::uint32_t applied_count = 0;
    std::uint32_t voided_count = 0;
    std::uint32_t invalidation_count = 0;
    std::uint32_t ordinal_overflow_count = 0;
};

// On by default, following the Task 335 precedent: `REPIU_GLIDE_SETTER_ELIDE=0`
// (or `off`/`false`) restores the unconditional rendezvous for an A/B.
bool ResolveGlideSetterElisionEnabled(std::string_view setting);
bool GlideSetterElisionEnabled();

// Task 437, batch two: the texture-state setters batch one deferred. They are the
// largest remaining group -- `grTexClampMode`, `grTexFilterMode` and
// `grTexMipMapMode` are called exactly as often as `grTexSource`, once per bind --
// and eliding a same-valued call is equivalent to executing it because
// `SetTextureSource` re-applies all four sampler parameters from the TMU state on
// every bind, so a skipped repeat can never leave a texture object stale.
//
// Task 439: on by default after the paired A/B measured the three gates as
// 99.76% redundant with no visual difference. `REPIU_GLIDE_SETTER_ELIDE=0`
// still wins: it disables the cache entirely, and this switch only widens
// what the cache covers.
//
// `grTexSource` is deliberately not here. Its fourth argument is a `GrTexInfo*`,
// and the struct behind an unchanged pointer can change without a download, which
// the texture generation does not catch, so an identical key would not prove
// identical state.
bool GlideSetterTextureStateElisionEnabled();
bool IsGlideSetterTextureStateElisionGate(repiu::hle::GlideGateId gate_id);

// Batch one: the setters Task 364 measured at 99.9% or better repetition with one
// or two distinct argument values, all of which return void. Must be a subset of
// `IsGlideSetterStateGate`, which the probe checks.
bool IsGlideSetterElisionGate(repiu::hle::GlideGateId gate_id);

// True when this exact state was already applied successfully and no invalidating
// event has happened since.
bool ShouldElideGlideSetterState(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal,
    const Win32GlideSetterStateKey& key);

void RecordGlideSetterStateElided(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal);

// Called only after the host actually applied the state successfully.
void RecordGlideSetterStateApplied(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal,
    const Win32GlideSetterStateKey& key);

// A declined gate or a retained-but-unexpressed argument leaves the host state
// unknown, so the record must go.
void RecordGlideSetterStateVoided(
    Win32GlideSetterStateCache* cache,
    std::uint16_t ordinal);

void InvalidateGlideSetterStateCache(Win32GlideSetterStateCache* cache);

void BumpGlideSetterStateCacheTextureGeneration(
    Win32GlideSetterStateCache* cache);

Win32GlideSetterStateCacheSnapshot SnapshotGlideSetterStateCache(
    const Win32GlideSetterStateCache& cache);

}  // namespace repiu::platform::win32
