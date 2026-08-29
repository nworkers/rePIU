#include "glide_swap_interval_policy_probe.h"

#include "repiu/engine/glide_swap_interval_policy.h"

#include <iostream>

namespace repiu::tools
{

bool RunGlideSwapIntervalPolicyProbe()
{
    using engine::ResolveGlideSwapIntervalOverride;
    using engine::GlideSwapIntervalPolicySnapshot;

    std::int32_t interval = 99;
    const bool accepted =
        ResolveGlideSwapIntervalOverride("-1", &interval) && interval == -1 &&
        ResolveGlideSwapIntervalOverride("0", &interval) && interval == 0 &&
        ResolveGlideSwapIntervalOverride("1", &interval) && interval == 1 &&
        ResolveGlideSwapIntervalOverride("4", &interval) && interval == 4;

    // Out-of-range and malformed values are rejected rather than clamped: a
    // mistyped variable must not quietly select a different measurement than the
    // one being compared against.
    std::int32_t untouched = 7;
    const bool rejected =
        !ResolveGlideSwapIntervalOverride("5", &untouched) &&
        !ResolveGlideSwapIntervalOverride("-2", &untouched) &&
        !ResolveGlideSwapIntervalOverride("1 ", &untouched) &&
        !ResolveGlideSwapIntervalOverride(" 1", &untouched) &&
        !ResolveGlideSwapIntervalOverride("x", &untouched) &&
        !ResolveGlideSwapIntervalOverride("", &untouched) &&
        !ResolveGlideSwapIntervalOverride("1", nullptr) &&
        untouched == 7;

    // A default-constructed snapshot must read as "no override requested" so an
    // unset run is distinguishable from one that requested interval zero.
    const GlideSwapIntervalPolicySnapshot inert_snapshot;
    const bool inert =
        !inert_snapshot.override_requested && !inert_snapshot.applied &&
        !inert_snapshot.effective_valid &&
        inert_snapshot.requested_interval == 0 &&
        inert_snapshot.effective_interval == 0;

    // A refused override must stay visible: requested true with applied false is
    // the state that would otherwise silently invalidate an A/B.
    GlideSwapIntervalPolicySnapshot refused;
    refused.override_requested = true;
    refused.requested_interval = 0;
    refused.applied = false;
    refused.effective_valid = true;
    refused.effective_interval = 1;
    const bool refusal_visible = refused.override_requested &&
        !refused.applied && refused.effective_interval == 1;

    const bool all = accepted && rejected && inert && refusal_visible;
    std::cout << "glide_swap_interval_accepted="
              << (accepted ? "true" : "false")
              << "\nglide_swap_interval_rejected="
              << (rejected ? "true" : "false")
              << "\nglide_swap_interval_inert=" << (inert ? "true" : "false")
              << "\nglide_swap_interval_refusal_visible="
              << (refusal_visible ? "true" : "false")
              << "\nglide_swap_interval_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
