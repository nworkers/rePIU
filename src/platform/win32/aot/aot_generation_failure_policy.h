#pragma once

#include <cstdint>

namespace repiu::platform::win32
{

// Task 415. A failed AOT re-translation used to quarantine the entry's whole
// guest page permanently, which dropped every other routine on that page to
// single-stepping. The penalty is now the failing address alone: it is never
// retried, which is the property quarantine provided, while the rest of the
// page keeps running from the cache. These report what the policy did.
//
// Deliberately free of ThreadContext so the host can read the counters without
// pulling in the execution headers.
// See docs/design/20260804-415-generation-failure-address-scope.md.

// Distinct guest addresses whose generation failed and are now suppressed.
std::uint32_t AotGenerationFailureAddressCount();

// Translation attempts skipped because the address had already failed.
std::uint32_t& AotGenerationFailureSkipCount();

// How often the old whole-page quarantine still fired, either through
// `REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE` or the address-set limit.
std::uint32_t& AotGenerationFailureQuarantineCount();

// Task 417. How often a requested entry that straddles a page boundary into a
// retired neighbour was activated anyway, which is the case that previously
// produced the failure this policy then had to absorb.
// `REPIU_AOT_STRICT_SPANNING_ENTRY=1` restores the old refusal.
std::uint32_t& AotSpanningEntryActivationCount();

}  // namespace repiu::platform::win32
