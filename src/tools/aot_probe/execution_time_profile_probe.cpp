#include "execution_time_profile_probe.h"

#include "repiu/platform/win32/execution_time_profile.h"

#include <cstdint>
#include <iostream>
#include <memory>

namespace repiu::tools
{

bool RunExecutionTimeProfileProbe()
{
    using namespace repiu::platform::win32;

    const bool policy =
        !ResolveExecutionTimeProfileEnabled("") &&
        ResolveExecutionTimeProfileEnabled("1") &&
        ResolveExecutionTimeProfileEnabled("on") &&
        ResolveExecutionTimeProfileEnabled("true") &&
        !ResolveExecutionTimeProfileEnabled("0") &&
        !ResolveExecutionTimeProfileEnabled("off") &&
        !ResolveExecutionTimeProfileEnabled("invalid");

    const auto index_of = [](ExecutionTimeBucket bucket) {
        return static_cast<std::uint32_t>(bucket);
    };

    // Direct accumulation, including the inside-VEH split.
    auto profile = std::make_unique<Win32ExecutionTimeProfile>();
    RecordExecutionTimeBucket(
        profile.get(), ExecutionTimeBucket::kGlideGate, 100U, false);
    RecordExecutionTimeBucket(
        profile.get(), ExecutionTimeBucket::kGlideGate, 40U, true);
    const Win32ExecutionTimeProfileSnapshot direct =
        SnapshotExecutionTimeProfile(*profile);
    const bool accumulation =
        direct.enabled &&
        direct.cycles[index_of(ExecutionTimeBucket::kGlideGate)] == 140U &&
        direct.counts[index_of(ExecutionTimeBucket::kGlideGate)] == 2U &&
        direct.inside_veh_cycles[
            index_of(ExecutionTimeBucket::kGlideGate)] == 40U &&
        direct.inside_veh_counts[
            index_of(ExecutionTimeBucket::kGlideGate)] == 1U;

    // A service scope opened inside a VEH scope must be tagged inside, and one
    // opened outside must not be. Nested VEH frames must attribute once.
    auto nested = std::make_unique<Win32ExecutionTimeProfile>();
    {
        const ExecutionTimeScope outside(
            nested.get(), ExecutionTimeBucket::kPortIoDevice);
        (void)outside;
    }
    {
        const ExecutionTimeScope veh(
            nested.get(), ExecutionTimeBucket::kVehTotal);
        const ExecutionTimeScope inner_veh(
            nested.get(), ExecutionTimeBucket::kVehTotal);
        const ExecutionTimeScope inside(
            nested.get(), ExecutionTimeBucket::kPortIoDevice);
        (void)veh;
        (void)inner_veh;
        (void)inside;
    }
    const Win32ExecutionTimeProfileSnapshot nested_snapshot =
        SnapshotExecutionTimeProfile(*nested);
    const bool depth_tracking =
        nested_snapshot.counts[
            index_of(ExecutionTimeBucket::kPortIoDevice)] == 2U &&
        nested_snapshot.inside_veh_counts[
            index_of(ExecutionTimeBucket::kPortIoDevice)] == 1U &&
        // Only the outermost VEH frame records, so a re-entrant fault inside a
        // handler cannot double count.
        nested_snapshot.counts[
            index_of(ExecutionTimeBucket::kVehTotal)] == 1U &&
        nested->veh_depth == 0U;

    // Task 325 invariant: the sub-buckets decompose kVehTotal, so their sum can
    // never exceed it when they are opened inside it.
    auto veh_profile = std::make_unique<Win32ExecutionTimeProfile>();
    {
        const ExecutionTimeScope veh(
            veh_profile.get(), ExecutionTimeBucket::kVehTotal);
        {
            const ExecutionTimeScope prologue(
                veh_profile.get(), ExecutionTimeBucket::kVehPrologue);
            (void)prologue;
        }
        {
            const ExecutionTimeScope transfer(
                veh_profile.get(), ExecutionTimeBucket::kVehAotTransfer);
            (void)transfer;
        }
        {
            const ExecutionTimeScope chain(
                veh_profile.get(), ExecutionTimeBucket::kVehHleChain);
            (void)chain;
        }
        (void)veh;
    }
    const Win32ExecutionTimeProfileSnapshot veh_snapshot =
        SnapshotExecutionTimeProfile(*veh_profile);
    std::uint64_t sub_bucket_cycles = 0;
    for (std::uint32_t index = kFirstVehSubBucket;
         index < kExecutionTimeBucketCount; ++index)
    {
        sub_bucket_cycles += veh_snapshot.cycles[index];
    }
    const bool veh_decomposition =
        veh_snapshot.counts[index_of(ExecutionTimeBucket::kVehTotal)] == 1U &&
        veh_snapshot.counts[
            index_of(ExecutionTimeBucket::kVehPrologue)] == 1U &&
        veh_snapshot.counts[
            index_of(ExecutionTimeBucket::kVehAotTransfer)] == 1U &&
        veh_snapshot.counts[
            index_of(ExecutionTimeBucket::kVehHleChain)] == 1U &&
        sub_bucket_cycles <=
            veh_snapshot.cycles[index_of(ExecutionTimeBucket::kVehTotal)];

    // Appending the Task 325 buckets must not have shifted the original five.
    const bool stable_indices =
        index_of(ExecutionTimeBucket::kGuestRunTotal) == 0U &&
        index_of(ExecutionTimeBucket::kVehTotal) == 1U &&
        index_of(ExecutionTimeBucket::kGlideGate) == 2U &&
        index_of(ExecutionTimeBucket::kPortIoDevice) == 3U &&
        index_of(ExecutionTimeBucket::kDosService) == 4U &&
        kFirstVehSubBucket == 5U &&
        // Task 326 appended two axes after the VEH sub-buckets. The reported
        // VEH residual sums only [kFirstVehSubBucket, kFirstAotHandlerBucket),
        // so this ordering is load-bearing, not cosmetic.
        kFirstAotHandlerBucket == 10U &&
        kFirstAotFunctionBucket == 16U &&
        index_of(ExecutionTimeBucket::kAotReentry) == 12U &&
        index_of(ExecutionTimeBucket::kAotResidency) == 19U &&
        kExecutionTimeBucketCount == 20U;

    // A disabled profile is a null pointer at every call site.
    const ExecutionTimeScope inert(nullptr, ExecutionTimeBucket::kVehTotal);
    (void)inert;
    RecordExecutionTimeBucket(
        nullptr, ExecutionTimeBucket::kVehHleChain, 10U, true);
    const Win32ExecutionTimeProfileSnapshot empty =
        SnapshotExecutionTimeProfile(Win32ExecutionTimeProfile{});
    bool disabled = !empty.enabled;
    for (std::uint32_t index = 0; index < kExecutionTimeBucketCount; ++index)
    {
        disabled = disabled && empty.cycles[index] == 0U &&
            empty.counts[index] == 0U;
    }

    // Task 326: the function axis nests inside the handler axis, and each axis
    // must stay within kVehAotTransfer on its own. Summing the two axes would
    // exceed it, which is why reporting never adds them together.
    auto axes_profile = std::make_unique<Win32ExecutionTimeProfile>();
    {
        const ExecutionTimeScope transfer(
            axes_profile.get(), ExecutionTimeBucket::kVehAotTransfer);
        {
            const ExecutionTimeScope handler(
                axes_profile.get(), ExecutionTimeBucket::kAotReentry);
            const ExecutionTimeScope resolve(
                axes_profile.get(), ExecutionTimeBucket::kAotTransferResolve);
            const ExecutionTimeScope residency(
                axes_profile.get(), ExecutionTimeBucket::kAotResidency);
            (void)handler;
            (void)resolve;
            (void)residency;
        }
        (void)transfer;
    }
    const Win32ExecutionTimeProfileSnapshot axes =
        SnapshotExecutionTimeProfile(*axes_profile);
    std::uint64_t handler_axis = 0;
    for (std::uint32_t index = kFirstAotHandlerBucket;
         index < kFirstAotFunctionBucket; ++index)
    {
        handler_axis += axes.cycles[index];
    }
    std::uint64_t function_axis = 0;
    for (std::uint32_t index = kFirstAotFunctionBucket;
         index < kExecutionTimeBucketCount; ++index)
    {
        function_axis += axes.cycles[index];
    }
    const std::uint64_t transfer_total =
        axes.cycles[index_of(ExecutionTimeBucket::kVehAotTransfer)];
    const bool nested_axes =
        axes.counts[index_of(ExecutionTimeBucket::kAotReentry)] == 1U &&
        axes.counts[
            index_of(ExecutionTimeBucket::kAotTransferResolve)] == 1U &&
        axes.counts[index_of(ExecutionTimeBucket::kAotResidency)] == 1U &&
        handler_axis <= transfer_total &&
        function_axis <= transfer_total;

    const bool all = policy && accumulation && depth_tracking &&
        veh_decomposition && stable_indices && nested_axes && disabled;

    std::cout
        << "execution_time_profile_policy=" << (policy ? "true" : "false")
        << "\nexecution_time_profile_accumulation="
        << (accumulation ? "true" : "false")
        << "\nexecution_time_profile_depth_tracking="
        << (depth_tracking ? "true" : "false")
        << "\nexecution_time_profile_veh_decomposition="
        << (veh_decomposition ? "true" : "false")
        << "\nexecution_time_profile_stable_indices="
        << (stable_indices ? "true" : "false")
        << "\nexecution_time_profile_nested_axes="
        << (nested_axes ? "true" : "false")
        << "\nexecution_time_profile_disabled="
        << (disabled ? "true" : "false")
        << "\nexecution_time_profile_all=" << (all ? "true" : "false")
        << "\n";
    return all;
}

}  // namespace repiu::tools
