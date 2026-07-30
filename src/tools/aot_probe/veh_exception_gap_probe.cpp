#include "veh_exception_gap_probe.h"

#include "repiu/platform/win32/execution_time_profile.h"

#include <iostream>

namespace repiu::tools
{

bool RunVehExceptionGapProbe()
{
    using platform::win32::ExecutionTimeBucket;
    using platform::win32::ExecutionTimeScope;
    using platform::win32::RecordVehExceptionGap;
    using platform::win32::SnapshotExecutionTimeProfile;
    using platform::win32::VehGapClass;
    using platform::win32::Win32ExecutionTimeProfile;

    // The first VEH frame has no predecessor, so it must bank nothing: an
    // interval measured from a zero timestamp would be the whole run.
    Win32ExecutionTimeProfile first;
    {
        const ExecutionTimeScope scope(&first, ExecutionTimeBucket::kVehTotal);
        RecordVehExceptionGap(&first, VehGapClass::kSingleStep);
    }
    const auto first_snapshot = SnapshotExecutionTimeProfile(first);
    const bool first_frame_banks_nothing =
        first_snapshot.veh_gap_counts[0] == 0U &&
        first_snapshot.veh_gap_cycles[0] == 0U &&
        first_snapshot.veh_gap_min_cycles == 0U;

    // Two further frames each bank the interval since the previous exit, and the
    // class comes from the census call rather than the scope.
    Win32ExecutionTimeProfile profile;
    const auto run_frame = [&profile](VehGapClass gap_class) {
        const ExecutionTimeScope scope(&profile,
                                       ExecutionTimeBucket::kVehTotal);
        RecordVehExceptionGap(&profile, gap_class);
    };
    run_frame(VehGapClass::kSingleStep);
    run_frame(VehGapClass::kSingleStep);
    run_frame(VehGapClass::kBreakpoint);
    run_frame(VehGapClass::kOther);
    const auto snapshot = SnapshotExecutionTimeProfile(profile);
    const bool classified =
        snapshot.veh_gap_counts[0] == 1U &&
        snapshot.veh_gap_counts[1] == 1U &&
        snapshot.veh_gap_counts[2] == 1U;

    // Classification moves the interval out of the residual, so the three classes
    // plus the residual always sum to what was banked.
    const std::uint64_t classified_total = snapshot.veh_gap_cycles[0] +
        snapshot.veh_gap_cycles[1] + snapshot.veh_gap_cycles[2];
    const bool residual_drained =
        snapshot.veh_gap_unclassified_cycles == 0U && classified_total != 0U;

    const bool extremes_tracked =
        snapshot.veh_gap_min_cycles != 0U &&
        snapshot.veh_gap_max_cycles >= snapshot.veh_gap_min_cycles;

    // An unclassified frame keeps its interval in the residual rather than
    // leaking it into the next exception's measurement.
    Win32ExecutionTimeProfile unclassified;
    {
        const ExecutionTimeScope scope(&unclassified,
                                       ExecutionTimeBucket::kVehTotal);
    }
    {
        const ExecutionTimeScope scope(&unclassified,
                                       ExecutionTimeBucket::kVehTotal);
    }
    const auto unclassified_snapshot =
        SnapshotExecutionTimeProfile(unclassified);
    const bool residual_retained =
        unclassified_snapshot.veh_gap_unclassified_cycles != 0U &&
        unclassified_snapshot.veh_gap_counts[0] == 0U;

    // A nested fault inside a handler must not bank an interval that never
    // contained guest execution, so only the outermost frame attributes.
    Win32ExecutionTimeProfile nested;
    {
        const ExecutionTimeScope outer(&nested,
                                       ExecutionTimeBucket::kVehTotal);
        {
            const ExecutionTimeScope inner(&nested,
                                           ExecutionTimeBucket::kVehTotal);
            RecordVehExceptionGap(&nested, VehGapClass::kSingleStep);
        }
    }
    const bool nested_ignored =
        SnapshotExecutionTimeProfile(nested).veh_gap_counts[0] == 0U;

    Win32ExecutionTimeProfile untouched;
    RecordVehExceptionGap(nullptr, VehGapClass::kSingleStep);
    RecordVehExceptionGap(&untouched, VehGapClass::kSingleStep);
    const bool inert =
        SnapshotExecutionTimeProfile(untouched).veh_gap_counts[0] == 0U;

    const bool all = first_frame_banks_nothing && classified &&
        residual_drained && extremes_tracked && residual_retained &&
        nested_ignored && inert;
    std::cout << "veh_gap_first_frame_banks_nothing="
              << (first_frame_banks_nothing ? "true" : "false")
              << "\nveh_gap_classified=" << (classified ? "true" : "false")
              << "\nveh_gap_residual_drained="
              << (residual_drained ? "true" : "false")
              << "\nveh_gap_extremes_tracked="
              << (extremes_tracked ? "true" : "false")
              << "\nveh_gap_residual_retained="
              << (residual_retained ? "true" : "false")
              << "\nveh_gap_nested_ignored="
              << (nested_ignored ? "true" : "false")
              << "\nveh_gap_inert=" << (inert ? "true" : "false")
              << "\nveh_gap_all=" << (all ? "true" : "false") << std::endl;
    return all;
}

}  // namespace repiu::tools
