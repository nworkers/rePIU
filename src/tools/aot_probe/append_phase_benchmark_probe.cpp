#include "append_phase_benchmark_probe.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/platform/win32/aot_worker_timing.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/runtime_memory.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::tools
{
namespace
{
#if defined(_WIN32)

// The image entry translates a far larger graph than any in-game translation,
// so it is sampled fewer times; the window-limited sample is the representative
// one and is sampled more.
constexpr std::uint32_t kLargeRepetitions = 3U;
constexpr std::uint32_t kSmallRepetitions = 8U;

// Task 328 measured the in-game translation at 1,039 instructions on average.
constexpr std::uint32_t kInGameInstructionMean = 1039U;
// Tried in order; 0 means "wherever the allocator chooses" and is the last
// resort. The first base whose entry translation also emits wins.
constexpr std::uintptr_t kArenaBaseCandidates[] = {
    0x01000000U, 0x02000000U, 0x03000000U, 0x04000000U, 0x06000000U,
    0x08000000U, 0x0C000000U, 0x20000000U, 0x30000000U, 0x40000000U, 0U};
constexpr std::uint32_t kCandidateWindows[] = {
    0x400U, 0x800U, 0x1000U, 0x2000U, 0x4000U};
// Descending, so the first that translates is the widest available second point.
constexpr std::uint32_t kLargeCandidateWindows[] = {
    0x40000U, 0x20000U, 0x10000U, 0x8000U, 0x4000U};

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

// One append's five phases plus the independently measured total, so a residual
// shows whether the phases partition the call.
struct AppendSample
{
    bool valid = false;
    std::uint32_t append_count = 0;
    std::uint64_t min_total_cycles = 0;
    std::uint64_t mean_total_cycles = 0;
    std::uint64_t arena_snapshot_cycles = 0;
    std::uint64_t plan_build_cycles = 0;
    std::uint64_t image_emit_cycles = 0;
    std::uint64_t validate_cycles = 0;
    std::uint64_t placement_cycles = 0;
    std::uint32_t plan_instruction_count = 0;
    std::uint32_t plan_block_count = 0;
    std::uint32_t emitted_bytes = 0;
    std::uint32_t snapshot_bytes = 0;
    std::uint32_t clamped_sample_count = 0;
    std::uint64_t plan_decode_cycles = 0;
    std::uint64_t plan_classify_cycles = 0;
    std::uint64_t plan_walk_cycles = 0;
    std::uint64_t plan_record_build_cycles = 0;
    std::uint64_t plan_sweep_cycles = 0;
};

std::uint64_t PhaseSum(const AppendSample& sample)
{
    return sample.arena_snapshot_cycles + sample.plan_build_cycles +
        sample.image_emit_cycles + sample.validate_cycles +
        sample.placement_cycles;
}

// Everything outside `[entry, entry + window)` is excluded, so the walk stops at
// a boundary record instead of expanding into the whole program. That is the
// same mechanism the running loader uses for guest-written ranges, and it makes
// the translation size a controlled input rather than a property of whichever
// address happened to be picked.
std::vector<runtime::AotExcludedGuestRange> WindowExclusion(
    std::uint32_t entry, std::uint32_t window)
{
    std::vector<runtime::AotExcludedGuestRange> ranges;
    if (entry != 0U)
    {
        ranges.push_back({0U, entry});
    }
    const std::uint64_t end = static_cast<std::uint64_t>(entry) + window;
    if (end < std::numeric_limits<std::uint32_t>::max())
    {
        const std::uint32_t start = static_cast<std::uint32_t>(end);
        ranges.push_back(
            {start, std::numeric_limits<std::uint32_t>::max() - start});
    }
    return ranges;
}

// A window is usable only if it both plans and emits. The whole image entry
// without a window does not emit ("direct control-flow target is outside the
// cache"), which is itself why every in-game dynamic translation carries
// excluded ranges: out-of-window targets become boundary records the emitter can
// resolve.
bool WindowTranslates(const runtime::RelocatedRuntimeImage& image,
                      std::uint32_t entry,
                      std::uint32_t window,
                      std::uint32_t* instructions)
{
    runtime::AotTranslationPlan plan;
    runtime::AotCodeCacheImage emitted;
    if (!runtime::BuildAotTranslationPlanFromEntry(
            image, entry, WindowExclusion(entry, window), &plan) ||
        !plan.valid || plan.instruction_count == 0U ||
        !runtime::BuildAotCodeCacheImage(plan, &emitted) || !emitted.valid)
    {
        return false;
    }
    if (instructions != nullptr)
    {
        *instructions = plan.instruction_count;
    }
    return true;
}

// Picks the window whose plan lands closest to the in-game mean, so the
// representative sample is chosen by measurement rather than assumed.
std::uint32_t SelectSmallWindow(const runtime::RelocatedRuntimeImage& image,
                                std::uint32_t entry,
                                std::uint32_t* selected_instructions)
{
    std::uint32_t best_window = 0;
    std::uint32_t best_instructions = 0;
    std::uint32_t best_distance = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t window : kCandidateWindows)
    {
        std::uint32_t instructions = 0;
        if (!WindowTranslates(image, entry, window, &instructions))
        {
            continue;
        }
        const std::uint32_t distance = instructions > kInGameInstructionMean
            ? instructions - kInGameInstructionMean
            : kInGameInstructionMean - instructions;
        if (distance < best_distance)
        {
            best_distance = distance;
            best_window = window;
            best_instructions = instructions;
        }
    }
    if (selected_instructions != nullptr)
    {
        *selected_instructions = best_instructions;
    }
    return best_window;
}

// The second point of the fit: the largest window that still translates. The
// wider the gap between the two sizes, the less a fixed per-append cost can hide
// inside the slope.
std::uint32_t SelectLargeWindow(const runtime::RelocatedRuntimeImage& image,
                                std::uint32_t entry,
                                std::uint32_t small_window,
                                std::uint32_t* selected_instructions)
{
    for (std::uint32_t window : kLargeCandidateWindows)
    {
        std::uint32_t instructions = 0;
        if (window > small_window &&
            WindowTranslates(image, entry, window, &instructions))
        {
            if (selected_instructions != nullptr)
            {
                *selected_instructions = instructions;
            }
            return window;
        }
    }
    return 0U;
}

// Runs real appends into a real placement and keeps the fastest repetition,
// which is the least cache- and scheduler-polluted one. The phases come from the
// product code's own instrumentation, so what is measured here is exactly what
// the loader reports in a live run.
bool MeasureAppends(
    std::uint32_t arena_base,
    std::uint32_t arena_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    std::uint32_t repetitions,
    platform::win32::Win32AotCodeCachePlacement* placement,
    platform::win32::Win32AotPageWriteWatchSet* watch_set,
    AppendSample* sample)
{
    if (sample == nullptr)
    {
        return false;
    }
    *sample = AppendSample{};
    std::uint64_t total_sum = 0;
    for (std::uint32_t repetition = 0; repetition < repetitions; ++repetition)
    {
        platform::win32::Win32AotWorkerTimingProfile timing;
        timing.enabled = true;
        platform::win32::Win32AotDynamicAppendResult result;
        const std::uint64_t start =
            platform::win32::ReadAotWorkerTimingCycles();
        const bool called = platform::win32::AppendWin32DynamicAotTranslation(
            arena_base, arena_size, guest_entry, excluded_ranges, watch_set,
            placement, nullptr, &result, &timing);
        const std::uint64_t elapsed = platform::win32::AotWorkerTimingDelta(
            &timing, start, platform::win32::ReadAotWorkerTimingCycles());
        if (!called || !result.appended || timing.append_phase_count != 1U ||
            elapsed == 0U)
        {
            return false;
        }
        total_sum += elapsed;
        ++sample->append_count;
        sample->clamped_sample_count += timing.clamped_sample_count;
        if (sample->min_total_cycles == 0U || elapsed < sample->min_total_cycles)
        {
            sample->min_total_cycles = elapsed;
            sample->arena_snapshot_cycles = timing.arena_snapshot_cycles;
            sample->plan_build_cycles = timing.plan_build_cycles;
            sample->image_emit_cycles = timing.image_emit_cycles;
            sample->validate_cycles = timing.validate_cycles;
            sample->placement_cycles = timing.placement_cycles;
            sample->plan_decode_cycles = timing.plan_decode_cycles;
            sample->plan_classify_cycles = timing.plan_classify_cycles;
            sample->plan_walk_cycles = timing.plan_walk_cycles;
            sample->plan_record_build_cycles = timing.plan_record_build_cycles;
            sample->plan_sweep_cycles = timing.plan_sweep_cycles;
            sample->plan_instruction_count = static_cast<std::uint32_t>(
                timing.plan_instruction_total);
            sample->plan_block_count =
                static_cast<std::uint32_t>(timing.plan_block_total);
            sample->emitted_bytes =
                static_cast<std::uint32_t>(timing.emitted_byte_total);
            sample->snapshot_bytes =
                static_cast<std::uint32_t>(timing.snapshot_byte_total);
        }
    }
    sample->mean_total_cycles =
        sample->append_count != 0U ? total_sum / sample->append_count : 0U;
    sample->valid = sample->append_count == repetitions;
    return sample->valid;
}

void PrintSample(const char* prefix, const AppendSample& sample)
{
    const std::uint64_t phase_sum = PhaseSum(sample);
    const std::uint64_t residual = sample.min_total_cycles > phase_sum
        ? sample.min_total_cycles - phase_sum
        : 0U;
    const std::uint32_t instructions =
        sample.plan_instruction_count != 0U ? sample.plan_instruction_count : 1U;
    std::cout << "append_bench_" << prefix << "_appends="
              << sample.append_count
              << "\nappend_bench_" << prefix << "_instructions="
              << sample.plan_instruction_count
              << "\nappend_bench_" << prefix << "_blocks="
              << sample.plan_block_count
              << "\nappend_bench_" << prefix << "_emitted_bytes="
              << sample.emitted_bytes
              << "\nappend_bench_" << prefix << "_snapshot_bytes="
              << sample.snapshot_bytes
              << "\nappend_bench_" << prefix << "_min_total="
              << sample.min_total_cycles
              << "\nappend_bench_" << prefix << "_mean_total="
              << sample.mean_total_cycles
              << "\nappend_bench_" << prefix << "_arena_snapshot="
              << sample.arena_snapshot_cycles
              << "\nappend_bench_" << prefix << "_plan_build="
              << sample.plan_build_cycles
              << "\nappend_bench_" << prefix << "_image_emit="
              << sample.image_emit_cycles
              << "\nappend_bench_" << prefix << "_validate="
              << sample.validate_cycles
              << "\nappend_bench_" << prefix << "_placement="
              << sample.placement_cycles
              << "\nappend_bench_" << prefix << "_residual=" << residual
              << "\nappend_bench_" << prefix << "_cycles_per_instruction="
              << sample.min_total_cycles / instructions
              << "\nappend_bench_" << prefix << "_plan_decode="
              << sample.plan_decode_cycles
              << "\nappend_bench_" << prefix << "_plan_classify="
              << sample.plan_classify_cycles
              << "\nappend_bench_" << prefix << "_plan_walk="
              << sample.plan_walk_cycles
              << "\nappend_bench_" << prefix << "_plan_record_build="
              << sample.plan_record_build_cycles
              << "\nappend_bench_" << prefix << "_plan_sweep="
              << sample.plan_sweep_cycles
              << "\nappend_bench_" << prefix << "_clamped_samples="
              << sample.clamped_sample_count << "\n";
}

// Share of one append in basis points, so no floating point enters a probe whose
// output is compared across configurations.
std::uint64_t ShareInBasisPoints(std::uint64_t part, std::uint64_t whole)
{
    return whole != 0U ? part * 10000U / whole : 0U;
}

// Two sizes give one linear fit per phase: the slope is the per-instruction
// cost and the intercept is what one append costs regardless of how much it
// translates. A phase whose cost is mostly intercept cannot be improved by
// translating less.
void PrintLinearFit(const char* name,
                    std::uint64_t small_cycles,
                    std::uint32_t small_instructions,
                    std::uint64_t large_cycles,
                    std::uint32_t large_instructions)
{
    std::uint64_t slope = 0;
    std::uint64_t fixed = 0;
    if (large_instructions > small_instructions && large_cycles > small_cycles)
    {
        slope = (large_cycles - small_cycles) /
            (large_instructions - small_instructions);
        const std::uint64_t variable = slope * small_instructions;
        fixed = small_cycles > variable ? small_cycles - variable : 0U;
    }
    std::cout << "append_bench_fit_" << name << "_per_instruction=" << slope
              << "\nappend_bench_fit_" << name << "_fixed=" << fixed << "\n";
}

#endif  // defined(_WIN32)
}  // namespace

