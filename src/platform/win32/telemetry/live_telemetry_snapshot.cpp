#include "live_telemetry_snapshot.h"
#include "win32_thread_api.h"
#include "boundary/timer_interrupt_boundary.h"
#include "execution/execution_internal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>

#include <psapi.h>

namespace repiu::platform::win32
{
namespace
{

// Task 333. The command-aware wait replaces an unconditional Sleep(1) on the
// host poll loop, and this switch exists so the two can be compared in one
// binary: `REPIU_GLIDE_HOST_WAIT=0` restores the sleep. Default on.
bool GlideHostCommandWaitEnabled()
{
    static const bool enabled = []() {
        const char* value = std::getenv("REPIU_GLIDE_HOST_WAIT");
        return value == nullptr || std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

void SaturatingAtomicAdd(std::atomic<std::uint32_t>* value,
                         std::uint64_t increment)
{
    std::uint32_t current = value->load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::uint32_t>::max())
    {
        const std::uint64_t room =
            std::numeric_limits<std::uint32_t>::max() - current;
        const std::uint32_t added = static_cast<std::uint32_t>(
            std::min(increment, room));
        const std::uint32_t next = current + added;
        if (value->compare_exchange_weak(
                current, next,
                std::memory_order_release,
                std::memory_order_relaxed))
        {
            return;
        }
    }
}

}  // namespace

SharedTelemetryMapping OpenSharedTelemetryMapping()
{
    SharedTelemetryMapping result;
    char mapping_name[256] = {};
    if (GetEnvironmentVariableA(kWin32LiveTelemetryEnvironment,
                                mapping_name,
                                sizeof(mapping_name)) == 0)
    {
        return result;
    }
    result.mapping = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        mapping_name);
    if (result.mapping == nullptr)
    {
        return result;
    }
    result.telemetry = static_cast<Win32SharedLiveTelemetry*>(
        MapViewOfFile(result.mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (result.telemetry == nullptr ||
        result.telemetry->magic != kWin32LiveTelemetryMagic ||
        result.telemetry->version != kWin32LiveTelemetryVersion)
    {
        if (result.telemetry != nullptr)
        {
            UnmapViewOfFile(result.telemetry);
            result.telemetry = nullptr;
        }
        CloseHandle(result.mapping);
        result.mapping = nullptr;
    }
    return result;
}

void WriteLiveTelemetrySnapshot(const ThreadContext& context,
                                DWORD elapsed_milliseconds,
                                DWORD poll_iteration)
{
    char buffer[768] = {};
    const int length = std::snprintf(
        buffer,
        sizeof(buffer),
        "[repiu-live] elapsed_ms=%lu poll=%lu phase=%u heartbeat=%u "
        "dispatch_entry=%u dispatch_exit=%u last_eip=0x%08X "
        "progress=%u single_step=%u aot=%u/%u fast=%u/%u/%u "
        "routea=%u/%u region=%u/%u/%u/%u/%u "
        "span=%u/%u/%u/%u/%u span_cache=%u/%u "
        "span_reject_cache=%u/%u/%u/%u/%u span_write=%u/%u/%u "
        "span_cancel_last=0x%08X/0x%08X "
        "span_jump=%u/%u "
        "retired_span=%u/%u "
        "selguard=%u/%u/%u/%u/%u "
        "posthle=%u/%u "
        "prov=%u/%u/%u/%u/%u/%u/%u/%u "
        "reject=0x%08X:0x%08X/0x%02X bytes=%08X%08X\r\n",
        static_cast<unsigned long>(elapsed_milliseconds),
        static_cast<unsigned long>(poll_iteration),
        context.live_telemetry_phase.load(std::memory_order_relaxed),
        context.live_telemetry_heartbeat.load(std::memory_order_relaxed),
        context.exception_dispatch_entry_count.load(
            std::memory_order_relaxed),
        context.exception_dispatch_exit_count.load(
            std::memory_order_relaxed),
        context.exception_dispatch_last_eip.load(std::memory_order_relaxed),
        context.diagnostic_progress_count.load(std::memory_order_relaxed),
        context.single_step_trace_count.load(std::memory_order_relaxed),
        context.aot_boundary_count.load(std::memory_order_relaxed),
        context.aot_reentry_count.load(std::memory_order_relaxed),
        context.native_fast_path.entry_count.load(std::memory_order_relaxed),
        context.native_fast_path.return_count.load(std::memory_order_relaxed),
        context.native_fast_path.cancel_count.load(std::memory_order_relaxed),
        context.routea_sensitive_count.load(std::memory_order_relaxed),
        context.routea_segment_sensitive_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.region_entry_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.region_sensitive_hit_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.region_return_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.region_reject_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.region_stray_heal_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_entry_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_boundary_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_cancel_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_instruction_total.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_reject_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_cache_hit_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_cache_miss_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_reject_cache_hit_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_reject_cache_miss_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_reject_cache_stale_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_reject_cache_store_count.load(
            std::memory_order_relaxed),
        context.native_fast_path
            .linear_span_reject_cache_capacity_skip_count.load(
                std::memory_order_relaxed),
        context.native_fast_path.linear_span_write_cross_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_write_guard_uncovered_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_write_fault_cancel_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_last_cancel_code.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_last_cancel_eip.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_direct_jump_chain_count.load(
            std::memory_order_relaxed),
        context.native_fast_path.linear_span_backward_jump_stop_count.load(
            std::memory_order_relaxed),
        context.aot_retired_span_attempt_count.load(
            std::memory_order_relaxed),
        context.aot_retired_span_success_count.load(
            std::memory_order_relaxed),
        context.aot_selector_guard_native_site_count.load(
            std::memory_order_relaxed),
        context.aot_selector_guard_hle_site_count.load(
            std::memory_order_relaxed),
        context.aot_selector_guard_unresolved_site_count.load(
            std::memory_order_relaxed),
        context.aot_selector_guard_hle_exit_count.load(
            std::memory_order_relaxed),
        context.aot_selector_guard_mismatch_count.load(
            std::memory_order_relaxed),
        context.aot_dbt_hle_translation_attempt_count.load(
            std::memory_order_relaxed),
        context.aot_dbt_hle_translation_success_count.load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[0].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[1].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[2].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[3].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[4].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[5].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[6].load(
            std::memory_order_relaxed),
        context.aot_breakpoint_provenance_counts[7].load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_candidate.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_instruction.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_opcode.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_bytes_high.load(
            std::memory_order_relaxed),
        context.native_fast_path.last_rejected_bytes_low.load(
            std::memory_order_relaxed));
    if (length <= 0)
    {
        return;
    }

    HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle == nullptr || stderr_handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    const DWORD byte_count = static_cast<DWORD>(
        length < static_cast<int>(sizeof(buffer)) ? length
                                                  : sizeof(buffer) - 1U);
    WriteFile(stderr_handle, buffer, byte_count, &written, nullptr);
}

DWORD PollThreadUntilExit(HANDLE thread,
                          DWORD timeout_milliseconds,
                          ThreadContext* progress_context,
                          ThreadContext* host_context,
                          DWORD* exit_code)
{
    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.get_exit_code_thread == nullptr)
    {
        return WAIT_FAILED;
    }

    const DWORD quiet_timeout_milliseconds = 1000U;
    const DWORD start_tick = GetTickCount();
    const auto steady_start = std::chrono::steady_clock::now();
    repiu::hle::PitIrqSchedule pit_irq_schedule;
    std::uint64_t bios_tick_count = 0;
    DWORD quiet_start_tick = start_tick;
    std::uint32_t last_progress_count = 0;
    std::uint32_t last_single_step_count = 0;
    std::uint32_t last_aot_progress_count = 0;
    if (progress_context != nullptr)
    {
        last_progress_count =
            progress_context->diagnostic_progress_count.load(
                std::memory_order_relaxed);
        last_single_step_count =
            progress_context->single_step_trace_count.load(
                std::memory_order_relaxed);
        last_aot_progress_count =
            progress_context->aot_boundary_count.load(
                std::memory_order_relaxed) +
            progress_context->aot_reentry_count.load(
                std::memory_order_relaxed);
    }

    // Sample the guest thread only after full exception dispatches have been
    // silent for a full second: dispatch-active phases already report their
    // location, and the observation target is the zero-dispatch native state.
    // The composite `progressed` tracker is unsuitable as this gate because
    // lightweight AOT VEH paths (inline-cache misses, reentries) increment
    // aot_boundary/aot_reentry continuously without any dispatch.
    constexpr DWORD kNativePhaseSampleQuietMilliseconds = 1000U;
    constexpr DWORD kNativePhaseSampleIntervalMilliseconds = 500U;
    const char* sampling_environment =
        std::getenv("REPIU_NATIVE_SAMPLING");
    const bool native_sampling_enabled =
        sampling_environment == nullptr ||
        std::strcmp(sampling_environment, "0") != 0;
    Win32NativePhaseSamplerState native_sampler_state;
    // Task 411: independent of the native sampler's cadence and of its gate.
    const DWORD position_census_interval = static_cast<DWORD>(
        GuestPositionCensusIntervalMilliseconds());
    DWORD last_position_census_tick = start_tick;
    // Task 421: the music position on its own cadence, sampled here rather than
    // in the audio worker so that a worker which is not running still produces
    // a reading — its absence is the measurement.
    const DWORD cd_audio_census_interval = static_cast<DWORD>(
        CdAudioPositionCensusIntervalMilliseconds());
    DWORD last_cd_audio_census_tick = start_tick;
    std::uint32_t last_cd_audio_worker_iterations = 0;
    // Task 430: the same differencing for timer tick delivery, so each sample
    // carries the loss over its own interval instead of a run-long average that
    // cannot say whether the loss was in gameplay or in the attract demo.
    std::uint32_t last_cd_audio_ticks_due = 0;
    std::uint32_t last_cd_audio_ticks_injected = 0;
    std::uint32_t last_cd_audio_ticks_coalesced = 0;
    std::uint32_t last_cd_audio_ticks_in_gate = 0;
    std::uint32_t last_cd_audio_safe_point_traps = 0;
    // Task 412: the loader's own image range, so a host sample can name the
    // call site that led into the kernel. Resolved once; the sampling path only
    // compares against it.
    std::uint32_t loader_module_base = 0;
    std::uint32_t loader_module_size = 0;
    {
        const HMODULE loader_module = GetModuleHandleW(nullptr);
        MODULEINFO module_info = {};
        if (loader_module != nullptr &&
            GetModuleInformation(GetCurrentProcess(), loader_module,
                                 &module_info, sizeof(module_info)))
        {
            loader_module_base = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll));
            loader_module_size =
                static_cast<std::uint32_t>(module_info.SizeOfImage);
        }
    }
    DWORD dispatch_quiet_start_tick = start_tick;
    std::uint32_t last_dispatch_total = 0;
    if (progress_context != nullptr)
    {
        last_dispatch_total =
            progress_context->exception_dispatch_entry_count.load(
                std::memory_order_relaxed) +
            progress_context->exception_dispatch_exit_count.load(
                std::memory_order_relaxed);
    }
    DWORD quiet_iterations = 0;
    DWORD last_live_snapshot_tick = start_tick;
    if (progress_context != nullptr)
    {
        progress_context->live_telemetry_phase.store(
            1,
            std::memory_order_relaxed);
        WriteLiveTelemetrySnapshot(*progress_context, 0, 0);
    }
    for (DWORD iteration = 0;; ++iteration)
    {
        if (host_context != nullptr)
        {
            host_context->glide_backend.PumpHostCommands();
            host_context->glide_backend.PumpEvents();
            if (host_context->glide_backend.exit_requested())
            {
                if (progress_context != nullptr)
                {
                    WriteLiveTelemetrySnapshot(
                        *progress_context,
                        GetTickCount() - start_tick,
                        iteration + 1);
                }
                return kWin32HostExitRequested;
            }
        }
        if (progress_context != nullptr)
        {
            progress_context->diagnostic_poll_iteration_count =
                iteration + 1;

            const auto steady_elapsed =
                std::chrono::steady_clock::now() - steady_start;
            const std::uint64_t elapsed_nanoseconds =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        steady_elapsed).count());
            bios_tick_count = repiu::hle::PitTickCountForElapsed(
                elapsed_nanoseconds,
                repiu::hle::PitChannel0::kDefaultDivisor);
            repiu::runtime::WriteDosLowMemory(
                &progress_context->dos_low_memory,
                0x046CU,
                static_cast<std::uint32_t>(bios_tick_count),
                4U);

