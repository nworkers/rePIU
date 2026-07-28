#pragma once

namespace repiu::tools
{

// Task 333. Checks the rendezvous decomposition's arithmetic deterministically:
// which interval each timestamp pair lands in, that the four named intervals
// account for the measured total, that direct host-thread calls stay off the
// rendezvous axis, and that a backwards TSC read clamps instead of wrapping.
bool RunGlideGateTimingProbe();

}  // namespace repiu::tools
