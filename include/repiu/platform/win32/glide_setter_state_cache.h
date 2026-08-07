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
    bool batch_three = false;
    bool batch_four = false;
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
// `grTexSource` is not in *this* list -- it moved to batch three below, once
// Task 442 established that the argument this backend actually reads is only
// `startAddress`.
bool GlideSetterTextureStateElisionEnabled();
bool IsGlideSetterTextureStateElisionGate(repiu::hle::GlideGateId gate_id);

// Task 442, batch three. `grTexSource` is here despite Task 437 excluding it,
// and the reason that exclusion was wrong matters enough to keep in the code:
// **this backend never reads the `GrTexInfo*`**. The gate takes `startAddress`
// alone, and the only thing that can change what lives at that address is a
// download, which bumps the texture generation the key already carries. Task
// 437 reasoned from the Glide specification rather than from this
// implementation.
//
// `grConstantColorValue` and `grDepthMask` are single-argument setters whose
// argument is the whole state, measured repeating 94.5% and 56.7% of the time.
//
// Task 443 promoted this: six gameplay runs showed the census `same` total
// equal to the cache's `elided` count exactly, zero voided entries, zero
// implementation gaps and no visual difference, with `grTexSource` costing 20.9%
// less per call. An explicit `0|off|false` opts out.
bool GlideSetterBatchThreeElisionEnabled();
bool IsGlideSetterBatchThreeElisionGate(repiu::hle::GlideGateId gate_id);

// Task 443, batch four: two setters the game re-issues with a value it never
// changes. Measured over 13,553 gameplay frames, `grFogColorValue` ran 179,717
// times with **one** distinct value and 179,716 repeats, and `grDitherMode`
// 61,041 times with one distinct value and 61,040 repeats -- 13.3 and 4.5 host
// round trips per frame, 1.401% and 0.745% of guest-run, for state that never
// moves.
//
// `grFogTable` is not here: its argument is a pointer to a 64-entry table, so an
// identical pointer does not prove identical contents. That one needs the
// contents in the key, which is a separate piece of work.
//
// Task 444: on by default. The A/B exercised `grDitherMode` only -- the sections
// played never called fog -- but a setter with **one** distinct value across
// 179,717 calls cannot render differently when a repeat is skipped, which is the
// strongest ceiling of any batch so far.
bool GlideSetterBatchFourElisionEnabled();
bool IsGlideSetterBatchFourElisionGate(repiu::hle::GlideGateId gate_id);

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
