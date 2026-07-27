#pragma once

namespace repiu::tools
{

// Verifies the Task 323 execution-time buckets and the Task 325 VEH
// decomposition: accumulation, VEH depth tracking under nesting, the
// sum(VEH sub-buckets) <= kVehTotal invariant, and that a disabled profile
// accumulates nothing.
bool RunExecutionTimeProfileProbe();

}  // namespace repiu::tools
