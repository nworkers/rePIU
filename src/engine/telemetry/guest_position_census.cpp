#include "repiu/engine/guest_position_census.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#endif

namespace repiu::engine
{
namespace
{

std::uint32_t HashCensusAddress(std::uint32_t address)
{
    static_assert((kWin32GuestPositionCensusCapacity &
                   (kWin32GuestPositionCensusCapacity - 1U)) == 0U);
    return (address * 2654435761U) &
        (kWin32GuestPositionCensusCapacity - 1U);
}

Win32GuestPositionSample MakeSample(const Win32GuestPositionEntry& entry)
{
    Win32GuestPositionSample sample;
    sample.address = entry.address;
    sample.sample_count = entry.sample_count;
    sample.origin_counts = entry.origin_counts;
    return sample;
}

std::vector<Win32GuestPositionSample> CollectSortedSamples(
    const Win32GuestPositionCensus& census)
{
    std::vector<Win32GuestPositionSample> samples;
    samples.reserve(census.distinct_address_count);
    for (const Win32GuestPositionEntry& entry : census.entries)
    {
        if (entry.occupied)
        {
            samples.push_back(MakeSample(entry));
        }
    }
    std::sort(samples.begin(), samples.end(),
              [](const auto& left, const auto& right) {
                  if (left.sample_count != right.sample_count)
                  {
                      return left.sample_count > right.sample_count;
                  }
                  return left.address < right.address;
              });
    return samples;
}

bool InRange(std::uint32_t address, std::uint32_t base, std::uint32_t size)
{
    if (size == 0U)
    {
        return false;
    }
    const std::uint64_t end =
        static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(size);
    return address >= base && static_cast<std::uint64_t>(address) < end;
}

}  // namespace

bool ResolveGuestPositionCensusEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GuestPositionCensusEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REPIU_GUEST_POSITION_CENSUS");
        return value != nullptr &&
            ResolveGuestPositionCensusEnabled(std::string_view(value));
    }();
    return enabled;
}

std::uint32_t ResolveGuestPositionCensusIntervalMilliseconds(
    std::string_view setting)
{
    if (setting.empty())
    {
        return kWin32GuestPositionCensusDefaultIntervalMs;
    }
    std::uint32_t parsed = 0;
    const char* first = setting.data();
    const char* last = first + setting.size();
    const std::from_chars_result result =
        std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last || parsed == 0U)
    {
        return kWin32GuestPositionCensusDefaultIntervalMs;
    }
    return std::min<std::uint32_t>(parsed, 1000U);
}

std::uint32_t GuestPositionCensusIntervalMilliseconds()
{
    static const std::uint32_t interval = [] {
        const char* value = std::getenv("REPIU_GUEST_POSITION_CENSUS_MS");
        return ResolveGuestPositionCensusIntervalMilliseconds(
            value == nullptr ? std::string_view{} : std::string_view(value));
    }();
    return interval;
}

Win32GuestPositionClassification ClassifyGuestPosition(
    std::uint32_t eip,
    bool mapped,
    std::uint32_t guest_eip,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t cache_base,
    std::uint32_t cache_size)
{
    Win32GuestPositionClassification classification;
    if (mapped)
    {
        classification.address = guest_eip;
        classification.origin = GuestPositionOrigin::kCacheMapped;
        return classification;
    }
    classification.address = eip;
    if (InRange(eip, cache_base, cache_size))
    {
        classification.origin = GuestPositionOrigin::kCacheUnmapped;
        return classification;
    }
    if (InRange(eip, runtime_base, runtime_size))
    {
        classification.origin = GuestPositionOrigin::kArena;
        return classification;
    }
    classification.origin = GuestPositionOrigin::kHost;
    return classification;
}

