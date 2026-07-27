#pragma once

#include "repiu/runtime/runtime_memory.h"

#include <cstdint>

namespace repiu::tools
{

// Task 330 Part B. Measures the platform-neutral plan builder over the real
// image so the same binary code can be timed in Debug and Release, separating
// build-configuration distortion from real cost without running the game.
bool RunPlanBuildBenchmarkProbe(const runtime::RelocatedRuntimeImage& image,
                                std::uint32_t entry_address);

}  // namespace repiu::tools
