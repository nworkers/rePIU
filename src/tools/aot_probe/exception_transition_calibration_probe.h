#pragma once

namespace repiu::tools
{

// Prices one Windows exception round trip on this machine (Task 323 Part B-2).
// Reports cycles per INT3 and per TF single-step transition so the loader's
// unaccounted wall-clock bucket can be split into guest execution and kernel
// transition. The result is a lower-bound estimate, never a measurement of the
// live run: the synthetic handler body is minimal and cache/TLB state differs.
bool RunExceptionTransitionCalibrationProbe();

}  // namespace repiu::tools
