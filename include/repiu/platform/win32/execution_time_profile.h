#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace repiu::platform::win32
{

// Guest-thread wall-clock attribution (Task 323).
//
// Task 322 measured only the inside of HandleSingleStepTrace and found it holds
// roughly 21.8% of wall clock, leaving about 78% never attributed. These buckets
// divide the guest thread's whole run so that residual can be named for the
// first time.
//
// Buckets are NOT mutually exclusive. A service bucket is reachable both from
// inside the VEH (the single-step HLE path) and from outside it (AOT fast-path
// thunks), so each bucket records a total plus the portion entered while the VEH
// was on the stack. Exclusivity is expressed only as a derived reporting value:
//
//   kVehExclusive = kVehTotal - (service cycles entered inside the VEH)
//   kUnaccounted  = kGuestRunTotal - kVehTotal - (service cycles outside it)
//
// kUnaccounted is the sum of guest execution inside the AOT code cache and
// kernel exception transition time, which cannot be observed from inside a
// handler. The calibration probe in repiu_aot_probe prices one transition so
// that residual can be split further, as an estimate.
//
// See docs/design/20260727-323-whole-run-execution-time-attribution.md.
enum class ExecutionTimeBucket : std::uint32_t
{
    kGuestRunTotal = 0,
    kVehTotal,
    kGlideGate,
    kPortIoDevice,
    kDosService,
    // Task 325: decomposition of kVehTotal, appended after the original five so
    // existing indices and log field order stay stable. These are parts OF
    // kVehTotal, not additions to it, so they never enter the kVehExclusive or
    // kUnaccounted formulas.
    // See docs/design/20260727-325-veh-boundary-path-attribution.md.
    kVehPrologue,
    kVehAotTransfer,
    kVehTelemetry,
    kVehBoundaryGates,
    kVehHleChain,
    // Task 326: two decompositions of kVehAotTransfer, measured together so one
    // run answers both which handler and what work.
    //
    // Handler axis -- mutually exclusive, since the six run sequentially.
    kAotWriteCompletion,
    kAotWriteFault,
    kAotReentry,
    kAotIndirect,
    kAotConditional,
    kAotReturn,
    // Function axis -- shared across handlers, instrumented at the definition
    // so every caller lands in one bucket. Deliberately NESTS inside the
    // handler axis, so the two axes are never summed together; each is a share
    // of kVehAotTransfer on its own.
    // See docs/design/20260727-326-aot-transfer-resolution-decomposition.md.
    kAotTransferResolve,
    kAotHleBoundaryScan,
    kAotDynamicTranslate,
    kAotResidency,
    // Task 334: decomposition of kAotReentry, which holds 97.48% of
    // kVehAotTransfer while the function axis above explains only 7.84% of it.
    // Mutually exclusive and NESTED inside kAotReentry, so they are a share of
    // that bucket alone and are never summed with either axis above.
    // See docs/design/20260728-334-aot-reentry-decomposition.md.
    kAotReentryGuestLookup,
    kAotReentryProvenance,
    kAotReentryRetired,
    kAotReentryBoundaryReason,
    kAotReentryNativeSpan,
    kAotReentrySingleStep,
    kCount,
};

constexpr std::uint32_t kFirstVehSubBucket =
    static_cast<std::uint32_t>(ExecutionTimeBucket::kVehPrologue);
constexpr std::uint32_t kFirstAotHandlerBucket =
    static_cast<std::uint32_t>(ExecutionTimeBucket::kAotWriteCompletion);
constexpr std::uint32_t kFirstAotFunctionBucket =
    static_cast<std::uint32_t>(ExecutionTimeBucket::kAotTransferResolve);
constexpr std::uint32_t kFirstAotReentryBucket =
    static_cast<std::uint32_t>(ExecutionTimeBucket::kAotReentryGuestLookup);

constexpr std::uint32_t kExecutionTimeBucketCount =
    static_cast<std::uint32_t>(ExecutionTimeBucket::kCount);

struct Win32ExecutionTimeProfile
{
    bool enabled = false;
    std::array<std::uint64_t, kExecutionTimeBucketCount> cycles = {};
    std::array<std::uint32_t, kExecutionTimeBucketCount> counts = {};
    std::array<std::uint64_t, kExecutionTimeBucketCount>
        inside_veh_cycles = {};
    std::array<std::uint32_t, kExecutionTimeBucketCount>
        inside_veh_counts = {};
    // Depth rather than a flag: the VEH can be re-entered while handling a
    // fault raised by a handler, and only the outermost frame may attribute.
    std::uint32_t veh_depth = 0;
    // The guest run normally ends by timeout, which tears the thread down
    // without unwinding, so the kGuestRunTotal scope destructor never runs and
    // the denominator would stay zero. Record the open interval's start so the
    // snapshot can close it at report time.
    std::uint64_t guest_run_start_cycles = 0;
    bool guest_run_open = false;
};

struct Win32ExecutionTimeProfileSnapshot
{
    bool enabled = false;
    std::array<std::uint64_t, kExecutionTimeBucketCount> cycles = {};
    std::array<std::uint32_t, kExecutionTimeBucketCount> counts = {};
    std::array<std::uint64_t, kExecutionTimeBucketCount>
        inside_veh_cycles = {};
    std::array<std::uint32_t, kExecutionTimeBucketCount>
        inside_veh_counts = {};
};

bool ResolveExecutionTimeProfileEnabled(std::string_view setting);
bool ExecutionTimeProfileEnabled();

void RecordExecutionTimeBucket(Win32ExecutionTimeProfile* profile,
                               ExecutionTimeBucket bucket,
                               std::uint64_t cycles,
                               bool inside_veh);

Win32ExecutionTimeProfileSnapshot SnapshotExecutionTimeProfile(
    const Win32ExecutionTimeProfile& profile);

class ExecutionTimeScope
{
public:
    ExecutionTimeScope(Win32ExecutionTimeProfile* profile,
                       ExecutionTimeBucket bucket,
                       std::uint64_t* completed_cycles = nullptr);
    ~ExecutionTimeScope();

    ExecutionTimeScope(const ExecutionTimeScope&) = delete;
    ExecutionTimeScope& operator=(const ExecutionTimeScope&) = delete;

private:
    Win32ExecutionTimeProfile* profile_ = nullptr;
    ExecutionTimeBucket bucket_ = ExecutionTimeBucket::kGuestRunTotal;
    std::uint64_t start_cycles_ = 0;
    std::uint64_t* completed_cycles_ = nullptr;
    bool inside_veh_ = false;
    bool owns_veh_depth_ = false;
};

}  // namespace repiu::platform::win32
