#include "plan_build_benchmark_probe.h"

#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/cycle_clock.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

constexpr std::uint32_t kRepetitions = 5U;

// Reports the build configuration by name so a Debug and a Release log line can
// be told apart without tracking which binary produced them.
const char* BuildConfigurationName()
{
#if defined(_DEBUG) || (defined(_MSC_VER) && !defined(NDEBUG))
    return "debug";
#elif defined(NDEBUG)
    return "release";
#else
    return "unknown";
#endif
}

struct BenchmarkResult
{
    bool valid = false;
    std::uint64_t min_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint32_t instruction_count = 0;
    std::uint32_t block_count = 0;
    std::uint32_t outside_image_target_count = 0;
    std::uint64_t estimated_emitted_bytes = 0;
    runtime::AotPlanBuildProfile fastest_profile;
};

// The minimum across repetitions is reported alongside the mean: the minimum is
// the least cache- and scheduler-polluted sample, and a large gap between them
// is itself evidence about environment cost.
BenchmarkResult MeasurePlanBuild(const runtime::RelocatedRuntimeImage& image,
                                 std::uint32_t entry_address)
{
    BenchmarkResult result;
    const std::vector<runtime::AotExcludedGuestRange> no_excluded_ranges;
    for (std::uint32_t repetition = 0; repetition < kRepetitions; ++repetition)
    {
        runtime::AotTranslationPlan plan;
        runtime::AotPlanBuildProfile profile;
        const std::uint64_t start = runtime::ReadCycleCounter();
        const bool built = runtime::BuildAotTranslationPlanFromEntry(
            image, entry_address, no_excluded_ranges, &plan, &profile);
        const std::uint64_t elapsed =
            runtime::CycleDelta(start, runtime::ReadCycleCounter());
        if (!built || !plan.valid)
        {
            return BenchmarkResult{};
        }
        result.total_cycles += elapsed;
        if (result.min_cycles == 0U || elapsed < result.min_cycles)
        {
            result.min_cycles = elapsed;
            result.fastest_profile = profile;
        }
        result.instruction_count = plan.instruction_count;
        result.block_count = plan.block_count;
        result.outside_image_target_count = plan.outside_image_target_count;
        result.estimated_emitted_bytes = plan.estimated_emitted_bytes;
        result.valid = true;
    }
    return result;
}

// Deterministic checks only. Timing values are reported for comparison across
// build configurations; they are never a pass condition, because a probe that
// failed on a slow machine would be useless.
bool ProfileIsConsistent(const BenchmarkResult& result)
{
    const runtime::AotPlanBuildProfile& profile = result.fastest_profile;
    const std::uint64_t stage_sum =
        profile.decoder_init_cycles + profile.decode_cycles +
        profile.record_build_cycles + profile.classify_cycles +
        profile.walk_cycles + profile.sweep_cycles;
    // `decode_count` also counts failed decodes, so it can only be at or above
    // the instruction count, while every counted instruction builds one record.
    // A backwards TSC read is reported rather than failed on, since it is a
    // machine property and not a defect in what is being measured.
    return profile.enabled &&
        profile.decode_count >= result.instruction_count &&
        profile.record_count == result.instruction_count &&
        profile.sweep_pass_count >= 1U &&
        profile.sweep_record_visit_count >= result.instruction_count &&
        profile.total_cycles != 0U && stage_sum <= profile.total_cycles;
}

}  // namespace

bool RunPlanBuildBenchmarkProbe(const runtime::RelocatedRuntimeImage& image,
                                std::uint32_t entry_address)
{
    const BenchmarkResult result = MeasurePlanBuild(image, entry_address);
    if (!result.valid || result.instruction_count == 0U)
    {
        std::cout << "plan_build_bench_all=false\n";
        return false;
    }

    // A plan build with instrumentation must be identical to one without it,
    // since the stages are observation only.
    const std::vector<runtime::AotExcludedGuestRange> no_excluded_ranges;
    runtime::AotTranslationPlan unprofiled;
    const bool unprofiled_built = runtime::BuildAotTranslationPlanFromEntry(
        image, entry_address, no_excluded_ranges, &unprofiled);
    const bool unchanged = unprofiled_built &&
        unprofiled.block_count == result.block_count &&
        unprofiled.instruction_count == result.instruction_count &&
        unprofiled.outside_image_target_count ==
            result.outside_image_target_count &&
        unprofiled.estimated_emitted_bytes == result.estimated_emitted_bytes;

    const bool consistent = ProfileIsConsistent(result);
    const runtime::AotPlanBuildProfile& profile = result.fastest_profile;
    const std::uint64_t stage_sum =
        profile.decoder_init_cycles + profile.decode_cycles +
        profile.record_build_cycles + profile.classify_cycles +
        profile.walk_cycles + profile.sweep_cycles;
    const std::uint64_t residual = profile.total_cycles > stage_sum
        ? profile.total_cycles - stage_sum
        : 0U;
    const std::uint64_t mean_cycles = result.total_cycles / kRepetitions;
    const auto per_instruction = [&result](std::uint64_t value) {
        return value / result.instruction_count;
    };

    const bool all = consistent && unchanged;
    std::cout << "plan_build_bench_configuration=" << BuildConfigurationName()
              << "\nplan_build_bench_repetitions=" << kRepetitions
              << "\nplan_build_bench_instructions=" << result.instruction_count
              << "\nplan_build_bench_blocks=" << result.block_count
              << "\nplan_build_bench_min_cycles=" << result.min_cycles
              << "\nplan_build_bench_mean_cycles=" << mean_cycles
              << "\nplan_build_bench_cycles_per_instruction="
              << per_instruction(result.min_cycles)
              << "\nplan_build_bench_stage_decoder_init="
              << profile.decoder_init_cycles
              << "\nplan_build_bench_stage_decode=" << profile.decode_cycles
              << "\nplan_build_bench_stage_record_build="
              << profile.record_build_cycles
              << "\nplan_build_bench_stage_classify=" << profile.classify_cycles
              << "\nplan_build_bench_stage_walk=" << profile.walk_cycles
              << "\nplan_build_bench_stage_sweep=" << profile.sweep_cycles
              << "\nplan_build_bench_stage_residual=" << residual
              << "\nplan_build_bench_decode_per_instruction="
              << per_instruction(profile.decode_cycles)
              << "\nplan_build_bench_record_per_instruction="
              << per_instruction(profile.record_build_cycles)
              << "\nplan_build_bench_sweep_passes=" << profile.sweep_pass_count
              << "\nplan_build_bench_sweep_visits="
              << profile.sweep_record_visit_count
              << "\nplan_build_bench_clamped_samples="
              << profile.clamped_sample_count
              << "\nplan_build_bench_profile_consistent="
              << (consistent ? "true" : "false")
              << "\nplan_build_bench_plan_unchanged="
              << (unchanged ? "true" : "false")
              << "\nplan_build_bench_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
