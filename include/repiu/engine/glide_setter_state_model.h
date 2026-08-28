#pragma once

#include "repiu/hle/glide_hle.h"

#include <array>
#include <cstdint>

namespace repiu::engine
{

// Task 365: the rules that decide what "the same Glide state" means live here
// alone, because two consumers must agree on them exactly — the Task 364 census,
// which only observes, and the Task 365 cache, which acts on the answer. If the
// observer and the actor could disagree, the census-measured ceiling would stop
// bounding what elision is allowed to do.

// The gate stack mirror holds the return address plus seven argument dwords, so a
// key can carry seven arguments. A wider setter is excluded rather than truncated
// into a colliding key.
constexpr std::size_t kWin32GlideSetterStateKeyWords = 7U;

struct Win32GlideSetterStateKey
{
    std::uint32_t word_count = 0;
    // Monotonic counter incremented by every texture download. Texture-state
    // setters cannot be decided by arguments alone, because a download can change
    // the contents behind an unchanged address. Only texture-dependent gates
    // carry it; every other setter builds its key with zero, so a download does
    // not spuriously invalidate an unrelated mask.
    std::uint32_t texture_generation = 0;
    std::array<std::uint32_t, kWin32GlideSetterStateKeyWords> words = {};
};

bool GlideSetterStateKeysEqual(const Win32GlideSetterStateKey& left,
                               const Win32GlideSetterStateKey& right);

Win32GlideSetterStateKey BuildGlideSetterStateKey(
    const std::uint32_t* argument_words,
    std::uint32_t argument_word_count,
    std::uint32_t texture_generation);

// Gates whose arguments fully describe a piece of render state. Draw, LFB region,
// swap, download, and query gates take pointers, coordinates, or per-call
// payloads and have no "same state" notion, so they are deliberately excluded.
bool IsGlideSetterStateGate(repiu::hle::GlideGateId gate_id);

// Gates after which no previously applied state can be assumed to survive.
bool IsGlideSetterStateInvalidatingGate(repiu::hle::GlideGateId gate_id);

// Gates whose effect can change the contents behind an unchanged texture address,
// so texture-state keys taken before and after must not compare equal.
bool IsGlideSetterStateTextureGenerationGate(repiu::hle::GlideGateId gate_id);

// True when this gate's state depends on the current texture contents.
bool IsGlideSetterStateTextureDependentGate(repiu::hle::GlideGateId gate_id);

}  // namespace repiu::engine
