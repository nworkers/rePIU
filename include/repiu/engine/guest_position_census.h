#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace repiu::engine
{

// Task 411. Every existing execution sampler is tied to an exception: the
// single-step hotspot profile records only while HandleSingleStepTrace runs,
// and the native phase sampler waits for a full second with no exception
// dispatch, which a stalled pumpit3 run never provides. Code that waits inside
// the AOT cache without faulting is therefore invisible to all of them. This
// census samples the guest thread on a wall-clock interval instead, so the
// sampling instant is independent of what the guest is doing.
// See docs/design/20260804-411-stall-guest-position-census.md.

constexpr std::uint32_t kWin32GuestPositionCensusCapacity = 4096U;
constexpr std::uint32_t kWin32GuestPositionReportCapacity = 16U;
// Task 412: host samples carry a second axis -- which of our call sites led into
// the host or kernel code being sampled. Fewer sites than addresses, so a
// smaller table.
constexpr std::uint32_t kWin32GuestPositionHostSiteCapacity = 1024U;
constexpr std::uint32_t kWin32GuestPositionHostSiteReportCapacity = 12U;

// Where the sampled instruction pointer was executing. Cache samples carry the
// reverse-mapped guest address, so kArena and kCacheMapped aggregate on one
// address axis while still being told apart.
enum class GuestPositionOrigin : std::uint32_t
{
    kArena = 0,
    kCacheMapped,
    kCacheUnmapped,
    kHost,
    kCount,
};

constexpr std::uint32_t kGuestPositionOriginCount =
    static_cast<std::uint32_t>(GuestPositionOrigin::kCount);

struct Win32GuestPositionEntry
{
    std::uint32_t address = 0;
    std::uint32_t sample_count = 0;
    std::array<std::uint32_t, kGuestPositionOriginCount> origin_counts = {};
    bool occupied = false;
};

struct Win32GuestPositionHostSiteEntry
{
    std::uint32_t address = 0;
    std::uint32_t sample_count = 0;
    bool occupied = false;
};

struct Win32GuestPositionCensus
{
    bool enabled = false;
    std::uint32_t total_sample_count = 0;
    std::uint32_t distinct_address_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint32_t capture_failure_count = 0;
    std::uint32_t interval_milliseconds = 0;
    // Task 412. Latest GetThreadTimes reading for the guest thread, in 100 ns
    // units, beside the elapsed wall clock: the one measurement that separates
    // "busy in kernel exception dispatch" from "blocked on something".
    std::uint64_t thread_kernel_time_100ns = 0;
    std::uint64_t thread_user_time_100ns = 0;
    std::uint32_t thread_time_elapsed_milliseconds = 0;
    bool thread_time_valid = false;
    // Task 412 host call-site axis. sited + no_site + failed must equal the
    // host sample count; the design refuses to read the distribution otherwise.
    std::uint32_t host_scan_sample_count = 0;
    std::uint32_t host_scan_sited_count = 0;
    std::uint32_t host_scan_no_site_count = 0;
    std::uint32_t host_scan_failed_count = 0;
    std::uint32_t host_site_distinct_count = 0;
    std::uint32_t host_site_overflow_count = 0;
    std::array<Win32GuestPositionHostSiteEntry,
               kWin32GuestPositionHostSiteCapacity> host_sites = {};
    // Summed at the one recording site, so sum(origin_counts) ==
    // total_sample_count is a check rather than an assumption (design section 5).
    std::array<std::uint32_t, kGuestPositionOriginCount> origin_counts = {};
    std::array<Win32GuestPositionEntry, kWin32GuestPositionCensusCapacity>
        entries = {};
    // Task 401's lesson: teardown can hang after the guest thread stops, so the
    // dump is written as early as teardown allows and reported again later.
    bool dump_written = false;
    std::uint32_t dump_entry_count = 0;
};

struct Win32GuestPositionSample
{
    std::uint32_t address = 0;
    std::uint32_t sample_count = 0;
    std::array<std::uint32_t, kGuestPositionOriginCount> origin_counts = {};
};

// One reported host call site, resolved to a module and, when a PDB is
// available, a symbol. `module_name` and `symbol` are empty when resolution
// failed; `address` is always usable.
struct Win32GuestPositionHostSiteSample
{
    std::uint32_t address = 0;
    std::uint32_t sample_count = 0;
    std::uint32_t module_offset = 0;
    std::string module_name;
    std::string symbol;
};

struct Win32GuestPositionCensusSnapshot
{
    bool enabled = false;
    std::uint32_t total_sample_count = 0;
    std::uint32_t distinct_address_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint32_t capture_failure_count = 0;
    std::uint32_t interval_milliseconds = 0;
    std::uint64_t thread_kernel_time_100ns = 0;
    std::uint64_t thread_user_time_100ns = 0;
    std::uint32_t thread_time_elapsed_milliseconds = 0;
    bool thread_time_valid = false;
    std::uint32_t host_scan_sample_count = 0;
    std::uint32_t host_scan_sited_count = 0;
    std::uint32_t host_scan_no_site_count = 0;
    std::uint32_t host_scan_failed_count = 0;
    std::uint32_t host_site_distinct_count = 0;
    std::uint32_t host_site_overflow_count = 0;
    std::uint32_t host_site_top_count = 0;
    std::array<Win32GuestPositionHostSiteSample,
               kWin32GuestPositionHostSiteReportCapacity> host_site_top = {};
    std::array<std::uint32_t, kGuestPositionOriginCount> origin_counts = {};
    std::uint32_t top_count = 0;
    std::uint32_t top_coverage_count = 0;
    std::array<Win32GuestPositionSample, kWin32GuestPositionReportCapacity>
        top = {};
    // Module identity of the reported addresses, parallel to `top`. Resolved
    // once at teardown, so the sampling path pays nothing for it.
    std::array<std::string, kWin32GuestPositionReportCapacity>
        top_module_names = {};
    std::array<std::uint32_t, kWin32GuestPositionReportCapacity>
        top_module_offsets = {};
    bool dump_written = false;
    std::uint32_t dump_entry_count = 0;
    std::string dump_path;
};

// `REPIU_GUEST_POSITION_CENSUS`: "1"/"on"/"true" enables the census.
bool ResolveGuestPositionCensusEnabled(std::string_view setting);
bool GuestPositionCensusEnabled();

// `REPIU_GUEST_POSITION_CENSUS_MS`: sampling interval, clamped to 1-1000 ms.
constexpr std::uint32_t kWin32GuestPositionCensusDefaultIntervalMs = 10U;
std::uint32_t ResolveGuestPositionCensusIntervalMilliseconds(
    std::string_view setting);
std::uint32_t GuestPositionCensusIntervalMilliseconds();

// Folds one captured instruction pointer onto the reported address axis.
// `mapped`/`guest_eip` come from CaptureWin32NativePhaseSample, which
// reverse-maps only cache-range addresses; a cache address it could not map
// stays raw and is reported as kCacheUnmapped.
struct Win32GuestPositionClassification
{
    std::uint32_t address = 0;
    GuestPositionOrigin origin = GuestPositionOrigin::kHost;
};

Win32GuestPositionClassification ClassifyGuestPosition(
    std::uint32_t eip,
    bool mapped,
    std::uint32_t guest_eip,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t cache_base,
    std::uint32_t cache_size);

void RecordGuestPosition(Win32GuestPositionCensus* census,
                         const Win32GuestPositionClassification& sample);

void RecordGuestPositionCaptureFailure(Win32GuestPositionCensus* census);

// Task 412. Records the outcome of one host-sample stack scan. `site` is the
// first return address found inside the loader module, or zero when the scan
// found none; `scan_failed` marks a read that faulted. Exactly one of the three
// outcomes is counted per host sample, which is what makes the reconciliation
// in the report meaningful.
void RecordGuestPositionHostSite(Win32GuestPositionCensus* census,
                                 std::uint32_t site,
                                 bool scan_failed);

void RecordGuestPositionThreadTime(Win32GuestPositionCensus* census,
                                   std::uint64_t kernel_time_100ns,
                                   std::uint64_t user_time_100ns,
                                   std::uint32_t elapsed_milliseconds);

// Fills the module names, offsets, and symbols on a snapshot. Runs on the
// teardown path only, after the guest thread has stopped, and degrades to
// leaving the strings empty when a module or symbol cannot be resolved.
void ResolveGuestPositionCensusSymbols(
    Win32GuestPositionCensusSnapshot* snapshot);

Win32GuestPositionCensusSnapshot SnapshotGuestPositionCensus(
    const Win32GuestPositionCensus& census);

// `REPIU_GUEST_POSITION_CENSUS_DUMP`: unset or empty disables the dump, "1"
// selects build/guest_position_census.txt, anything else is used as the path.
std::filesystem::path ResolveGuestPositionCensusDumpPath(
    std::string_view setting);

std::filesystem::path GuestPositionCensusDumpPath();

// Writes every occupied entry ordered by sample count, so an outer loop that
// samples two orders of magnitude below the hot one is still readable.
bool WriteGuestPositionCensusDump(const std::filesystem::path& path,
                                  Win32GuestPositionCensus* census,
                                  std::uint32_t* written_entry_count);

// Resolves the configured path and writes once. Safe to call from several
// teardown points; only the first call touches the file.
bool WriteGuestPositionCensusDumpIfEnabled(Win32GuestPositionCensus* census,
                                           std::uint32_t* written_entry_count,
                                           std::string* resolved_path);

}  // namespace repiu::engine