            const std::uint64_t due_interrupts = pit_irq_schedule.Poll(
                progress_context->pit_channel0.snapshot(),
                elapsed_nanoseconds);
            if (due_interrupts != 0U)
            {
                // Task 366: recorded before arming, because whether a tick was
                // still outstanding is exactly what separates a clean handoff
                // from an owed tick being discarded.
                // Task 431: whether the guest thread is inside the Glide gate
                // right now decides whether a coalesced tick was merely late or
                // could not have been delivered at all -- that window runs no
                // guest code, so no safe point is reachable.
                RecordTimerTicksDue(
                    &progress_context->timer_tick_delivery,
                    static_cast<std::uint32_t>(due_interrupts),
                    progress_context->timer_interrupt_pending.load(
                        std::memory_order_acquire),
                    TimerTickBacklogEnabled(),
                    host_context != nullptr &&
                        host_context->glide_backend.guest_in_glide_gate());
                SaturatingAtomicAdd(
                    &progress_context->last_timer_injection_ticks,
                    due_interrupts);
                if (progress_context->aot_placement != nullptr &&
                    progress_context->aot_placement
                        ->timer_source_profile.enabled)
                {
                    SaturatingAtomicAdd(
                        &progress_context->timer_interrupt_due_ticks,
                        due_interrupts);
                }
                progress_context->timer_interrupt_pending.store(
                    true, std::memory_order_release);
                ArmAotTimerSafePoint(progress_context);
            }
        }
        DWORD current_exit_code = 0;
        if (!api.get_exit_code_thread(thread, &current_exit_code))
        {
            return WAIT_FAILED;
        }

        if (current_exit_code != STILL_ACTIVE)
        {
            if (exit_code != nullptr)
            {
                *exit_code = current_exit_code;
            }
            return WAIT_OBJECT_0;
        }

        bool progressed = false;
        if (progress_context != nullptr)
        {
            const std::uint32_t progress_count =
                progress_context->diagnostic_progress_count.load(
                    std::memory_order_relaxed);
            const std::uint32_t single_step_count =
                progress_context->single_step_trace_count.load(
                    std::memory_order_relaxed);
            const std::uint32_t aot_progress_count =
                progress_context->aot_boundary_count.load(
                    std::memory_order_relaxed) +
                progress_context->aot_reentry_count.load(
                    std::memory_order_relaxed);
            progressed =
                progress_count != last_progress_count ||
                single_step_count != last_single_step_count ||
                aot_progress_count != last_aot_progress_count;
            last_progress_count = progress_count;
            last_single_step_count = single_step_count;
            last_aot_progress_count = aot_progress_count;
        }
        if (progressed)
        {
            quiet_iterations = 0;
            quiet_start_tick = GetTickCount();
        }
        else
        {
            ++quiet_iterations;
        }
        if (progress_context != nullptr)
        {
            progress_context->diagnostic_quiet_iteration_count =
                quiet_iterations;
        }

        if (timeout_milliseconds != INFINITE &&
            GetTickCount() - quiet_start_tick >=
            quiet_timeout_milliseconds)
        {
            if (progress_context != nullptr)
            {
                WriteLiveTelemetrySnapshot(
                    *progress_context,
                    GetTickCount() - start_tick,
                    iteration + 1);
            }
            return WAIT_TIMEOUT;
        }

        if (timeout_milliseconds != INFINITE &&
            GetTickCount() - start_tick >= timeout_milliseconds)
        {
            if (progress_context != nullptr)
            {
                WriteLiveTelemetrySnapshot(
                    *progress_context,
                    GetTickCount() - start_tick,
                    iteration + 1);
            }
            return WAIT_TIMEOUT;
        }

        const DWORD current_tick = GetTickCount();
        if (progress_context != nullptr)
        {
            const std::uint32_t dispatch_total =
                progress_context->exception_dispatch_entry_count.load(
                    std::memory_order_relaxed) +
                progress_context->exception_dispatch_exit_count.load(
                    std::memory_order_relaxed);
            if (dispatch_total != last_dispatch_total)
            {
                last_dispatch_total = dispatch_total;
                dispatch_quiet_start_tick = current_tick;
            }
        }
        // Task 411: the same capture, but on a plain wall-clock interval with
        // no dispatch-quiet gate. A stalled run faults continuously, so the
        // gate below never opens there, and the guest's position during the
        // stall is exactly what needs sampling.
        if (progress_context != nullptr &&
            progress_context->guest_position_census != nullptr &&
            current_tick - last_position_census_tick >=
                position_census_interval)
        {
            std::uint32_t cache_base = 0;
            std::uint32_t cache_size = 0;
            if (progress_context->aot_placement != nullptr &&
                progress_context->aot_placement->placed)
            {
                cache_base = progress_context->aot_placement->base_address;
                cache_size = static_cast<std::uint32_t>(
                    progress_context->aot_placement->size);
            }
            Win32NativePhaseSample census_sample;
            // Null telemetry: the stage marker belongs to the native sampler,
            // and overwriting it would confuse a reader of that instrument.
            if (CaptureWin32NativePhaseSample(
                    thread, progress_context->aot_placement, nullptr,
                    &census_sample, loader_module_base, loader_module_size))
            {
                const Win32GuestPositionClassification classification =
                    ClassifyGuestPosition(
                        census_sample.eip,
                        census_sample.mapped,
                        census_sample.guest_eip,
                        progress_context->runtime_base,
                        progress_context->runtime_size,
                        cache_base,
                        cache_size);
                RecordGuestPosition(
                    progress_context->guest_position_census.get(),
                    classification);
                // Task 412: the call-site axis exists only for host samples, so
                // exactly one outcome is recorded per host sample and the
                // reconciliation in the report stays meaningful.
                if (classification.origin == GuestPositionOrigin::kHost)
                {
                    RecordGuestPositionHostSite(
                        progress_context->guest_position_census.get(),
                        census_sample.host_call_site,
                        census_sample.host_scan_failed);
                }
            }
            else
            {
                RecordGuestPositionCaptureFailure(
                    progress_context->guest_position_census.get());
            }
            // Task 412: the busy-or-blocked split. One call per sample, on the
            // poll thread, and the last reading is the one reported.
            FILETIME creation_time = {};
            FILETIME exit_time = {};
            FILETIME kernel_time = {};
            FILETIME user_time = {};
            if (GetThreadTimes(thread, &creation_time, &exit_time,
                               &kernel_time, &user_time))
            {
                const auto to_100ns = [](const FILETIME& value) {
                    return (static_cast<std::uint64_t>(value.dwHighDateTime)
                            << 32) |
                        static_cast<std::uint64_t>(value.dwLowDateTime);
                };
                RecordGuestPositionThreadTime(
                    progress_context->guest_position_census.get(),
                    to_100ns(kernel_time), to_100ns(user_time),
                    static_cast<std::uint32_t>(current_tick - start_tick));
            }
            last_position_census_tick = current_tick;
        }
        // Task 421: the music position, on its own interval. `host_context` is
        // what owns the audio backend, while the census lives on the guest
        // context, so both have to be present.
        if (progress_context != nullptr && host_context != nullptr &&
            progress_context->cd_audio_position_census != nullptr &&
            current_tick - last_cd_audio_census_tick >=
                cd_audio_census_interval)
        {
            Win32CdAudioPositionEntry entry;
            entry.wall_milliseconds =
                static_cast<std::uint32_t>(current_tick - start_tick);
            host_context->cd_audio.FillPositionSample(&entry);
            // Report iterations *since the previous sample*: a zero here is the
            // worker not having run across the whole interval.
            const std::uint32_t iterations = entry.worker_iterations;
            entry.worker_iterations =
                iterations - last_cd_audio_worker_iterations;
            last_cd_audio_worker_iterations = iterations;
            // Task 430: the guest's clock advances once per injected tick while
            // the music advances on real time, so this pair is what separates a
            // drifting guest clock from a healthy one — on the same time axis
            // as the position beside it.
            const Win32TimerTickDeliverySnapshot ticks =
                SnapshotTimerTickDelivery(
                    progress_context->timer_tick_delivery);
            entry.timer_ticks_due =
                ticks.due_total - last_cd_audio_ticks_due;
            entry.timer_ticks_injected =
                ticks.injected_total - last_cd_audio_ticks_injected;
            last_cd_audio_ticks_due = ticks.due_total;
            last_cd_audio_ticks_injected = ticks.injected_total;
            // Task 431: the opportunity side, differenced the same way. The
            // trap count is where the injections come from, so the two read
            // together say whether the ticks were refused or never offered.
            entry.ticks_coalesced =
                ticks.coalesced_total - last_cd_audio_ticks_coalesced;
            last_cd_audio_ticks_coalesced = ticks.coalesced_total;
            entry.ticks_coalesced_in_gate =
                ticks.coalesced_in_gate_total - last_cd_audio_ticks_in_gate;
            last_cd_audio_ticks_in_gate = ticks.coalesced_in_gate_total;
            const std::uint32_t safe_point_traps =
                progress_context->aot_placement != nullptr
                    ? progress_context->aot_placement
                          ->timer_safe_point_trap_count
                    : 0U;
            entry.safe_point_traps =
                safe_point_traps - last_cd_audio_safe_point_traps;
            last_cd_audio_safe_point_traps = safe_point_traps;
            RecordCdAudioPosition(
                progress_context->cd_audio_position_census.get(), entry);
            last_cd_audio_census_tick = current_tick;
        }

        if (native_sampling_enabled && progress_context != nullptr &&
            current_tick - dispatch_quiet_start_tick >=
                kNativePhaseSampleQuietMilliseconds &&
            current_tick - native_sampler_state.last_sample_tick >=
                kNativePhaseSampleIntervalMilliseconds)
        {
            Win32NativePhaseSample sample;
            if (CaptureWin32NativePhaseSample(
                    thread, progress_context->aot_placement,
                    progress_context->shared_live_telemetry, &sample))
            {
                sample.last_indirect_source =
                    progress_context->aot_last_indirect_source.load(
                        std::memory_order_relaxed);
                sample.last_indirect_target =
                    progress_context->aot_last_indirect_target.load(
                        std::memory_order_relaxed);
                RecordWin32NativePhaseSample(
                    sample,
                    &native_sampler_state,
                    progress_context->shared_live_telemetry);
            }
            WriteWin32NativePhaseSampleLine(sample,
                                            native_sampler_state,
                                            current_tick - start_tick);
            native_sampler_state.last_sample_tick = current_tick;
        }
        if (progress_context != nullptr &&
            current_tick - last_live_snapshot_tick >= 1000U)
        {
            WriteLiveTelemetrySnapshot(*progress_context,
                                       current_tick - start_tick,
                                       iteration + 1);
            last_live_snapshot_tick = current_tick;
        }
        // Task 333: the loop used to end in Sleep(1), so a Glide command
        // published just after the pump above waited out the whole sleep before
        // anyone looked at it — measured as the dominant part of a gate call.
        // Waiting on the command instead keeps the same 1ms poll cadence when
        // nothing is posted and wakes immediately when something is.
        if (host_context != nullptr && GlideHostCommandWaitEnabled())
        {
            host_context->glide_backend.WaitAndPumpHostCommands(1U);
        }
        else
        {
            Sleep(1);
        }
    }
}

