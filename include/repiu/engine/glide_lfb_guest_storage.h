#ifndef REPIU_ENGINE_GLIDE_LFB_GUEST_STORAGE_H_
#define REPIU_ENGINE_GLIDE_LFB_GUEST_STORAGE_H_

// Task 708. The memory grLfbLock hands the guest.
//
// `GrLfbInfo_t::lfbPtr` is 32 bits and the guest writes through it. On Win32
// x86 the host heap already satisfies that, and the staging surface's own
// buffer was enough; on x86-64 a 600 KB allocation lands above 4 GiB, the
// pointer arrives truncated, and the guest's first write faults -- which is
// exactly where the Linux x64 run stopped once Task 707 let it reach the LFB
// path at all.
//
// This unit owns the reservation that fixes that and installs it into the
// surface. It is separate from `linexe_glide_boundary.cpp` because placing
// memory is not that file's job: the boundary decides what a Glide call means,
// and the integration point there is one idempotent call.
//
// `repiu::hle` deliberately stays out of it. That layer names no operating
// system today, so `GlideLfbSurface` only accepts storage rather than placing
// any, and the placement lives here beside the rest of the engine's memory
// policy.

#include "repiu/runtime/low_address_reservation.h"

#include <cstddef>
#include <cstdint>

namespace repiu::hle
{
class GlideLfbSurface;
}

namespace repiu::engine
{

// Fixed addresses tried in order on a 64-bit host, clear of the low addresses
// already spoken for: the guest arena below about 0x09600000 (Task 551), the
// shadow selector block from 0x1F000000 (Task 586), the AOT code cache from
// 0x20000000 (Task 554), and the engine image at 0x40000000 (Task 503).
inline constexpr std::uintptr_t kGlideLfbStorageCandidateBases[] = {
    0x1D000000U,
    0x25000000U,
    0x2D000000U,
    0x35000000U,
    0x3D000000U,
};

// 640x480 at two bytes per texel. `DecodeGlideResolution` accepts resolution 7
// and refuses every other, so this is not a cap chosen here but the only mode
// this engine opens a window in. A resolution added later makes `Resize` refuse
// rather than silently hand the guest a short buffer.
inline constexpr std::size_t kGlideLfbStorageByteCount = 640U * 480U * 2U;

struct GlideLfbGuestStorage
{
    repiu::runtime::LowAddressReservation reservation;
    // Why it ended the way it did. Always set, on success too, so a caller has
    // one line to log rather than a bool to interpret.
    const char* message = "LFB guest storage was not requested";
    bool installed = false;
};

// Reserves the storage and installs it into `surface`, once.
//
// Idempotent: a storage already installed is left alone and true is returned,
// so the call can sit ahead of every Resize rather than needing an
// initialization order of its own.
//
// Returns false when no address a 32-bit field can name was available. There is
// no fallback to an address above 4 GiB on purpose -- the guest dereferences
// this pointer, so such a reservation would only move the fault.
bool EnsureGlideLfbGuestStorage(GlideLfbGuestStorage* storage,
                                repiu::hle::GlideLfbSurface* surface);

void ReleaseGlideLfbGuestStorage(GlideLfbGuestStorage* storage);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GLIDE_LFB_GUEST_STORAGE_H_