void RecordGuestPosition(Win32GuestPositionCensus* census,
                         const Win32GuestPositionClassification& sample)
{
    if (census == nullptr)
    {
        return;
    }
    census->enabled = true;
    ++census->total_sample_count;

    std::uint32_t origin_index = static_cast<std::uint32_t>(sample.origin);
    if (origin_index >= kGuestPositionOriginCount)
    {
        origin_index = static_cast<std::uint32_t>(GuestPositionOrigin::kHost);
    }
    ++census->origin_counts[origin_index];

    const std::uint32_t first = HashCensusAddress(sample.address);
    for (std::uint32_t probe = 0;
         probe < kWin32GuestPositionCensusCapacity; ++probe)
    {
        Win32GuestPositionEntry& entry =
            census->entries[(first + probe) &
                            (kWin32GuestPositionCensusCapacity - 1U)];
        if (!entry.occupied)
        {
            entry.occupied = true;
            entry.address = sample.address;
            ++census->distinct_address_count;
        }
        if (entry.address != sample.address)
        {
            continue;
        }
        ++entry.sample_count;
        ++entry.origin_counts[origin_index];
        return;
    }
    ++census->overflow_count;
}

void RecordGuestPositionCaptureFailure(Win32GuestPositionCensus* census)
{
    if (census == nullptr)
    {
        return;
    }
    census->enabled = true;
    ++census->capture_failure_count;
}

void RecordGuestPositionHostSite(Win32GuestPositionCensus* census,
                                 std::uint32_t site,
                                 bool scan_failed)
{
    if (census == nullptr)
    {
        return;
    }
    ++census->host_scan_sample_count;
    if (scan_failed)
    {
        ++census->host_scan_failed_count;
        return;
    }
    if (site == 0U)
    {
        ++census->host_scan_no_site_count;
        return;
    }
    ++census->host_scan_sited_count;

    const std::uint32_t first =
        (site * 2654435761U) & (kWin32GuestPositionHostSiteCapacity - 1U);
    for (std::uint32_t probe = 0;
         probe < kWin32GuestPositionHostSiteCapacity; ++probe)
    {
        Win32GuestPositionHostSiteEntry& entry =
            census->host_sites[(first + probe) &
                               (kWin32GuestPositionHostSiteCapacity - 1U)];
        if (!entry.occupied)
        {
            entry.occupied = true;
            entry.address = site;
            ++census->host_site_distinct_count;
        }
        if (entry.address != site)
        {
            continue;
        }
        ++entry.sample_count;
        return;
    }
    ++census->host_site_overflow_count;
}

void RecordGuestPositionThreadTime(Win32GuestPositionCensus* census,
                                   std::uint64_t kernel_time_100ns,
                                   std::uint64_t user_time_100ns,
                                   std::uint32_t elapsed_milliseconds)
{
    if (census == nullptr)
    {
        return;
    }
    census->thread_kernel_time_100ns = kernel_time_100ns;
    census->thread_user_time_100ns = user_time_100ns;
    census->thread_time_elapsed_milliseconds = elapsed_milliseconds;
    census->thread_time_valid = true;
}