void CopySnapshotFromContextRecord(const CONTEXT& source,
                                   X86ExecutionSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return;
    }

#if defined(_M_IX86)
    snapshot->captured = true;
    snapshot->eip = source.Eip;
    snapshot->eax = source.Eax;
    snapshot->ebx = source.Ebx;
    snapshot->ecx = source.Ecx;
    snapshot->edx = source.Edx;
    snapshot->esi = source.Esi;
    snapshot->edi = source.Edi;
    snapshot->esp = source.Esp;
    snapshot->ebp = source.Ebp;
    snapshot->eflags = source.EFlags;
    snapshot->cs = static_cast<std::uint16_t>(source.SegCs);
    snapshot->ds = static_cast<std::uint16_t>(source.SegDs);
    snapshot->es = static_cast<std::uint16_t>(source.SegEs);
    snapshot->ss = static_cast<std::uint16_t>(source.SegSs);
    snapshot->fs = static_cast<std::uint16_t>(source.SegFs);
    snapshot->gs = static_cast<std::uint16_t>(source.SegGs);
#else
    (void)source;
    snapshot->captured = false;
#endif
}

void CaptureSuspendedThreadSnapshot(HANDLE thread,
                                    X86ExecutionSnapshot* snapshot)
{
    if (thread == nullptr || snapshot == nullptr)
    {
        return;
    }

#if defined(_M_IX86)
    const Win32ThreadApi& api = GetWin32ThreadApi();
    if (api.suspend_thread == nullptr ||
        api.get_thread_context == nullptr ||
        api.resume_thread == nullptr)
    {
        return;
    }

    if (api.suspend_thread(thread) == static_cast<DWORD>(-1))
    {
        return;
    }

    CONTEXT thread_context = {};
    thread_context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
    if (api.get_thread_context(thread, &thread_context))
    {
        CopySnapshotFromContextRecord(thread_context, snapshot);
    }
    api.resume_thread(thread);
#else
    (void)thread;
    snapshot->captured = false;
#endif
}

bool BuildSingleStepSnapshot(const ThreadContext& context,
                             X86ExecutionSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return false;
    }

    const std::uint32_t count =
        context.single_step_trace_count.load(std::memory_order_relaxed);
    if (count == 0)
    {
        *snapshot = X86ExecutionSnapshot{};
        return false;
    }

    snapshot->captured = true;
    snapshot->eip =
        context.single_step_eip.load(std::memory_order_relaxed);
    snapshot->eax =
        context.single_step_eax.load(std::memory_order_relaxed);
    snapshot->ebx =
        context.single_step_ebx.load(std::memory_order_relaxed);
    snapshot->ecx =
        context.single_step_ecx.load(std::memory_order_relaxed);
    snapshot->edx =
        context.single_step_edx.load(std::memory_order_relaxed);
    snapshot->esi =
        context.single_step_esi.load(std::memory_order_relaxed);
    snapshot->edi =
        context.single_step_edi.load(std::memory_order_relaxed);
    snapshot->esp =
        context.single_step_esp.load(std::memory_order_relaxed);
    snapshot->ebp =
        context.single_step_ebp.load(std::memory_order_relaxed);
    snapshot->eflags =
        context.single_step_eflags.load(std::memory_order_relaxed);
    snapshot->cs = static_cast<std::uint16_t>(
        context.single_step_cs.load(std::memory_order_relaxed));
    snapshot->ds = static_cast<std::uint16_t>(
        context.single_step_ds.load(std::memory_order_relaxed));
    snapshot->es = static_cast<std::uint16_t>(
        context.single_step_es.load(std::memory_order_relaxed));
    snapshot->ss = static_cast<std::uint16_t>(
        context.single_step_ss.load(std::memory_order_relaxed));
    snapshot->fs = static_cast<std::uint16_t>(
        context.single_step_fs.load(std::memory_order_relaxed));
    snapshot->gs = static_cast<std::uint16_t>(
        context.single_step_gs.load(std::memory_order_relaxed));
    return true;
}

