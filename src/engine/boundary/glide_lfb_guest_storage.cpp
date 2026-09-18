#include "repiu/engine/glide_lfb_guest_storage.h"

#include "repiu/hle/glide_lfb.h"

namespace repiu::engine
{
namespace
{

constexpr std::size_t kCandidateCount =
    sizeof(kGlideLfbStorageCandidateBases) /
    sizeof(kGlideLfbStorageCandidateBases[0]);

}  // namespace

bool EnsureGlideLfbGuestStorage(GlideLfbGuestStorage* const storage,
                                repiu::hle::GlideLfbSurface* const surface)
{
    if (storage == nullptr || surface == nullptr)
    {
        return false;
    }
    if (storage->installed)
    {
        return true;
    }

    repiu::runtime::LowAddressReservationRequest request;
    request.candidate_bases = kGlideLfbStorageCandidateBases;
    request.candidate_count = kCandidateCount;
    request.capacity = kGlideLfbStorageByteCount;
    // No unhinted last resort. The guest dereferences this address, so a
    // reservation above 4 GiB would not be a weaker success but the same fault
    // one step later, with a mapping to release on the way out.
    request.allow_unhinted_fallback = false;

    const repiu::runtime::LowAddressReservation reservation =
        repiu::runtime::ReserveLowAddressMemory(request);
    if (!reservation.valid || !reservation.fits_32bit)
    {
        if (reservation.valid)
        {
            repiu::runtime::ReleaseLowAddressMemory(reservation);
        }
        storage->message =
            "no address below 4GiB was available for the Glide LFB staging "
            "surface";
        return false;
    }

    if (!surface->UseExternalStorage(
            static_cast<std::uint8_t*>(reservation.base),
            kGlideLfbStorageByteCount))
    {
        repiu::runtime::ReleaseLowAddressMemory(reservation);
        storage->message = "Glide LFB staging surface refused the storage";
        return false;
    }

    storage->reservation = reservation;
    storage->installed = true;
    storage->message = "Glide LFB staging surface placed below 4GiB";
    return true;
}

void ReleaseGlideLfbGuestStorage(GlideLfbGuestStorage* const storage)
{
    if (storage == nullptr)
    {
        return;
    }
    repiu::runtime::ReleaseLowAddressMemory(storage->reservation);
    storage->reservation = {};
    storage->installed = false;
}

}  // namespace repiu::engine