Win32GuestPositionCensusSnapshot SnapshotGuestPositionCensus(
    const Win32GuestPositionCensus& census)
{
    Win32GuestPositionCensusSnapshot snapshot;
    snapshot.enabled = census.enabled;
    snapshot.total_sample_count = census.total_sample_count;
    snapshot.distinct_address_count = census.distinct_address_count;
    snapshot.overflow_count = census.overflow_count;
    snapshot.capture_failure_count = census.capture_failure_count;
    snapshot.interval_milliseconds = census.interval_milliseconds;
    snapshot.origin_counts = census.origin_counts;
    snapshot.dump_written = census.dump_written;
    snapshot.dump_entry_count = census.dump_entry_count;
    snapshot.thread_kernel_time_100ns = census.thread_kernel_time_100ns;
    snapshot.thread_user_time_100ns = census.thread_user_time_100ns;
    snapshot.thread_time_elapsed_milliseconds =
        census.thread_time_elapsed_milliseconds;
    snapshot.thread_time_valid = census.thread_time_valid;
    snapshot.host_scan_sample_count = census.host_scan_sample_count;
    snapshot.host_scan_sited_count = census.host_scan_sited_count;
    snapshot.host_scan_no_site_count = census.host_scan_no_site_count;
    snapshot.host_scan_failed_count = census.host_scan_failed_count;
    snapshot.host_site_distinct_count = census.host_site_distinct_count;
    snapshot.host_site_overflow_count = census.host_site_overflow_count;

    const std::vector<Win32GuestPositionSample> samples =
        CollectSortedSamples(census);
    snapshot.top_count = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(samples.size()),
        kWin32GuestPositionReportCapacity);
    for (std::uint32_t index = 0; index < snapshot.top_count; ++index)
    {
        snapshot.top[index] = samples[index];
        snapshot.top_coverage_count += samples[index].sample_count;
    }

    std::vector<Win32GuestPositionHostSiteEntry> sites;
    sites.reserve(census.host_site_distinct_count);
    for (const Win32GuestPositionHostSiteEntry& entry : census.host_sites)
    {
        if (entry.occupied)
        {
            sites.push_back(entry);
        }
    }
    std::sort(sites.begin(), sites.end(),
              [](const auto& left, const auto& right) {
                  if (left.sample_count != right.sample_count)
                  {
                      return left.sample_count > right.sample_count;
                  }
                  return left.address < right.address;
              });
    snapshot.host_site_top_count = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(sites.size()),
        kWin32GuestPositionHostSiteReportCapacity);
    for (std::uint32_t index = 0; index < snapshot.host_site_top_count;
         ++index)
    {
        snapshot.host_site_top[index].address = sites[index].address;
        snapshot.host_site_top[index].sample_count = sites[index].sample_count;
    }
    return snapshot;
}

#if defined(_WIN32)
namespace
{

// Resolves one address to its module file name and offset. Returns false when
// the address belongs to no loaded module, which is normal for guest addresses.
bool ResolveModule(std::uint32_t address,
                   std::string* module_name,
                   std::uint32_t* module_offset)
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(static_cast<std::uintptr_t>(address)),
            &module) ||
        module == nullptr)
    {
        return false;
    }
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
    if (length == 0U)
    {
        return false;
    }
    const char* leaf = std::strrchr(path, '\\');
    *module_name = leaf != nullptr ? leaf + 1 : path;
    *module_offset = address -
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    return true;
}

// Symbol resolution is best-effort: without a PDB beside the binary this
// answers nothing and the caller falls back to module+offset.
class SymbolSession
{
public:
    SymbolSession()
    {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        initialised_ = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != 0;
    }

    ~SymbolSession()
    {
        if (initialised_)
        {
            SymCleanup(GetCurrentProcess());
        }
    }

    SymbolSession(const SymbolSession&) = delete;
    SymbolSession& operator=(const SymbolSession&) = delete;

    bool Resolve(std::uint32_t address, std::string* name) const
    {
        if (!initialised_)
        {
            return false;
        }
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;
        DWORD64 displacement = 0;
        if (!SymFromAddr(GetCurrentProcess(),
                         static_cast<DWORD64>(address),
                         &displacement,
                         symbol))
        {
            return false;
        }
        *name = symbol->Name;
        if (displacement != 0)
        {
            *name += "+0x";
            char digits[32] = {};
            std::snprintf(digits, sizeof(digits), "%llX",
                          static_cast<unsigned long long>(displacement));
            *name += digits;
        }
        return true;
    }

private:
    bool initialised_ = false;
};

}  // namespace