void CopyThreadObservationToAttempt(const ThreadContext& context,
                                    Win32MinimalExecutionAttempt* attempt)
{
    if (attempt == nullptr)
    {
        return;
    }

    attempt->dos_termination_captured = context.dos_termination_captured;
    attempt->dos_termination_ax = context.dos_termination_ax;
    attempt->dos_termination_eip = context.dos_termination_eip;
    attempt->dos_termination_esp = context.dos_termination_esp;
    std::memcpy(attempt->dos_termination_stack,
                context.dos_termination_stack,
                sizeof(attempt->dos_termination_stack));

    attempt->single_step_trace_count =
        context.single_step_trace_count.load(std::memory_order_relaxed);
    // Task 337: read after the guest thread has stopped, so plain counters.
    attempt->veh_single_step_exception_count =
        context.veh_single_step_exception_count;
    attempt->veh_breakpoint_exception_count =
        context.veh_breakpoint_exception_count;
    attempt->veh_access_violation_exception_count =
        context.veh_access_violation_exception_count;
    attempt->veh_other_exception_count = context.veh_other_exception_count;
    attempt->veh_other_exception_code_overflow =
        context.veh_other_exception_code_overflow;
    for (std::uint32_t index = 0;
         index < ThreadContext::kOtherExceptionCodeCapacity; ++index)
    {
        attempt->veh_other_exception_codes[index] =
            context.veh_other_exception_codes[index];
        attempt->veh_other_exception_code_counts[index] =
            context.veh_other_exception_code_counts[index];
    }
    attempt->veh_single_step_run_total = context.veh_single_step_run_total;
    attempt->veh_single_step_run_max = context.veh_single_step_run_max;
    std::memcpy(attempt->veh_single_step_run_buckets,
                context.veh_single_step_run_buckets,
                sizeof(attempt->veh_single_step_run_buckets));
    attempt->hle_reentry_reject_not_pending =
        context.hle_reentry_reject_not_pending;
    attempt->hle_reentry_reject_segment_write =
        context.hle_reentry_reject_segment_write;
    attempt->hle_reentry_reject_outside_arena =
        context.hle_reentry_reject_outside_arena;
    attempt->hle_reentry_reject_quarantined =
        context.hle_reentry_reject_quarantined;
    attempt->hle_reentry_reject_cache_miss =
        context.hle_reentry_reject_cache_miss;
    attempt->hle_reentry_reject_span_unsafe =
        context.hle_reentry_reject_span_unsafe;
    attempt->hle_reentry_success = context.hle_reentry_success;
    attempt->hle_reentry_segment_write_resumed =
        context.hle_reentry_segment_write_resumed;
    attempt->quarantine_trace_count = context.quarantine_trace_count;
    attempt->quarantine_unknown_source_count =
        context.quarantine_unknown_source_count;
    attempt->quarantine_deferred_count = context.quarantine_deferred_count;
    attempt->guest_page_write_history_overflow =
        context.guest_page_write_history_overflow;
    for (std::uint32_t index = 0;
         index < ThreadContext::kQuarantineTraceCapacity; ++index)
    {
        attempt->quarantine_trace[index] = {
            context.quarantine_trace[index].page,
            context.quarantine_trace[index].source,
            context.quarantine_trace[index].destination,
            context.quarantine_trace[index].byte_count};
    }
    static_assert(
        sizeof(attempt->generation_failure_trace) /
            sizeof(attempt->generation_failure_trace[0]) ==
            ThreadContext::kGenerationFailureTraceCapacity,
        "generation failure trace capacities must match");
    static_assert(
        sizeof(attempt->generation_failure_trace[0].message) ==
            ThreadContext::kGenerationFailureMessageCapacity,
        "generation failure message capacities must match");
    attempt->generation_failure_trace_count =
        context.generation_failure_trace_count;
    attempt->generation_failure_trace_overflow =
        context.generation_failure_trace_overflow;
    for (std::uint32_t index = 0;
         index < ThreadContext::kGenerationFailureTraceCapacity; ++index)
    {
        const auto& source = context.generation_failure_trace[index];
        auto& destination = attempt->generation_failure_trace[index];
        destination.target = source.target;
        destination.page = source.page;
        destination.quarantined = source.quarantined;
        destination.terminal = source.terminal;
        std::memcpy(destination.message, source.message,
                    sizeof(destination.message));
    }
    static_assert(
        sizeof(attempt->port_io_address_census) /
            sizeof(attempt->port_io_address_census[0]) ==
            ThreadContext::kPortIoAddressCensusCapacity,
        "port I/O address census capacities must match");
    attempt->port_io_address_census_size =
        context.port_io_address_census_size;
    attempt->port_io_address_census_overflow =
        context.port_io_address_census_overflow;
    for (std::uint32_t index = 0;
         index < ThreadContext::kPortIoAddressCensusCapacity; ++index)
    {
        attempt->port_io_address_census[index] = {
            context.port_io_address_census[index].guest_address,
            context.port_io_address_census[index].count,
            context.port_io_address_census[index].cache_count,
            context.port_io_address_census[index].mapped_count,
            context.port_io_address_census[index].reentry_pending_count,
            context.port_io_address_census[index].entry_transition_count,
            context.port_io_address_census[index].entry_previous_code,
            context.port_io_address_census[index].entry_previous_eip,
            context.port_io_address_census[index].entry_flags,
            context.port_io_address_census[index].entry_prev_single_step,
            context.port_io_address_census[index].entry_prev_breakpoint,
            context.port_io_address_census[index].entry_prev_access_violation,
            context.port_io_address_census[index].entry_prev_other,
            context.port_io_address_census[index].entry_previous_exit_site,
            context.port_io_address_census[index].entry_previous_exit_eip};
    }
    static_assert(
        sizeof(attempt->arena_port_io_entry_trace) /
            sizeof(attempt->arena_port_io_entry_trace[0]) ==
            ThreadContext::kArenaPortIoEntryTraceCapacity,
        "arena port I/O entry trace capacities must match");
    attempt->arena_port_io_entry_trace_count =
        context.arena_port_io_entry_trace_count;
    for (std::uint32_t index = 0;
         index < ThreadContext::kArenaPortIoEntryTraceCapacity; ++index)
    {
        const auto& source = context.arena_port_io_entry_trace[index];
        attempt->arena_port_io_entry_trace[index] = {
            source.guest_address, source.previous_code, source.previous_eip,
            source.previous_in_cache, source.trap_flag, source.reentry_pending,
            source.legacy_fallback, source.single_step_trace};
    }
    // Task 410: the exit-site histogram. The snapshot array is sized generously
    // and asserted against the enumeration, so adding a site cannot silently
    // truncate the report.
    static_assert(
        kVehExitSiteCount <=
            Win32MinimalExecutionAttempt::kVehExitSiteSnapshotCapacity,
        "VEH exit site snapshot capacity must cover the enumeration");
    attempt->veh_arena_single_step_count =
        context.veh_arena_single_step_count;
    for (std::uint32_t site = 0; site < kVehExitSiteCount; ++site)
    {
        attempt->veh_arena_single_step_exit_site_counts[site] =
            context.veh_arena_single_step_exit_site_counts[site];
    }
    attempt->single_step_hotspot_profile =
        context.single_step_hotspot_profile != nullptr
            ? SnapshotSingleStepHotspotProfile(
                  *context.single_step_hotspot_profile)
            : Win32SingleStepHotspotProfileSnapshot{};
    // Task 400: this runs exactly once per attempt, on both the interrupted and
    // the normal teardown path, so the full-table dump belongs here rather than
    // inside the snapshot function.
    if (context.single_step_hotspot_profile != nullptr)
    {
        attempt->single_step_hotspot_profile.dump_written =
            WriteSingleStepHotspotDumpIfEnabled(
                context.single_step_hotspot_profile.get(),
                &attempt->single_step_hotspot_profile.dump_entry_count,
                &attempt->single_step_hotspot_profile.dump_path);
    }
    // Task 411: same placement and the same reason as the hotspot dump above --
    // this runs once per attempt on both teardown paths, and writing the file
    // here keeps it ahead of a stalled Glide close.
    attempt->guest_position_census =
        context.guest_position_census != nullptr
            ? SnapshotGuestPositionCensus(*context.guest_position_census)
            : Win32GuestPositionCensusSnapshot{};
    if (context.guest_position_census != nullptr)
    {
        attempt->guest_position_census.dump_written =
            WriteGuestPositionCensusDumpIfEnabled(
                context.guest_position_census.get(),
                &attempt->guest_position_census.dump_entry_count,
                &attempt->guest_position_census.dump_path);
        // Task 412: module and symbol lookup runs here, after the guest thread
        // has stopped, so nothing on the sampling path pays for it.
        ResolveGuestPositionCensusSymbols(&attempt->guest_position_census);
    }
    // Task 421: written here for the same reason -- ahead of a Glide close that
    // may stall, so the series survives a teardown that does not finish.
    if (context.cd_audio_position_census != nullptr)
    {
        WriteCdAudioPositionCensusDump(
            *context.cd_audio_position_census,
            &attempt->cd_audio_position_dump_entry_count,
            &attempt->cd_audio_position_regression_count);
    }
    // Task 422: same placement, same reason.
    if (context.mscdex_command_trace != nullptr)
    {
        WriteMscdexCommandTraceDump(
            *context.mscdex_command_trace,
            &attempt->mscdex_command_trace_entry_count);
        attempt->mscdex_command_trace_total =
            context.mscdex_command_trace->total_commands;
    }
    attempt->execution_time_profile =
        context.execution_time_profile != nullptr
            ? SnapshotExecutionTimeProfile(*context.execution_time_profile)
            : Win32ExecutionTimeProfileSnapshot{};
    attempt->aot_worker_timing =
        context.aot_worker_timing != nullptr
            ? SnapshotAotWorkerTiming(*context.aot_worker_timing)
            : Win32AotWorkerTimingSnapshot{};
    // Task 333: read after the guest thread has stopped, so the backend's
    // counters are quiescent and no lock is needed here.
    attempt->glide_gate_timing = context.glide_backend.glide_gate_timing();
    attempt->glide_rendezvous_spin =
        context.glide_backend.rendezvous_spin_counts();
    attempt->glide_ordinal_timing =
        SnapshotGlideOrdinalTiming(context.glide_ordinal_timing);
    attempt->glide_buffer_swap_timing =
        context.glide_backend.glide_buffer_swap_timing();
    attempt->glide_setter_census =
        SnapshotGlideSetterCensus(context.glide_setter_census);
    attempt->glide_setter_phase_timing =
        context.glide_backend.glide_setter_phase_timing();
    attempt->glide_setter_state_cache =
        SnapshotGlideSetterStateCache(context.glide_setter_state_cache);
    attempt->glide_gl_error_policy =
        context.glide_backend.glide_gl_error_policy();
    attempt->glide_swap_interval_policy =
        context.glide_backend.glide_swap_interval_policy();
    attempt->glide_texture_census =
        context.glide_backend.glide_texture_census();
    attempt->out_of_arena_step_census =
        SnapshotOutOfArenaStepCensus(context.out_of_arena_step_census);
    attempt->timer_tick_delivery =
        SnapshotTimerTickDelivery(context.timer_tick_delivery);
    attempt->native_fast_path_entry_count =
        context.native_fast_path.entry_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_return_count =
        context.native_fast_path.return_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_cancel_count =
        context.native_fast_path.cancel_count.load(std::memory_order_relaxed);
    attempt->native_fast_path_last_entry =
        context.native_fast_path.last_entry;
    attempt->native_fast_path_last_return =
        context.native_fast_path.last_return;
    attempt->native_linear_span_entry_count =
        context.native_fast_path.linear_span_entry_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_boundary_count =
        context.native_fast_path.linear_span_boundary_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_count =
        context.native_fast_path.linear_span_cancel_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_tf_count =
        context.native_fast_path.linear_span_cancel_tf_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr0_count =
        context.native_fast_path.linear_span_cancel_dr0_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr1_count =
        context.native_fast_path.linear_span_cancel_dr1_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr2_count =
        context.native_fast_path.linear_span_cancel_dr2_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr3_count =
        context.native_fast_path.linear_span_cancel_dr3_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_other_db_count =
        context.native_fast_path.linear_span_cancel_other_db_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_tf_first_eip =
        context.native_fast_path.linear_span_cancel_tf_first_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr0_first_eip =
        context.native_fast_path.linear_span_cancel_dr0_first_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr1_first_eip =
        context.native_fast_path.linear_span_cancel_dr1_first_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr2_first_eip =
        context.native_fast_path.linear_span_cancel_dr2_first_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_dr3_first_eip =
        context.native_fast_path.linear_span_cancel_dr3_first_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cancel_other_db_first_eip =
        context.native_fast_path.linear_span_cancel_other_db_first_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_instruction_total =
        context.native_fast_path.linear_span_instruction_total.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_reject_count =
        context.native_fast_path.linear_span_reject_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cache_hit_count =
        context.native_fast_path.linear_span_cache_hit_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_cache_miss_count =
        context.native_fast_path.linear_span_cache_miss_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_reject_cache_hit_count =
        context.native_fast_path.linear_span_reject_cache_hit_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_reject_cache_miss_count =
        context.native_fast_path.linear_span_reject_cache_miss_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_reject_cache_stale_count =
        context.native_fast_path.linear_span_reject_cache_stale_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_reject_cache_store_count =
        context.native_fast_path.linear_span_reject_cache_store_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_reject_cache_capacity_skip_count =
        context.native_fast_path
            .linear_span_reject_cache_capacity_skip_count.load(
                std::memory_order_relaxed);
    attempt->native_linear_span_write_cross_count =
        context.native_fast_path.linear_span_write_cross_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_write_guard_uncovered_count =
        context.native_fast_path.linear_span_write_guard_uncovered_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_write_fault_cancel_count =
        context.native_fast_path.linear_span_write_fault_cancel_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_last_cancel_code =
        context.native_fast_path.linear_span_last_cancel_code.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_last_cancel_eip =
        context.native_fast_path.linear_span_last_cancel_eip.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_direct_jump_chain_count =
        context.native_fast_path.linear_span_direct_jump_chain_count.load(
            std::memory_order_relaxed);
    attempt->native_linear_span_backward_jump_stop_count =
        context.native_fast_path.linear_span_backward_jump_stop_count.load(
            std::memory_order_relaxed);
    attempt->execution_backend = context.execution_backend;
    attempt->aot_backend_active = context.aot_placement != nullptr;
    attempt->aot_cache_entry_count = context.aot_cache_entry_count.load(
        std::memory_order_relaxed);
    attempt->aot_boundary_count = context.aot_boundary_count.load(
        std::memory_order_relaxed);
    attempt->aot_boundary_return_count =
        context.aot_boundary_return_count.load(std::memory_order_relaxed);
    attempt->aot_boundary_indirect_count =
        context.aot_boundary_indirect_count.load(std::memory_order_relaxed);
    attempt->aot_boundary_direct_count =
        context.aot_boundary_direct_count.load(std::memory_order_relaxed);
    attempt->aot_boundary_conditional_count =
        context.aot_boundary_conditional_count.load(std::memory_order_relaxed);
    attempt->aot_boundary_other_count =
        context.aot_boundary_other_count.load(std::memory_order_relaxed);
    for (std::uint32_t index = 0;
         index < kAotCacheBreakpointProvenanceCount; ++index)
    {
        attempt->aot_breakpoint_provenance_counts[index] =
            context.aot_breakpoint_provenance_counts[index].load(
                std::memory_order_relaxed);
    }
    // Task 263(a): top-8 lead opcodes of the `other` boundary bucket.
    {
        std::uint32_t histogram[256];
        for (int i = 0; i < 256; ++i)
        {
            histogram[i] = context.aot_other_opcode_histogram[i];
        }
        for (int slot = 0; slot < 8; ++slot)
        {
            int best = 0;
            for (int i = 1; i < 256; ++i)
            {
                if (histogram[i] > histogram[best])
                {
                    best = i;
                }
            }
            attempt->aot_other_top_opcodes[slot] =
                static_cast<std::uint32_t>(best);
            attempt->aot_other_top_counts[slot] = histogram[best];
            histogram[best] = 0;
        }
    }
    // Task 367: the same samples ranked by real instruction rather than by lead
    // byte. Sorting happens here, at exit, never on the hot path.
    {
        const Win32AotBoundaryOpcodeCensus& census =
            context.aot_boundary_opcode_census;
        RankAotOpcodeHistogram(
            census.effective_opcode_counts,
            attempt->aot_effective_opcode_ranks,
            Win32MinimalExecutionAttempt::kAotOpcodeRankCount);
        RankAotOpcodeHistogram(
            census.escape_opcode_counts,
            attempt->aot_escape_opcode_ranks,
            Win32MinimalExecutionAttempt::kAotOpcodeRankCount);
        attempt->aot_opcode_census_samples = census.sample_count;
        attempt->aot_opcode_census_escapes = census.escape_count;
        attempt->aot_opcode_census_prefixed = census.prefixed_count;
        attempt->aot_opcode_census_segment_prefixed =
            census.segment_prefixed_count;
        attempt->aot_opcode_census_operand_size_prefixed =
            census.operand_size_prefixed_count;
        attempt->aot_opcode_census_truncated = census.escape_truncated_count;
        attempt->aot_opcode_census_prefix_overflow =
            census.prefix_overflow_count;
        attempt->aot_opcode_census_empty = census.empty_sample_count;
    }
    attempt->aot_last_other_eip =
        context.aot_last_other_boundary_eip.load(std::memory_order_relaxed);
    attempt->aot_last_other_bytes =
        context.aot_last_other_boundary_bytes.load(std::memory_order_relaxed);
    attempt->aot_residency_total =
        context.aot_residency_instruction_total.load(std::memory_order_relaxed);
    attempt->aot_residency_samples =
        context.aot_residency_sample_count.load(std::memory_order_relaxed);
    attempt->aot_residency_max =
        context.aot_residency_max.load(std::memory_order_relaxed);
    attempt->aot_reentry_count = context.aot_reentry_count.load(
        std::memory_order_relaxed);
    attempt->aot_legacy_fallback_count =
        context.aot_legacy_fallback_count.load(std::memory_order_relaxed);
    attempt->aot_last_fallback_address =
        context.aot_last_fallback_address.load(std::memory_order_relaxed);
    attempt->aot_dynamic_attempt_count =
        context.aot_dynamic_attempt_count.load(std::memory_order_relaxed);
    attempt->aot_dynamic_success_count =
        context.aot_dynamic_success_count.load(std::memory_order_relaxed);
    attempt->aot_dynamic_added_bytes =
        context.aot_dynamic_added_bytes.load(std::memory_order_relaxed);
    attempt->aot_dbt_hle_reentry_attempt_count =
        context.aot_dbt_hle_reentry_attempt_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_reentry_success_count =
        context.aot_dbt_hle_reentry_success_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_translation_attempt_count =
        context.aot_dbt_hle_translation_attempt_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_translation_success_count =
        context.aot_dbt_hle_translation_success_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_entry_count =
        context.aot_dbt_hle_dispatch_entry_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_success_count =
        context.aot_dbt_hle_dispatch_success_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_fallback_count =
        context.aot_dbt_hle_dispatch_fallback_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_attempt_count =
        attempt->aot_dbt_hle_dispatch_success_count +
        attempt->aot_dbt_hle_dispatch_fallback_count;
    for (std::uint32_t index = 0;
         index < kAotDbtHleFallbackReasonCount; ++index)
    {
        attempt->aot_dbt_hle_dispatch_fallback_reason_counts[index] =
            context.aot_dbt_hle_dispatch_fallback_reason_counts[index].load(
                std::memory_order_relaxed);
    }
    attempt->aot_selector_guard_native_site_count =
        context.aot_selector_guard_native_site_count.load(
            std::memory_order_relaxed);
    attempt->aot_selector_guard_hle_site_count =
        context.aot_selector_guard_hle_site_count.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_last_source =
        context.aot_dbt_hle_dispatch_last_source.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_last_next =
        context.aot_dbt_hle_dispatch_last_next.load(
            std::memory_order_relaxed);
    attempt->aot_dbt_hle_dispatch_last_bytes =
        context.aot_dbt_hle_dispatch_last_bytes.load(
            std::memory_order_relaxed);
    attempt->aot_selector_guard_unresolved_site_count =
        context.aot_selector_guard_unresolved_site_count.load(
            std::memory_order_relaxed);
    attempt->aot_selector_guard_hle_exit_count =
        context.aot_selector_guard_hle_exit_count.load(
            std::memory_order_relaxed);
    attempt->aot_selector_guard_mismatch_count =
        context.aot_selector_guard_mismatch_count.load(
            std::memory_order_relaxed);
    if (context.aot_placement != nullptr)
    {
        attempt->aot_guarded_segment_pop_success_count =
            context.aot_placement->guarded_segment_pop_success_count;
        attempt->aot_guarded_segment_pop_fallback_count =
            context.aot_placement->guarded_segment_pop_fallback_count;
        attempt->aot_guarded_segment_load_success_count =
            context.aot_placement->guarded_segment_load_success_count;
        attempt->aot_guarded_segment_load_fallback_count =
            context.aot_placement->guarded_segment_load_fallback_count;
        attempt->aot_timer_safe_point_trap_count =
            context.aot_placement->timer_safe_point_trap_count;
        attempt->aot_timer_safe_point_injected_count =
            context.aot_placement->timer_safe_point_injected_count;
        attempt->aot_timer_safe_point_deferred_count =
            context.aot_placement->timer_safe_point_deferred_count;
        attempt->aot_timer_source_profile =
            context.aot_placement->timer_source_profile;
    }
    attempt->aot_dbt_return_entry_count =
        context.aot_dbt_return_entry_count.load(std::memory_order_relaxed);
    attempt->aot_dbt_return_success_count =
        context.aot_dbt_return_success_count.load(std::memory_order_relaxed);
    attempt->aot_dbt_return_fallback_count =
        context.aot_dbt_return_fallback_count.load(std::memory_order_relaxed);
    // Derive the reported attempt as success + fallback so the accounting
    // invariant also holds for a sample whose graceful timeout landed inside the
    // resolver (Task 281 open item, corrected in Task 282).
    attempt->aot_dbt_return_attempt_count =
        attempt->aot_dbt_return_success_count +
        attempt->aot_dbt_return_fallback_count;
    for (std::uint32_t index = 0;
         index < kAotDbtDispatchFallbackReasonCount; ++index)
    {
        attempt->aot_dbt_return_fallback_reason_counts[index] =
            context.aot_dbt_return_fallback_reason_counts[index].load(
                std::memory_order_relaxed);
    }
    attempt->aot_dbt_indirect_entry_count =
        context.aot_dbt_indirect_entry_count.load(std::memory_order_relaxed);
    attempt->aot_dbt_indirect_success_count =
        context.aot_dbt_indirect_success_count.load(std::memory_order_relaxed);
    attempt->aot_dbt_indirect_fallback_count =
        context.aot_dbt_indirect_fallback_count.load(std::memory_order_relaxed);
    attempt->aot_dbt_indirect_attempt_count =
        attempt->aot_dbt_indirect_success_count +
        attempt->aot_dbt_indirect_fallback_count;
    for (std::uint32_t index = 0;
         index < kAotDbtDispatchFallbackReasonCount; ++index)
    {
        attempt->aot_dbt_indirect_fallback_reason_counts[index] =
            context.aot_dbt_indirect_fallback_reason_counts[index].load(
                std::memory_order_relaxed);
    }
    attempt->aot_indirect_dispatch_count =
        context.aot_indirect_dispatch_count.load(std::memory_order_relaxed);
    attempt->aot_inline_cache_patch_attempt_count =
        context.aot_inline_cache_patch_attempt_count.load(
            std::memory_order_relaxed);
    attempt->aot_inline_cache_patch_success_count =
        context.aot_inline_cache_patch_success_count.load(
            std::memory_order_relaxed);
    attempt->aot_inline_cache_site_count = context.aot_placement != nullptr
        ? static_cast<std::uint32_t>(
              context.aot_placement->indirect_inline_cache_sites.size())
        : 0U;
    attempt->aot_last_reentry_cache_address =
        context.aot_reentry_cache_address;
    attempt->aot_code_write_count =
        context.aot_code_write_count.load(std::memory_order_relaxed);
    attempt->aot_page_retire_attempt_count =
        context.aot_page_retire_attempt_count.load(std::memory_order_relaxed);
    attempt->aot_page_retire_success_count =
        context.aot_page_retire_success_count.load(std::memory_order_relaxed);
    attempt->aot_generation_publish_count =
        context.aot_generation_publish_count.load(std::memory_order_relaxed);
    attempt->aot_generation_failure_count =
        context.aot_generation_failure_count.load(std::memory_order_relaxed);
    attempt->aot_generation_relinked_entry_count =
        context.aot_generation_relinked_entry_count.load(
            std::memory_order_relaxed);
    attempt->aot_retired_entry_trap_count =
        context.aot_retired_entry_trap_count.load(std::memory_order_relaxed);
    attempt->aot_retired_trap_profile =
        SnapshotAotRetiredTrapProfile(context.aot_retired_trap_profile);
    attempt->aot_retired_span_attempt_count =
        context.aot_retired_span_attempt_count.load(
            std::memory_order_relaxed);
    attempt->aot_retired_span_success_count =
        context.aot_retired_span_success_count.load(
            std::memory_order_relaxed);
    attempt->aot_quarantine_count =
        context.aot_quarantine_count.load(std::memory_order_relaxed);
    attempt->aot_last_code_write_source =
        context.aot_last_code_write_source.load(std::memory_order_relaxed);
    attempt->aot_last_code_write_destination =
        context.aot_last_code_write_destination.load(
            std::memory_order_relaxed);
    attempt->aot_last_retired_page =
        context.aot_last_retired_page.load(std::memory_order_relaxed);
    attempt->aot_last_published_generation =
        context.aot_last_published_generation.load(
            std::memory_order_relaxed);
    attempt->aot_exception_mapping_valid =
        context.aot_exception_mapping_valid;
    attempt->aot_exception_cache_address =
        context.aot_exception_cache_address;
    attempt->aot_exception_guest_address =
        context.aot_exception_guest_address;
    std::memcpy(attempt->aot_exception_cache_bytes,
                context.aot_exception_cache_bytes,
                sizeof(attempt->aot_exception_cache_bytes));
    std::memcpy(attempt->aot_exception_guest_bytes,
                context.aot_exception_guest_bytes,
                sizeof(attempt->aot_exception_guest_bytes));
    attempt->aot_last_indirect_source =
        context.aot_last_indirect_source.load(std::memory_order_relaxed);
    attempt->aot_last_indirect_target =
        context.aot_last_indirect_target.load(std::memory_order_relaxed);
    attempt->aot_return_dispatch_count =
        context.aot_return_dispatch_count.load(std::memory_order_relaxed);
    attempt->aot_last_return_target =
        context.aot_last_return_target.load(std::memory_order_relaxed);
    attempt->aot_last_return_source =
        context.aot_last_return_source.load(std::memory_order_relaxed);
    std::memcpy(attempt->aot_last_return_stack,
                context.aot_last_return_stack,
                sizeof(attempt->aot_last_return_stack));
    attempt->execution_probe_configured = context.execution_probe_configured;
    attempt->execution_probe_hit = context.execution_probe_hit;
    attempt->execution_probe_offset = context.execution_probe_offset;
    attempt->execution_probe_snapshot = context.execution_probe_snapshot;
    std::memcpy(attempt->execution_probe_stack,
                context.execution_probe_stack,
                sizeof(attempt->execution_probe_stack));
    attempt->execution_trace_configured = context.execution_trace_configured;
    attempt->execution_trace_start_offset = context.execution_trace_start_offset;
    attempt->execution_trace_end_offset = context.execution_trace_end_offset;
    attempt->execution_trace_esp_offset = context.execution_trace_esp_offset;
    attempt->execution_trace_hit_count = context.execution_trace_hit_count;
    attempt->execution_trace_sentinel2_configured =
        context.execution_trace_sentinel2_configured;
    attempt->execution_trace_sentinel2_offset =
        context.execution_trace_sentinel2_offset;
    attempt->execution_trace_sentinel_rearm_count =
        context.execution_trace_sentinel_rearm_count;
    std::memcpy(attempt->execution_trace, context.execution_trace,
                sizeof(attempt->execution_trace));
    attempt->aot_call_depth = context.aot_call_depth;
    attempt->aot_last_return_matches_call =
        context.aot_last_return_matches_call;
    attempt->aot_last_expected_return = context.aot_last_expected_return;
    attempt->aot_last_call_source = context.aot_last_call_source;
    attempt->aot_last_call_target = context.aot_last_call_target;
    attempt->aot_last_expected_call_source =
        context.aot_last_expected_call_source;
    attempt->aot_last_expected_call_target =
        context.aot_last_expected_call_target;
    attempt->aot_return_trace_count = context.aot_return_trace_count;
    std::memcpy(attempt->aot_return_trace, context.aot_return_trace,
                sizeof(attempt->aot_return_trace));
    attempt->aot_transfer_trace_count = context.aot_transfer_trace_count;
    std::memcpy(attempt->aot_transfer_trace, context.aot_transfer_trace,
                sizeof(attempt->aot_transfer_trace));
    attempt->aot_dbt_call_return_trace_configured =
        context.aot_dbt_call_return_trace_configured;
    attempt->aot_dbt_call_return_trace_count =
        context.aot_dbt_call_return_trace_count;
    attempt->aot_dbt_call_return_call_count =
        context.aot_dbt_call_return_call_count;
    attempt->aot_dbt_call_return_return_count =
        context.aot_dbt_call_return_return_count;
    attempt->aot_dbt_call_return_match_count =
        context.aot_dbt_call_return_match_count;
    attempt->aot_dbt_call_return_mismatch_count =
        context.aot_dbt_call_return_mismatch_count;
    attempt->aot_dbt_call_return_overwrite_count =
        context.aot_dbt_call_return_overwrite_count;
    attempt->aot_dbt_call_return_first_divergence_valid =
        context.aot_dbt_call_return_first_divergence_valid;
    attempt->aot_dbt_call_return_first_divergence =
        context.aot_dbt_call_return_first_divergence;
    std::memcpy(attempt->aot_dbt_call_return_trace,
                context.aot_dbt_call_return_trace,
                sizeof(attempt->aot_dbt_call_return_trace));
    attempt->aot_dbt_call_step_probe_configured =
        context.aot_dbt_call_step_probe_configured;
    attempt->aot_dbt_call_step_probe_target_count =
        context.aot_dbt_call_step_probe_target_count;
    std::memcpy(attempt->aot_dbt_call_step_probe_targets,
                context.aot_dbt_call_step_probe_targets,
                sizeof(attempt->aot_dbt_call_step_probe_targets));
    attempt->aot_dbt_call_step_probe_trace_count =
        context.aot_dbt_call_step_probe_trace_count;
    attempt->aot_dbt_call_step_probe_arm_count =
        context.aot_dbt_call_step_probe_arm_count;
    attempt->aot_dbt_call_step_probe_complete_count =
        context.aot_dbt_call_step_probe_complete_count;
    attempt->aot_dbt_call_step_probe_conflict_count =
        context.aot_dbt_call_step_probe_conflict_count;
    attempt->aot_dbt_call_step_probe_skipped_count =
        context.aot_dbt_call_step_probe_skipped_count;
    attempt->aot_dbt_call_step_probe_phase =
        context.aot_dbt_call_step_probe_phase;
    attempt->aot_dbt_call_step_probe_active_call_sequence =
        context.aot_dbt_call_step_probe_active_call_sequence;
    std::memcpy(attempt->aot_dbt_call_step_probe_trace,
                context.aot_dbt_call_step_probe_trace,
                sizeof(attempt->aot_dbt_call_step_probe_trace));
    attempt->diagnostic_poll_iteration_count =
        context.diagnostic_poll_iteration_count;
    attempt->diagnostic_progress_count =
        context.diagnostic_progress_count.load(std::memory_order_relaxed);
    attempt->diagnostic_quiet_iteration_count =
        context.diagnostic_quiet_iteration_count;
    attempt->exception_dispatch_entry_count =
        context.exception_dispatch_entry_count.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_exit_count =
        context.exception_dispatch_exit_count.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_last_eip =
        context.exception_dispatch_last_eip.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_malformed_count =
        context.exception_dispatch_malformed_count.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_last_bad_context =
        context.exception_dispatch_last_bad_context.load(
            std::memory_order_relaxed);
    attempt->exception_dispatch_last_bad_record =
        context.exception_dispatch_last_bad_record.load(
            std::memory_order_relaxed);
    attempt->selector_table_valid = context.selector_table.valid;
    attempt->selector_descriptor_count =
        static_cast<std::uint32_t>(
            context.selector_table.descriptors.size());
    attempt->linexe_environment_active = context.linexe_environment_active;
    attempt->linexe_gs_byte_load_count = context.linexe_gs_byte_load_count;
    attempt->linexe_first_gs_byte_offset = context.linexe_first_gs_byte_offset;
    attempt->linexe_first_gs_byte_value = context.linexe_first_gs_byte_value;
    const repiu::runtime::GuestDescriptor* client_descriptor =
        repiu::runtime::FindDescriptor(context.selector_table,
                                       kDos4gwClientDataSelector);
    if (client_descriptor != nullptr && client_descriptor->present)
    {
        attempt->linexe_client_descriptor_valid = true;
        attempt->linexe_client_descriptor_base = client_descriptor->base;
        attempt->linexe_client_descriptor_limit = client_descriptor->limit;
        if (client_descriptor->limit >= kDos4gwPrivateRootOffset + 3U &&
            static_cast<std::uint64_t>(client_descriptor->base) +
                    kDos4gwPrivateRootOffset + 4U <=
                static_cast<std::uint64_t>(context.runtime_base) +
                    context.runtime_size)
        {
            const auto* root = reinterpret_cast<const std::uint16_t*>(
                static_cast<std::uintptr_t>(client_descriptor->base +
                                            kDos4gwPrivateRootOffset));
            attempt->linexe_root_offset = root[0];
            attempt->linexe_root_selector = root[1];
        }
    }
    const repiu::runtime::GuestDescriptor* data_descriptor =
        repiu::runtime::FindDescriptor(context.selector_table,
                                       kDos4gwLinexeDataSelector);
    if (data_descriptor != nullptr && data_descriptor->present)
    {
        attempt->linexe_data_descriptor_valid = true;
        attempt->linexe_data_descriptor_base = data_descriptor->base;
        const auto* module = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(data_descriptor->base +
                                        kDos4gwLinexeLoaderOffset));
        attempt->linexe_module_name_offset = module[2];
        attempt->linexe_module_name_selector = module[3];
        attempt->linexe_direct_export_count = module[8];
        attempt->linexe_direct_export_table_offset = module[9];
        attempt->linexe_direct_export_table_selector = module[10];
        const auto* first_export = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(data_descriptor->base + module[9]));
        attempt->linexe_direct_first_export_name_offset = first_export[0];
        attempt->linexe_direct_first_export_name_selector = first_export[1];
        const char* name = reinterpret_cast<const char*>(
            static_cast<std::uintptr_t>(data_descriptor->base + module[2]));
        for (std::uint32_t index = 0; index < 64 && name[index] != '\0'; ++index)
        {
            attempt->linexe_direct_module_name.push_back(name[index]);
        }
    }
    attempt->linexe_scan_entry_count = context.linexe_scan_entry_count;
    attempt->linexe_module_candidate_count =
        context.linexe_module_candidate_count;
    attempt->linexe_module_match_count = context.linexe_module_match_count;
    attempt->linexe_name_pointer_valid_count =
        context.linexe_name_pointer_valid_count;
    attempt->linexe_name_byte_instruction_count =
        context.linexe_name_byte_instruction_count;
    attempt->linexe_data_gs_load_count = context.linexe_data_gs_load_count;
    attempt->linexe_module_selector_stack_value =
        context.linexe_module_selector_stack_value;
    attempt->linexe_module_offset_stack_value =
        context.linexe_module_offset_stack_value;
    attempt->linexe_export_offset_stack_value =
        context.linexe_export_offset_stack_value;
    attempt->linexe_export_selector_stack_value =
        context.linexe_export_selector_stack_value;
    attempt->linexe_export_jump_source_esp =
        context.linexe_export_jump_source_esp;
    attempt->linexe_export_jump_source_module_offset =
        context.linexe_export_jump_source_module_offset;
    attempt->linexe_export_jump_source_module_selector =
        context.linexe_export_jump_source_module_selector;
    attempt->linexe_export_jump_target_esp =
        context.linexe_export_jump_target_esp;
    attempt->linexe_export_jump_target_module_offset =
        context.linexe_export_jump_target_module_offset;
    attempt->linexe_export_jump_target_module_selector =
        context.linexe_export_jump_target_module_selector;
    attempt->linexe_export_name_compare_count =
        context.linexe_export_name_compare_count;
    attempt->linexe_export_name_compare_gs =
        context.linexe_export_name_compare_gs;
    attempt->linexe_export_name_compare_edi =
        context.linexe_export_name_compare_edi;
    attempt->linexe_export_name_compare_esi =
        context.linexe_export_name_compare_esi;
    attempt->linexe_export_name_actual_byte =
        context.linexe_export_name_actual_byte;
    attempt->linexe_export_name_expected_byte =
        context.linexe_export_name_expected_byte;
    attempt->linexe_export_name_stage_mask =
        context.linexe_export_name_stage_mask;
    attempt->linexe_export_entry_name_offset_value =
        context.linexe_export_entry_name_offset_value;
    attempt->linexe_export_entry_name_selector_value =
        context.linexe_export_entry_name_selector_value;
    attempt->linexe_export_result_store_destination =
        context.linexe_export_result_store_destination;
    attempt->linexe_export_result_store_value =
        context.linexe_export_result_store_value;
    attempt->linexe_export_result_store_count =
        context.linexe_export_result_store_count;
    attempt->linexe_export_value_load_selector =
        context.linexe_export_value_load_selector;
    attempt->linexe_export_value_load_offset =
        context.linexe_export_value_load_offset;
    attempt->linexe_export_value_load_value =
        context.linexe_export_value_load_value;
    attempt->linexe_root_selector_eax = context.linexe_root_selector_eax;
    attempt->linexe_root_read_gs = context.linexe_root_read_gs;
    attempt->linexe_shared_load_entry_count =
        context.linexe_shared_load_entry_count;
    attempt->linexe_shared_load_read_count =
        context.linexe_shared_load_read_count;
    attempt->linexe_shared_load_selector = context.linexe_shared_load_selector;
    attempt->linexe_shared_load_offset = context.linexe_shared_load_offset;
    attempt->linexe_shared_load_value = context.linexe_shared_load_value;
    attempt->linexe_root_offset_load_value =
        context.linexe_root_offset_load_value;
    attempt->linexe_root_selector_load_value =
        context.linexe_root_selector_load_value;
    attempt->linexe_root_offset_load_success =
        context.linexe_root_offset_load_success;
    attempt->linexe_root_selector_load_success =
        context.linexe_root_selector_load_success;
    attempt->linexe_export_match_count = context.linexe_export_match_count;
    attempt->linexe_export_entry_loop_count =
        context.linexe_export_entry_loop_count;
    attempt->linexe_export_compare_count = context.linexe_export_compare_count;
    attempt->linexe_export_compare_eax = context.linexe_export_compare_eax;
    attempt->linexe_export_compare_ecx = context.linexe_export_compare_ecx;
    attempt->linexe_export_compare_eflags =
        context.linexe_export_compare_eflags;
    attempt->linexe_export_count_load_edx =
        context.linexe_export_count_load_edx;
    attempt->linexe_export_count_load_gs =
        context.linexe_export_count_load_gs;
    attempt->linexe_scan_return_count = context.linexe_scan_return_count;
    attempt->linexe_indirect_far_call_count = context.linexe_indirect_far_call_count;
    attempt->linexe_indirect_far_call_source = context.linexe_indirect_far_call_source;
    attempt->linexe_indirect_far_call_pointer = context.linexe_indirect_far_call_pointer;
    attempt->linexe_indirect_far_call_offset = context.linexe_indirect_far_call_offset;
    attempt->linexe_indirect_far_call_selector = context.linexe_indirect_far_call_selector;
    attempt->linexe_indirect_far_call_known_export = context.linexe_indirect_far_call_known_export;
    attempt->timer_interrupt_chain_hle_count = context.timer_interrupt_chain_hle_count;
    attempt->timer_interrupt_chain_hle_source = context.timer_interrupt_chain_hle_source;
    attempt->timer_interrupt_chain_hle_pointer = context.timer_interrupt_chain_hle_pointer;
    attempt->timer_interrupt_chain_hle_offset = context.timer_interrupt_chain_hle_offset;
    attempt->timer_interrupt_chain_hle_selector = context.timer_interrupt_chain_hle_selector;
    attempt->linexe_bridge_entry_count = context.linexe_bridge_entry_count;
    attempt->linexe_bridge_gate_valid = context.linexe_bridge_gate_valid;
    attempt->linexe_bridge_selector = context.linexe_bridge_selector;
    attempt->linexe_bridge_offset = context.linexe_bridge_offset;
    attempt->linexe_bridge_service = context.linexe_bridge_service;
    attempt->linexe_bridge_esp = context.linexe_bridge_esp;
    attempt->linexe_bridge_ebp = context.linexe_bridge_ebp;
    std::memcpy(attempt->linexe_bridge_stack,
                context.linexe_bridge_stack,
                sizeof(attempt->linexe_bridge_stack));
    std::memcpy(attempt->linexe_bridge_argument_text,
                context.linexe_bridge_argument_text,
                sizeof(attempt->linexe_bridge_argument_text));
    std::memcpy(attempt->linexe_bridge_stack_text,
                context.linexe_bridge_stack_text,
                sizeof(attempt->linexe_bridge_stack_text));
    attempt->linexe_virtual_module_load_count =
        context.linexe_virtual_module_load_count;
    attempt->linexe_virtual_module_handle =
        context.linexe_virtual_module_handle;
    attempt->linexe_get_proc_count = context.linexe_get_proc_count;
    attempt->linexe_get_proc_result_pointer =
        context.linexe_get_proc_result_pointer;
    std::memcpy(attempt->linexe_get_proc_name,
                context.linexe_get_proc_name,
                sizeof(attempt->linexe_get_proc_name));
    attempt->glide_gate_entry_count = context.glide_gate_entry_count;
    attempt->glide_gate_handled_count = context.glide_gate_handled_count;
    attempt->glide_gate_esp = context.glide_gate_esp;
    std::memcpy(attempt->glide_gate_stack,
                context.glide_gate_stack,
                sizeof(attempt->glide_gate_stack));
    attempt->glide_gate_ordinal = context.glide_gate_ordinal;
    attempt->glide_gate_argument_bytes = context.glide_gate_argument_bytes;
    std::memcpy(attempt->glide_gate_name,
                context.glide_gate_name,
                sizeof(attempt->glide_gate_name));
    attempt->glide_implementation_issues =
        context.glide_implementation_issues;
    attempt->glide_texture_gate_trace_count = context.glide_texture_gate_trace_count;
    attempt->glide_texture_gate_trace_wrapped = context.glide_texture_gate_trace_wrapped;
    std::memcpy(attempt->glide_texture_gate_trace,
                context.glide_texture_gate_trace,
                sizeof(attempt->glide_texture_gate_trace));
    attempt->glide_first_triangle = context.glide_first_triangle;
    attempt->glide_triangle_trace_count = context.glide_triangle_trace_count;
    attempt->glide_triangle_trace_wrapped = context.glide_triangle_trace_wrapped;
    std::memcpy(attempt->glide_triangle_trace, context.glide_triangle_trace,
                sizeof(attempt->glide_triangle_trace));
    for (std::size_t ordinal = 0;
         ordinal < context.glide_call_counts.size(); ++ordinal)
    {
        if (context.glide_call_counts[ordinal] == 0U)
        {
            continue;
        }
        Win32MinimalExecutionAttempt::GlideCallObservation observation;
        observation.ordinal = static_cast<std::uint16_t>(ordinal);
        observation.count = context.glide_call_counts[ordinal];
        observation.name = context.glide_call_names[ordinal];
        std::copy(context.glide_first_stacks[ordinal].begin(),
                  context.glide_first_stacks[ordinal].end(),
                  observation.first_stack);
        attempt->glide_calls.push_back(std::move(observation));
    }
    for (std::size_t ordinal = 0;
         ordinal < attempt->glide_ordinal_timing.entries.size(); ++ordinal)
    {
        const Win32GlideOrdinalTimingEntry& timing =
            attempt->glide_ordinal_timing.entries[ordinal];
        if (timing.count == 0U)
        {
            continue;
        }
        Win32MinimalExecutionAttempt::GlideOrdinalTimingObservation
            observation;
        observation.ordinal = static_cast<std::uint16_t>(ordinal);
        observation.name = context.glide_call_names[ordinal];
        observation.timing = timing;
        attempt->glide_ordinal_timings.push_back(std::move(observation));
    }
    std::sort(
        attempt->glide_ordinal_timings.begin(),
        attempt->glide_ordinal_timings.end(),
        [](const auto& left, const auto& right) {
            if (left.timing.gate_cycles != right.timing.gate_cycles)
            {
                return left.timing.gate_cycles >
                    right.timing.gate_cycles;
            }
            return left.ordinal < right.ordinal;
        });
    // Read from the profile rather than the snapshot: the snapshot carries only
    // aggregates so a copy does not move the whole 256-entry array.
    for (std::size_t ordinal = 0;
         ordinal < context.glide_setter_census.entries.size(); ++ordinal)
    {
        const Win32GlideSetterCensusEntry& census =
            context.glide_setter_census.entries[ordinal];
        const Win32GlideSetterStateCacheEntry& cache =
            context.glide_setter_state_cache.entries[ordinal];
        if (census.call_count == 0U && census.key_overflow_count == 0U &&
            cache.elided_count == 0U && cache.applied_count == 0U)
        {
            continue;
        }
        Win32MinimalExecutionAttempt::GlideSetterCensusObservation observation;
        observation.ordinal = static_cast<std::uint16_t>(ordinal);
        observation.name = context.glide_call_names[ordinal];
        observation.census = census;
        observation.elided_count = cache.elided_count;
        observation.applied_count = cache.applied_count;
        attempt->glide_setter_censuses.push_back(std::move(observation));
    }
    // Ranked by call volume: the leading setters are the ones whose repetition
    // rate decides whether Task 365 is worth implementing.
    std::sort(
        attempt->glide_setter_censuses.begin(),
        attempt->glide_setter_censuses.end(),
        [](const auto& left, const auto& right) {
            if (left.census.call_count != right.census.call_count)
            {
                return left.census.call_count > right.census.call_count;
            }
            return left.ordinal < right.ordinal;
        });
    attempt->mscdex_available = context.mscdex_available;
    attempt->cd_audio_available = context.cd_audio_available;
    attempt->mscdex_track_count = static_cast<std::uint32_t>(
        context.cd_image.tracks().size());
    attempt->mscdex_request_count = context.mscdex_request_count;
    attempt->mscdex_frame_es = context.mscdex_frame_es;
    attempt->mscdex_decline_count = context.mscdex_decline_count;
    attempt->mscdex_last_decline_reason = context.mscdex_last_decline_reason;
    attempt->mscdex_last_resolve_kind = context.mscdex_last_resolve_kind;
    attempt->mscdex_last_header_bytes = context.mscdex_last_header_bytes;
    attempt->mscdex_last_ioctl_subfunction =
        context.mscdex_last_ioctl_subfunction;
    attempt->mscdex_last_ioctl_handled = context.mscdex_last_ioctl_handled;
    attempt->mscdex_last_ioctl_length = context.mscdex_last_ioctl_length;
    attempt->mscdex_ioctl_reject_mask = context.mscdex_ioctl_reject_mask;
    attempt->mscdex_last_play_mode = context.mscdex_last_play_mode;
    attempt->mscdex_last_play_start = context.mscdex_last_play_start;
    attempt->mscdex_last_play_length = context.mscdex_last_play_length;
    attempt->mscdex_last_seek_target = context.mscdex_last_seek_target;
    attempt->cd_audio_current_lba = context.cd_audio.current_lba();
    attempt->glide_window_open_count = context.glide_window_open_count;
    attempt->glide_logical_width = context.glide_logical_width;
    attempt->glide_logical_height = context.glide_logical_height;
    attempt->glide_backend_message = context.glide_backend_message;
    attempt->glide_texture_memory_bytes =
        context.glide_state.texture_memory_bytes;
    repiu::hle::CalculateGlideTextureMaxAddress(
        context.glide_state.texture_memory_bytes,
        &attempt->glide_texture_max_address);
    attempt->linexe_scan_return_eax = context.linexe_scan_return_eax;
    attempt->linexe_scan_return_ebp = context.linexe_scan_return_ebp;
    attempt->linexe_scan_caller_eax = context.linexe_scan_caller_eax;
    std::memcpy(attempt->linexe_selector_init_results,
                context.linexe_selector_init_results,
                sizeof(attempt->linexe_selector_init_results));
    attempt->dpmi_allocate_call_count = context.dpmi_allocate_call_count;
    attempt->dpmi_last_allocate_requested_count =
        context.dpmi_last_allocate_requested_count;
    attempt->dpmi_last_allocated_selector =
        context.dpmi_last_allocated_selector;
    constexpr std::uint32_t kSelectorWordsOffset = 0x000C68C0U;
    constexpr std::uint32_t kResolvedExportsOffset = 0x001A62C4U;
    constexpr std::uint32_t kSavedClientGsOffset = 0x001A6354U;
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context.runtime_base) +
        context.runtime_size;
    if (context.linexe_environment_active &&
        static_cast<std::uint64_t>(context.runtime_base) +
                kResolvedExportsOffset + 8U * 8U <= runtime_end)
    {
        const auto* saved_gs = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(context.runtime_base +
                                        kSavedClientGsOffset));
        attempt->linexe_saved_client_gs = *saved_gs;
        const auto* selector_words = reinterpret_cast<const std::uint16_t*>(
            static_cast<std::uintptr_t>(context.runtime_base +
                                        kSelectorWordsOffset));
        std::memcpy(attempt->linexe_selector_words,
                    selector_words,
                    sizeof(attempt->linexe_selector_words));
        for (std::uint32_t index = 0; index < 8; ++index)
        {
            const auto* value = reinterpret_cast<const std::uint32_t*>(
                static_cast<std::uintptr_t>(
                    context.runtime_base + kResolvedExportsOffset +
                    index * 8U));
            attempt->linexe_resolved_exports[index] = *value;
            if (*value != 0)
            {
                ++attempt->linexe_resolved_export_count;
            }
        }
    }
    attempt->dos_low_memory_valid = context.dos_low_memory.valid;
    attempt->dos_low_memory_size =
        repiu::runtime::kDosLowMemorySize;
    BuildSingleStepSnapshot(context, &attempt->last_single_step_snapshot);
    attempt->dos_environment_block_size =
        static_cast<std::uint32_t>(context.dos_environment_block.size());
    attempt->last_dos_environment_access_valid =
        context.last_dos_environment_access_valid;
    attempt->last_dos_environment_access_offset =
        context.last_dos_environment_access_offset;
    attempt->last_dos_environment_entry_offset =
        context.last_dos_environment_entry_offset;
    attempt->last_dos_environment_value_length =
        context.last_dos_environment_value_length;
    attempt->last_dos_environment_entry_name =
        context.last_dos_environment_entry_name;
    attempt->handled_hle_trap_count = context.handled_hle_trap_count;
    attempt->last_hle_trap_address = context.last_hle_trap_address;
    attempt->last_hle_trap_opcode = context.last_hle_trap_opcode;
    attempt->port_io = context.port_io;
    attempt->dos_path = context.dos_path;
    attempt->dos_file_io = context.dos_file_io;
    attempt->dos_file_io.read_count = context.dos_file_system.file_read_count;
    attempt->dos_file_io.host_open_count =
        context.dos_file_system.host_file_open_count;
    attempt->allocator_probe = context.allocator_probe;
    attempt->allocator_control_flow = context.allocator_control_flow;
    attempt->handled_dos_interrupt_count =
        context.handled_dos_interrupt_count;
    attempt->last_dos_interrupt_vector = context.last_dos_interrupt_vector;
    attempt->last_dos_interrupt_ah = context.last_dos_interrupt_ah;
    attempt->last_dos_interrupt_ax = context.last_dos_interrupt_ax;
    std::memcpy(attempt->handled_dos_interrupt_ah_counts, context.handled_dos_interrupt_ah_counts, sizeof(attempt->handled_dos_interrupt_ah_counts));
    attempt->handled_dos_chdir_count = context.handled_dos_chdir_count;
    attempt->last_dos_chdir_guest_path =
        context.last_dos_chdir_guest_path;
    attempt->last_dos_chdir_host_path = context.last_dos_chdir_host_path;
    attempt->last_dos_chdir_virtual_path =
        context.last_dos_chdir_virtual_path;
    attempt->last_dos_chdir_success = context.last_dos_chdir_success;
    attempt->last_dos_chdir_error = context.last_dos_chdir_error;
    attempt->handled_dos_getcwd_count = context.handled_dos_getcwd_count;
    attempt->last_dos_getcwd_drive = context.last_dos_getcwd_drive;
    attempt->last_dos_getcwd_path = context.last_dos_getcwd_path;
    attempt->last_dos_getcwd_success = context.last_dos_getcwd_success;
    attempt->last_dos_getcwd_error = context.last_dos_getcwd_error;
    attempt->handled_dos_getdrive_count =
        context.handled_dos_getdrive_count;
    attempt->last_dos_getdrive_value = context.last_dos_getdrive_value;
    attempt->handled_dos_open_count = context.handled_dos_open_count;
    attempt->last_dos_open_guest_path = context.last_dos_open_guest_path;
    attempt->last_dos_open_host_path = context.last_dos_open_host_path;
    attempt->last_dos_open_virtual_path =
        context.last_dos_open_virtual_path;
    attempt->last_dos_open_success = context.last_dos_open_success;
    attempt->last_dos_open_error = context.last_dos_open_error;
    attempt->last_dos_open_handle = context.last_dos_open_handle;
    attempt->last_dos_open_access_mode = context.last_dos_open_access_mode;
    attempt->handled_dos_read_count = context.handled_dos_read_count;
    attempt->last_dos_read_handle = context.last_dos_read_handle;
    attempt->last_dos_read_requested_bytes =
        context.last_dos_read_requested_bytes;
    attempt->last_dos_read_actual_bytes =
        context.last_dos_read_actual_bytes;
    attempt->last_dos_read_buffer = context.last_dos_read_buffer;
    attempt->last_dos_read_success = context.last_dos_read_success;
    attempt->last_dos_read_error = context.last_dos_read_error;
    attempt->handled_dos_seek_count = context.handled_dos_seek_count;
    attempt->last_dos_seek_handle = context.last_dos_seek_handle;
    attempt->last_dos_seek_origin = context.last_dos_seek_origin;
    attempt->last_dos_seek_offset = context.last_dos_seek_offset;
    attempt->last_dos_seek_position = context.last_dos_seek_position;
    attempt->last_dos_seek_success = context.last_dos_seek_success;
    attempt->last_dos_seek_error = context.last_dos_seek_error;
    attempt->handled_dos_close_count = context.handled_dos_close_count;
    attempt->last_dos_close_handle = context.last_dos_close_handle;
    attempt->last_dos_close_success = context.last_dos_close_success;
    attempt->last_dos_close_error = context.last_dos_close_error;
    attempt->handled_dos_ioctl_count = context.handled_dos_ioctl_count;
    attempt->last_dos_ioctl_subfunction =
        context.last_dos_ioctl_subfunction;
    attempt->last_dos_ioctl_handle = context.last_dos_ioctl_handle;
    attempt->last_dos_ioctl_success = context.last_dos_ioctl_success;
    attempt->last_dos_ioctl_error = context.last_dos_ioctl_error;
    attempt->last_dos_ioctl_device_info =
        context.last_dos_ioctl_device_info;
    attempt->handled_dos_resize_count = context.handled_dos_resize_count;
    attempt->last_dos_resize_selector = context.last_dos_resize_selector;
    attempt->last_dos_resize_paragraphs =
        context.last_dos_resize_paragraphs;
    attempt->last_dos_resize_success = context.last_dos_resize_success;
    attempt->last_dos_resize_error = context.last_dos_resize_error;
    attempt->last_dos_resize_requested_end =
        context.last_dos_resize_requested_end;
    attempt->last_dos_resize_allocator_end =
        context.last_dos_resize_allocator_end;
    attempt->handled_segment_load_count =
        context.handled_segment_load_count;
    attempt->last_segment_load_address = context.last_segment_load_address;
    attempt->last_segment_load_opcode = context.last_segment_load_opcode;
    attempt->last_segment_load_register =
        context.last_segment_load_register;
    attempt->last_segment_load_selector =
        context.last_segment_load_selector;
    attempt->last_segment_load_source = context.last_segment_load_source;
    std::memcpy(attempt->handled_segment_load_register_counts, context.handled_segment_load_register_counts, sizeof(attempt->handled_segment_load_register_counts));
    attempt->segment_load = context.segment_load;
    attempt->handled_segment_store_count =
        context.handled_segment_store_count;
    attempt->last_segment_store_address = context.last_segment_store_address;
    attempt->last_segment_store_opcode = context.last_segment_store_opcode;
    attempt->last_segment_store_register =
        context.last_segment_store_register;
    attempt->last_segment_store_selector =
        context.last_segment_store_selector;
    attempt->last_segment_store_destination =
        context.last_segment_store_destination;
    std::memcpy(attempt->handled_segment_store_register_counts, context.handled_segment_store_register_counts, sizeof(attempt->handled_segment_store_register_counts));
    attempt->handled_segment_memory_load_count =
        context.handled_segment_memory_load_count;
    attempt->last_segment_memory_load_address =
        context.last_segment_memory_load_address;
    attempt->last_segment_memory_load_opcode =
        context.last_segment_memory_load_opcode;
    attempt->last_segment_memory_load_register =
        context.last_segment_memory_load_register;
    attempt->last_segment_memory_load_selector =
        context.last_segment_memory_load_selector;
    attempt->last_segment_memory_load_offset =
        context.last_segment_memory_load_offset;
    attempt->last_segment_memory_load_width =
        context.last_segment_memory_load_width;
    attempt->last_segment_memory_load_value =
        context.last_segment_memory_load_value;
    attempt->handled_low_memory_access_count =
        context.handled_low_memory_access_count;
    attempt->last_low_memory_access_address =
        context.last_low_memory_access_address;
    attempt->last_low_memory_access_opcode =
        context.last_low_memory_access_opcode;
    attempt->last_low_memory_access_esi =
        context.last_low_memory_access_esi;
    attempt->last_low_memory_access_edi =
        context.last_low_memory_access_edi;
    attempt->last_low_memory_access_destination =
        context.last_low_memory_access_destination;
    attempt->last_low_memory_access_value =
        context.last_low_memory_access_value;
    attempt->low_memory_read_emulate_count =
        context.low_memory_read_emulate_count;
    attempt->last_low_memory_read_emulate_address =
        context.last_low_memory_read_emulate_address;
    attempt->last_low_memory_read_emulate_eip =
        context.last_low_memory_read_emulate_eip;
    attempt->last_low_memory_read_emulate_value =
        context.last_low_memory_read_emulate_value;
    attempt->last_low_memory_read_emulate_reg =
        context.last_low_memory_read_emulate_reg;
    attempt->debug_emulate_stage =
        context.debug_emulate_stage;
    attempt->debug_emulate_decode_result =
        context.debug_emulate_decode_result;
    attempt->debug_emulate_calculated_address =
        context.debug_emulate_calculated_address;
    attempt->rep_movs_copy_failure_count =
        context.rep_movs_copy_failure_count;
    attempt->last_rep_movs_copy_failure_stage =
        context.last_rep_movs_copy_failure_stage;
    attempt->last_rep_movs_copy_error =
        context.last_rep_movs_copy_error;
    attempt->last_rep_movs_copy_source =
        context.last_rep_movs_copy_source;
    attempt->last_rep_movs_copy_destination =
        context.last_rep_movs_copy_destination;
    attempt->last_rep_movs_copy_bytes =
        context.last_rep_movs_copy_bytes;
    attempt->handled_memory_store_count = context.handled_memory_store_count;
    attempt->last_memory_store_address = context.last_memory_store_address;
    attempt->last_memory_store_opcode = context.last_memory_store_opcode;
    attempt->last_memory_store_destination =
        context.last_memory_store_destination;
    attempt->last_memory_store_value = context.last_memory_store_value;
    attempt->last_memory_store_width = context.last_memory_store_width;
    attempt->last_memory_store_source_kind =
        context.last_memory_store_source_kind;
    attempt->last_memory_store_applied = context.last_memory_store_applied;
    attempt->shadow_memory_write_count = context.shadow_memory_write_count;
    attempt->shadow_memory_read_hit_count =
        context.shadow_memory_read_hit_count;
    attempt->shadow_memory_byte_count =
        static_cast<std::uint32_t>(context.shadow_memory.size());
    attempt->shadow_memory_range_valid = context.shadow_memory_range_valid;
    attempt->shadow_memory_min_address = context.shadow_memory_min_address;
    attempt->shadow_memory_max_address = context.shadow_memory_max_address;
    attempt->handled_fatal_breakpoint_count =
        context.handled_fatal_breakpoint_count;
    attempt->last_fatal_breakpoint_address =
        context.last_fatal_breakpoint_address;
    attempt->last_fatal_message_address =
        context.last_fatal_message_address;
    attempt->last_fatal_message = context.last_fatal_message;
    attempt->fatal_halt_reached = context.fatal_halt_reached;
}

} // namespace repiu::platform::win32
