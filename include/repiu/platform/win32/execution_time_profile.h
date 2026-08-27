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

// Task 372: how the exit-to-next-entry gap is attributed. Deliberately coarse --
// the question is whether the kernel round trip is a large share of wall, and
// only the single-step class answers it cleanly.
enum class VehGapClass : std::uint32_t
{
    kSingleStep = 0,
    kBreakpoint,
    kOther,
    kCount,
};

constexpr std::uint32_t kVehGapClassCount =
    static_cast<std::uint32_t>(VehGapClass::kCount);

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
    // Task 368 stage one: the interval from VEH entry to the Glide gate scope
    // opening. That is exactly what exception-free gate dispatch would remove --
    // kernel transition, VEH prologue, and transfer resolution for this one
    // population -- as opposed to the gate body, which the work would still do.
    //
    // Both timestamps are already taken by the two scopes, so this adds no clock
    // read, per the Task 353 rule.
    std::uint64_t veh_entry_cycles = 0;
    std::uint64_t glide_gate_prologue_cycles = 0;
    std::uint32_t glide_gate_prologue_count = 0;
    std::uint32_t glide_gate_prologue_clamped_count = 0;
    // Task 372: the interval this instrument has always been blind to. kVehTotal
    // times handler entry to exit, so the kernel's own delivery path -- the trap,
    // the walk to KiUserExceptionDispatcher, and the RtlRestoreContext return --
    // falls outside every bucket and lands in `unaccounted` alongside real guest
    // execution. Measuring exit-to-next-entry recovers it.
    //
    // The single-step class is the one that reads as a pure round trip: between
    // two consecutive single-step exceptions the guest executes exactly one
    // instruction, so almost nothing but the kernel fits in that gap. Breakpoint
    // and other gaps include real guest work and are kept separate rather than
    // averaged in.
    //
    // Both timestamps already exist, so like the Task 368 prologue above this
    // adds no clock read.
    std::uint64_t veh_last_exit_cycles = 0;
    std::uint64_t veh_gap_pending_cycles = 0;
    std::array<std::uint64_t, kVehGapClassCount> veh_gap_cycles = {};
    std::array<std::uint32_t, kVehGapClassCount> veh_gap_counts = {};
    // Across a million-plus samples nothing can make the round trip cheaper than
    // the smallest gap observed, so this floor stays meaningful even where the
    // mean is contaminated by guest execution.
    std::uint64_t veh_gap_min_cycles = 0;
    std::uint64_t veh_gap_max_cycles = 0;
    // Banked but never classified, because the handler returned before reaching
    // the census. Kept visible rather than dropped so the classes always sum.
    std::uint64_t veh_gap_unclassified_cycles = 0;
    std::uint32_t veh_gap_clamped_count = 0;
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
    // Task 368 stage one. See the profile struct for what this interval is.
    std::uint64_t glide_gate_prologue_cycles = 0;
    std::uint32_t glide_gate_prologue_count = 0;
    std::uint32_t glide_gate_prologue_clamped_count = 0;
    // Task 372. See the profile struct for why the single-step class is the one
    // that reads as a kernel round trip.
    std::array<std::uint64_t, kVehGapClassCount> veh_gap_cycles = {};
    std::array<std::uint32_t, kVehGapClassCount> veh_gap_counts = {};
    std::uint64_t veh_gap_min_cycles = 0;
    std::uint64_t veh_gap_max_cycles = 0;
    std::uint64_t veh_gap_unclassified_cycles = 0;
    std::uint32_t veh_gap_clamped_count = 0;
};

bool ResolveExecutionTimeProfileEnabled(std::string_view setting);
bool ExecutionTimeProfileEnabled();

void RecordExecutionTimeBucket(Win32ExecutionTimeProfile* profile,
                               ExecutionTimeBucket bucket,
                               std::uint64_t cycles,
                               bool inside_veh);

// Task 372: moves the pending gap the VEH scope banked into the class the
// exception turned out to be. Called from the census, which runs after the
// exception record has been validated -- classifying inside the scope
// constructor would have to read a record Task 296 showed can be malformed.
void RecordVehExceptionGap(Win32ExecutionTimeProfile* profile,
                           VehGapClass gap_class);

Win32ExecutionTimeProfileSnapshot SnapshotExecutionTimeProfile(
    const Win32ExecutionTimeProfile& profile);

// Task 511: the five numbers a report actually quotes, derived once.
//
// The two interesting ones are not buckets. `veh_exclusive` and `unaccounted`
// come out of the formulas at the top of this header, and until now those
// formulas lived inline in the loader's summary. A second reader needed them --
// the live report, which prints while the run is still going, because a Linux
// run that reaches rendering never stops its guest thread and so never reaches
// that summary at all. Two copies of a formula is how two reports come to
// disagree, so there is one.
struct Win32ExecutionTimeShares
{
    std::uint64_t total = 0;
    std::uint64_t veh = 0;
    std::uint64_t veh_exclusive = 0;
    std::uint64_t glide_gate = 0;
    std::uint64_t port_io = 0;
    std::uint64_t dos_service = 0;
    // Guest execution inside the AOT code cache plus kernel transition time --
    // what no handler can observe from the inside.
    std::uint64_t unaccounted = 0;
};

[[nodiscard]] Win32ExecutionTimeShares ComputeExecutionTimeShares(
    const Win32ExecutionTimeProfileSnapshot& snapshot);

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
