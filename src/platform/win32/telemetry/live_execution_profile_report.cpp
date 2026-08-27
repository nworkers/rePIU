#include "repiu/platform/win32/live_execution_profile_report.h"

#include "repiu/platform/host_error_stream.h"
#include "repiu/platform/host_time.h"

#include <cstdio>
#include <cstdlib>

namespace repiu::platform::win32
{
namespace
{

// Resolved once. This is read on the frame path, and `getenv` is not free.
std::uint32_t ResolveIntervalMilliseconds()
{
    const char* value = std::getenv("REPIU_LIVE_PROFILE_INTERVAL_MS");
    if (value == nullptr)
    {
        return 0U;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || parsed == 0UL)
    {
        return 0U;
    }
    // A report a frame would be its own workload; a report an hour is not a
    // series. Both ends are clamped rather than refused, so a mistyped value
    // still produces something readable.
    constexpr unsigned long kMinimum = 100UL;
    constexpr unsigned long kMaximum = 600000UL;
    if (parsed < kMinimum)
    {
        return static_cast<std::uint32_t>(kMinimum);
    }
    if (parsed > kMaximum)
    {
        return static_cast<std::uint32_t>(kMaximum);
    }
    return static_cast<std::uint32_t>(parsed);
}

std::uint32_t IntervalMilliseconds()
{
    static const std::uint32_t interval = ResolveIntervalMilliseconds();
    return interval;
}

// Guest-thread state, and only ever touched from there. See the header for why
// that is what makes the whole thing lock-free.
bool g_started = false;
std::uint32_t g_last_report_ticks = 0;
std::uint32_t g_report_index = 0;
std::uint64_t g_last_total = 0;
std::uint64_t g_last_frames = 0;

double Share(std::uint64_t part, std::uint64_t whole)
{
    if (whole == 0U)
    {
        return 0.0;
    }
    return 100.0 * static_cast<double>(part) / static_cast<double>(whole);
}

}  // namespace

bool LiveExecutionProfileReportEnabled()
{
    return IntervalMilliseconds() != 0U;
}

void ReportLiveExecutionProfileIfDue(
    const Win32ExecutionTimeProfile* profile,
    const std::uint64_t frames)
{
    const std::uint32_t interval = IntervalMilliseconds();
    if (interval == 0U || profile == nullptr || !profile->enabled)
    {
        return;
    }

    const std::uint32_t now = repiu::platform::MillisecondTicks();
    if (!g_started)
    {
        // The first call only starts the clock. A report taken here would cover
        // whatever the run happened to do before the first frame, which is a
        // different question from the one this instrument is for.
        g_started = true;
        g_last_report_ticks = now;
        return;
    }
    // Unsigned subtraction, which is right across the 49-day wrap the tick
    // counter documents.
    if (static_cast<std::uint32_t>(now - g_last_report_ticks) < interval)
    {
        return;
    }

    const Win32ExecutionTimeProfileSnapshot snapshot =
        SnapshotExecutionTimeProfile(*profile);
    const Win32ExecutionTimeShares shares =
        ComputeExecutionTimeShares(snapshot);

    // Reported as a window rather than only as a running total. A total over a
    // whole run cannot tell a steady cost from one that arrives late, and this
    // port has twice been misled by a period longer than the observation window.
    const std::uint64_t window_total =
        shares.total > g_last_total ? shares.total - g_last_total : 0U;
    const std::uint64_t window_frames =
        frames > g_last_frames ? frames - g_last_frames : 0U;

    char line[320] = {};
    const int length = std::snprintf(
        line, sizeof(line),
        "[repiu-live-profile] #%u frames=%llu window_frames=%llu "
        "guest_run=%llu window_cycles=%llu cycles_per_frame=%llu "
        "veh=%.2f%% veh_excl=%.2f%% glide=%.2f%% port_io=%.2f%% dos=%.2f%% "
        "unaccounted=%.2f%%\n",
        static_cast<unsigned>(++g_report_index),
        static_cast<unsigned long long>(frames),
        static_cast<unsigned long long>(window_frames),
        static_cast<unsigned long long>(shares.total),
        static_cast<unsigned long long>(window_total),
        static_cast<unsigned long long>(
            window_frames == 0U ? 0U : window_total / window_frames),
        Share(shares.veh, shares.total),
        Share(shares.veh_exclusive, shares.total),
        Share(shares.glide_gate, shares.total),
        Share(shares.port_io, shares.total),
        Share(shares.dos_service, shares.total),
        Share(shares.unaccounted, shares.total));
    if (length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            line,
            static_cast<std::size_t>(length) < sizeof(line)
                ? static_cast<std::size_t>(length)
                : sizeof(line) - 1U);
    }

    // Task 512: a second line, because the first answers "where does the time
    // go" and this answers "why is that bucket large" -- and one line carrying
    // both reads worse for a person and for a script.
    //
    // A handler bucket is a product: deliveries a frame times cycles a
    // delivery. Which of the two carries a cross-host factor decides the fix
    // completely, so both are printed rather than the product alone.
    //
    // `gap` is Task 372's interval from handler exit to the next entry -- the
    // kernel's delivery path with no handler body in it. The single-step class
    // is the purest reading, since the guest executes exactly one instruction
    // between two of them, and the minimum is a floor no sample goes below.
    const std::uint32_t veh_count =
        snapshot.counts[static_cast<std::uint32_t>(
            ExecutionTimeBucket::kVehTotal)];
    const std::uint32_t gap_single_step_count =
        snapshot.veh_gap_counts[static_cast<std::uint32_t>(
            VehGapClass::kSingleStep)];
    const std::uint64_t gap_single_step_cycles =
        snapshot.veh_gap_cycles[static_cast<std::uint32_t>(
            VehGapClass::kSingleStep)];
    char veh_line[320] = {};
    const int veh_length = std::snprintf(
        veh_line, sizeof(veh_line),
        "[repiu-live-veh] #%u veh_count=%llu per_frame=%llu "
        "cycles_per_veh=%llu gap_min=%llu gap_ss_mean=%llu gap_ss_count=%llu\n",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned long long>(veh_count),
        static_cast<unsigned long long>(frames == 0U ? 0U : veh_count / frames),
        static_cast<unsigned long long>(
            veh_count == 0U ? 0U : shares.veh / veh_count),
        static_cast<unsigned long long>(snapshot.veh_gap_min_cycles),
        static_cast<unsigned long long>(
            gap_single_step_count == 0U
                ? 0U
                : gap_single_step_cycles / gap_single_step_count),
        static_cast<unsigned long long>(gap_single_step_count));
    if (veh_length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            veh_line,
            static_cast<std::size_t>(veh_length) < sizeof(veh_line)
                ? static_cast<std::size_t>(veh_length)
                : sizeof(veh_line) - 1U);
    }

    g_last_report_ticks = now;
    g_last_total = shares.total;
    g_last_frames = frames;
}

}  // namespace repiu::platform::win32