bool RunAppendPhaseBenchmarkProbe(const exe::Dos4gwLoadResult& load)
{
#if !defined(_WIN32)
    static_cast<void>(load);
    return true;
#else
    // The image span is needed before an arena can be reserved, and the arena's
    // address is needed before the image can be relocated into it, so the image
    // is relocated twice: once to measure, once for real.
    exe::ParseError error;
    runtime::RelocatableRuntimeImagePlan probe_plan;
    if (!runtime::BuildRelocatableRuntimeImagePlan(
            load, 0x01000000U, &probe_plan, &error) || !probe_plan.valid)
    {
        std::cout << "append_bench_all=false\n";
        return false;
    }
    std::uint64_t span = 0;
    for (const runtime::RelocatableRuntimeObjectRegion& region :
         probe_plan.object_regions)
    {
        const std::uint64_t end =
            static_cast<std::uint64_t>(region.relocated_base_address) +
            region.virtual_size - probe_plan.relocated_image_base;
        span = std::max(span, end);
    }
    if (span == 0U || span > std::numeric_limits<std::uint32_t>::max())
    {
        std::cout << "append_bench_all=false\n";
        return false;
    }

    // Which base the arena gets changes the plan, because the guest's own
    // absolute addresses move with it: at some bases a direct target ends up
    // outside every object and the emitter refuses the whole image. The
    // unwindowed entry translation is the wide second point of the fit, so bases
    // are tried until one produces a translation that emits, and only if none
    // does is a windowed second point used instead.
    std::uint8_t* arena = nullptr;
    std::uint32_t arena_base = 0;
    const std::uint32_t arena_size = static_cast<std::uint32_t>(span);
    runtime::RelocatedRuntimeImage image;
    bool prepared = false;
    bool entry_translates = false;
    for (std::uintptr_t candidate : kArenaBaseCandidates)
    {
        void* reserved = candidate != 0U
            ? VirtualAlloc(reinterpret_cast<void*>(candidate),
                           static_cast<SIZE_T>(span),
                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
            : VirtualAlloc(nullptr, static_cast<SIZE_T>(span),
                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (reserved == nullptr ||
            reinterpret_cast<std::uintptr_t>(reserved) >
                std::numeric_limits<std::uint32_t>::max())
        {
            if (reserved != nullptr)
            {
                VirtualFree(reserved, 0, MEM_RELEASE);
            }
            continue;
        }
        const std::uint32_t base = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(reserved));

        // Relocate for real, then move the bytes to the addresses they were
        // relocated for. The owning copy stays as the plan-building source for
        // window calibration; the append path reads the live arena directly.
        runtime::RelocatableRuntimeImagePlan plan_at_arena;
        runtime::RelocatedRuntimeImage candidate_image;
        bool candidate_prepared = runtime::BuildRelocatableRuntimeImagePlan(
                                      load, base, &plan_at_arena, &error) &&
            plan_at_arena.valid &&
            runtime::BuildRelocatedRuntimeImage(load, plan_at_arena,
                                                &candidate_image, &error) &&
            candidate_image.valid;
        if (candidate_prepared)
        {
            auto* bytes = static_cast<std::uint8_t*>(reserved);
            for (const runtime::RelocatedRuntimeObject& object :
                 candidate_image.objects)
            {
                const std::uint64_t offset =
                    static_cast<std::uint64_t>(object.relocated_base_address) -
                    base;
                if (offset + object.memory.size() > arena_size)
                {
                    candidate_prepared = false;
                    break;
                }
                std::memcpy(bytes + offset, object.memory.data(),
                            object.memory.size());
            }
        }
        if (!candidate_prepared)
        {
            VirtualFree(reserved, 0, MEM_RELEASE);
            continue;
        }

        runtime::AotTranslationPlan entry_plan;
        runtime::AotCodeCacheImage entry_image;
        const bool translates = runtime::BuildAotTranslationPlanFromEntry(
                                    candidate_image,
                                    candidate_image
                                        .relocated_entry_linear_address,
                                    &entry_plan) &&
            entry_plan.valid &&
            runtime::BuildAotCodeCacheImage(entry_plan, &entry_image) &&
            entry_image.valid;
        if (!translates && arena != nullptr)
        {
            // A usable fallback arena is already held, so keep it.
            VirtualFree(reserved, 0, MEM_RELEASE);
            continue;
        }
        if (arena != nullptr)
        {
            VirtualFree(arena, 0, MEM_RELEASE);
        }
        arena = static_cast<std::uint8_t*>(reserved);
        arena_base = base;
        image = std::move(candidate_image);
        prepared = true;
        entry_translates = translates;
        if (translates)
        {
            break;
        }
    }
    if (!prepared)
    {
        std::cout << "append_bench_all=false\n";
        return false;
    }

    const std::uint32_t guest_entry = image.relocated_entry_linear_address;
    std::uint32_t window_instructions = 0;
    std::uint32_t large_window_instructions = 0;
    const std::uint32_t window =
        SelectSmallWindow(image, guest_entry, &window_instructions);
    const std::uint32_t large_window = entry_translates || window == 0U
        ? 0U
        : SelectLargeWindow(image, guest_entry, window,
                            &large_window_instructions);
    const std::vector<runtime::AotExcludedGuestRange> large_exclusion =
        entry_translates ? std::vector<runtime::AotExcludedGuestRange>{}
                         : WindowExclusion(guest_entry, large_window);

    // The seed exists so appends run against a real placement; its own size is
    // not the quantity under measurement, and the append cost per entry does not
    // depend on how many entries preceded it, since registration and indexing
    // are amortized O(1) and the relink lookup is a hash.
    runtime::AotTranslationPlan seed_plan;
    runtime::AotCodeCacheImage seed_image;
    platform::win32::Win32AotCodeCachePlacement placement;
    const bool seed_planned = window != 0U &&
        runtime::BuildAotTranslationPlanFromEntry(
            image, guest_entry, WindowExclusion(guest_entry, window),
            &seed_plan) && seed_plan.valid;
    const bool seed_emitted = seed_planned &&
        runtime::BuildAotCodeCacheImage(seed_plan, &seed_image) &&
        seed_image.valid;
    const bool seeded = seed_emitted &&
        platform::win32::PlaceWin32AotCodeCache(seed_image, &placement) &&
        placement.placed;

    platform::win32::Win32AotPageWriteWatchSet watches;
    AppendSample small_sample;
    AppendSample large_sample;
    const bool measured = seeded && window != 0U &&
        (entry_translates || large_window != 0U) &&
        MeasureAppends(arena_base, arena_size, guest_entry,
                       WindowExclusion(guest_entry, window), kSmallRepetitions,
                       &placement, &watches, &small_sample) &&
        MeasureAppends(arena_base, arena_size, guest_entry, large_exclusion,
                       kLargeRepetitions, &placement, &watches, &large_sample);

    // Releasing the placement resets it, so anything reported about it is read
    // first.
    const std::string placement_message = placement.message;
    platform::win32::RestoreWin32AotGuestPageWriteWatches(&watches);
    platform::win32::ReleaseWin32AotCodeCache(&placement);
    VirtualFree(arena, 0, MEM_RELEASE);

    if (!measured)
    {
        std::cout << "append_bench_arena_base=" << arena_base
                  << "\nappend_bench_prepared=" << (prepared ? "true" : "false")
                  << "\nappend_bench_seed_planned="
                  << (seed_planned ? "true" : "false")
                  << "\nappend_bench_seed_instructions="
                  << seed_plan.instruction_count
                  << "\nappend_bench_seed_message=" << seed_plan.message
                  << "\nappend_bench_seed_emitted="
                  << (seed_emitted ? "true" : "false")
                  << "\nappend_bench_seed_image_message=" << seed_image.message
                  << "\nappend_bench_seeded=" << (seeded ? "true" : "false")
                  << "\nappend_bench_placement_message=" << placement_message
                  << "\nappend_bench_small_appends=" << small_sample.append_count
                  << "\nappend_bench_window=" << window
                  << "\nappend_bench_large_window=" << large_window
                  << "\nappend_bench_all=false\n";
        return false;
    }

    // The snapshot must stay gone (Task 329), the phases must partition the
    // call, and the small sample must actually be the smaller translation.
    const bool snapshot_removed = small_sample.snapshot_bytes == 0U &&
        large_sample.snapshot_bytes == 0U;
    const bool partitioned =
        PhaseSum(small_sample) <= small_sample.min_total_cycles &&
        PhaseSum(large_sample) <= large_sample.min_total_cycles;
    const bool scaled = small_sample.plan_instruction_count <
        large_sample.plan_instruction_count;
    const bool all = snapshot_removed && partitioned && scaled;

    std::cout << "append_bench_configuration=" << BuildConfigurationName()
              << "\nappend_bench_arena_bytes=" << arena_size
              << "\nappend_bench_seed_map_entries=" << seed_image.address_map.size()
              << "\nappend_bench_window=" << window
              << "\nappend_bench_window_instructions=" << window_instructions
              << "\nappend_bench_large_window=" << large_window
              << "\nappend_bench_large_window_instructions="
              << large_window_instructions
              << "\nappend_bench_entry_translates="
              << (entry_translates ? "true" : "false") << "\n";
    PrintSample("small", small_sample);
    PrintSample("large", large_sample);
    std::cout << "append_bench_small_share_plan_build_bp="
              << ShareInBasisPoints(small_sample.plan_build_cycles,
                                    small_sample.min_total_cycles)
              << "\nappend_bench_small_share_image_emit_bp="
              << ShareInBasisPoints(small_sample.image_emit_cycles,
                                    small_sample.min_total_cycles)
              << "\nappend_bench_small_share_placement_bp="
              << ShareInBasisPoints(small_sample.placement_cycles,
                                    small_sample.min_total_cycles)
              << "\nappend_bench_small_share_validate_bp="
              << ShareInBasisPoints(small_sample.validate_cycles,
                                    small_sample.min_total_cycles)
              << "\n";
    PrintLinearFit("plan_build", small_sample.plan_build_cycles,
                   small_sample.plan_instruction_count,
                   large_sample.plan_build_cycles,
                   large_sample.plan_instruction_count);
    PrintLinearFit("image_emit", small_sample.image_emit_cycles,
                   small_sample.plan_instruction_count,
                   large_sample.image_emit_cycles,
                   large_sample.plan_instruction_count);
    PrintLinearFit("placement", small_sample.placement_cycles,
                   small_sample.plan_instruction_count,
                   large_sample.placement_cycles,
                   large_sample.plan_instruction_count);
    PrintLinearFit("validate", small_sample.validate_cycles,
                   small_sample.plan_instruction_count,
                   large_sample.validate_cycles,
                   large_sample.plan_instruction_count);
    std::cout << "append_bench_snapshot_removed="
              << (snapshot_removed ? "true" : "false")
              << "\nappend_bench_partitioned="
              << (partitioned ? "true" : "false")
              << "\nappend_bench_scaled=" << (scaled ? "true" : "false")
              << "\nappend_bench_all=" << (all ? "true" : "false") << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
