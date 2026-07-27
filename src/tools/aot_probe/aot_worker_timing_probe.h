#pragma once

namespace repiu::tools
{

// Verifies the Task 327 rendezvous timing accumulators: the four intervals, the
// backwards-TSC clamp, other-operation counting, request spacing, and inertness
// when the profile is disabled.
bool RunAotWorkerTimingProbe();

}  // namespace repiu::tools