void ResolveGuestPositionCensusSymbols(
    Win32GuestPositionCensusSnapshot* snapshot)
{
    if (snapshot == nullptr || !snapshot->enabled)
    {
        return;
    }
    const SymbolSession symbols;
    for (std::uint32_t index = 0; index < snapshot->top_count; ++index)
    {
        std::string name;
        std::uint32_t offset = 0;
        if (ResolveModule(snapshot->top[index].address, &name, &offset))
        {
            snapshot->top_module_names[index] = name;
            snapshot->top_module_offsets[index] = offset;
        }
    }
    for (std::uint32_t index = 0; index < snapshot->host_site_top_count;
         ++index)
    {
        Win32GuestPositionHostSiteSample& site =
            snapshot->host_site_top[index];
        std::string name;
        std::uint32_t offset = 0;
        if (ResolveModule(site.address, &name, &offset))
        {
            site.module_name = name;
            site.module_offset = offset;
        }
        std::string symbol;
        if (symbols.Resolve(site.address, &symbol))
        {
            site.symbol = symbol;
        }
    }
}
#else
void ResolveGuestPositionCensusSymbols(
    Win32GuestPositionCensusSnapshot* snapshot)
{
    (void)snapshot;
}
#endif

std::filesystem::path ResolveGuestPositionCensusDumpPath(
    std::string_view setting)
{
    if (setting.empty() || setting == "0" || setting == "off" ||
        setting == "false")
    {
        return {};
    }
    if (setting == "1" || setting == "on" || setting == "true")
    {
        return std::filesystem::path("build") / "guest_position_census.txt";
    }
    return std::filesystem::path(std::string(setting));
}

std::filesystem::path GuestPositionCensusDumpPath()
{
    static const std::filesystem::path path = [] {
        const char* value = std::getenv("REPIU_GUEST_POSITION_CENSUS_DUMP");
        return ResolveGuestPositionCensusDumpPath(
            value == nullptr ? std::string_view{} : std::string_view(value));
    }();
    return path;
}

bool WriteGuestPositionCensusDump(const std::filesystem::path& path,
                                  Win32GuestPositionCensus* census,
                                  std::uint32_t* written_entry_count)
{
    if (written_entry_count != nullptr)
    {
        *written_entry_count = 0;
    }
    if (path.empty() || census == nullptr)
    {
        return false;
    }
    if (census->dump_written)
    {
        if (written_entry_count != nullptr)
        {
            *written_entry_count = census->dump_entry_count;
        }
        return true;
    }

    const std::vector<Win32GuestPositionSample> samples =
        CollectSortedSamples(*census);

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        return false;
    }

    file << "# rePIU guest position census dump\n"
         << "# total_samples=" << census->total_sample_count
         << " distinct=" << census->distinct_address_count
         << " overflow=" << census->overflow_count
         << " capture_failures=" << census->capture_failure_count
         << " interval_ms=" << census->interval_milliseconds << "\n"
         << "# address sample_count arena cache_mapped cache_unmapped host\n";
    for (const Win32GuestPositionSample& sample : samples)
    {
        file << "0x" << std::uppercase << std::hex << std::setw(8)
             << std::setfill('0') << sample.address << std::nouppercase
             << std::dec << std::setfill(' ') << ' ' << sample.sample_count;
        for (std::uint32_t index = 0; index < kGuestPositionOriginCount;
             ++index)
        {
            file << ' ' << sample.origin_counts[index];
        }
        file << '\n';
    }
    if (!file)
    {
        return false;
    }

    census->dump_written = true;
    census->dump_entry_count = static_cast<std::uint32_t>(samples.size());
    if (written_entry_count != nullptr)
    {
        *written_entry_count = census->dump_entry_count;
    }
    return true;
}

bool WriteGuestPositionCensusDumpIfEnabled(Win32GuestPositionCensus* census,
                                           std::uint32_t* written_entry_count,
                                           std::string* resolved_path)
{
    const std::filesystem::path path = GuestPositionCensusDumpPath();
    if (resolved_path != nullptr)
    {
        *resolved_path = path.string();
    }
    return WriteGuestPositionCensusDump(path, census, written_entry_count);
}

}  // namespace repiu::engine
