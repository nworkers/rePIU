#include "repiu/engine/live_execution_profile_report.h"

#include "repiu/platform/host_error_stream.h"
#include "repiu/platform/host_time.h"

#include <cstdio>
#include <cstdlib>

namespace repiu::engine
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
    const ExecutionTimeProfile* profile,
    const std::uint64_t frames,
    const LiveAotCounters& aot,
    const LiveDosCounters& dos)
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

    const ExecutionTimeProfileSnapshot snapshot =
        SnapshotExecutionTimeProfile(*profile);
    const ExecutionTimeShares shares =
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
    // Task 515: all three classes, not just the single-step one.
    //
    // 512 printed single-step alone and used it to rule that class out of the
    // 13.6x excess -- 3.6% of Linux's deliveries against 24.4% of Windows'. What
    // the other 96.4% is decides the next step completely: breakpoints point at
    // the policy that plants them, and `other` at access violations, which is
    // page protection, port I/O and write watches. The counters for both were
    // already filling.
    const auto gap_count = [&snapshot](VehGapClass gap_class) {
        return snapshot.veh_gap_counts[static_cast<std::uint32_t>(gap_class)];
    };
    const auto gap_mean = [&snapshot](VehGapClass gap_class) -> std::uint64_t {
        const std::uint32_t index = static_cast<std::uint32_t>(gap_class);
        const std::uint32_t count = snapshot.veh_gap_counts[index];
        return count == 0U ? 0U : snapshot.veh_gap_cycles[index] / count;
    };
    const std::uint32_t gap_single_step_count =
        gap_count(VehGapClass::kSingleStep);
    char veh_line[448] = {};
    const int veh_length = std::snprintf(
        veh_line, sizeof(veh_line),
        "[repiu-live-veh] #%u veh_count=%llu per_frame=%llu "
        "cycles_per_veh=%llu gap_min=%llu "
        "ss_count=%llu ss_mean=%llu bp_count=%llu bp_mean=%llu "
        "other_count=%llu other_mean=%llu\n",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned long long>(veh_count),
        static_cast<unsigned long long>(frames == 0U ? 0U : veh_count / frames),
        static_cast<unsigned long long>(
            veh_count == 0U ? 0U : shares.veh / veh_count),
        static_cast<unsigned long long>(snapshot.veh_gap_min_cycles),
        static_cast<unsigned long long>(gap_single_step_count),
        static_cast<unsigned long long>(gap_mean(VehGapClass::kSingleStep)),
        static_cast<unsigned long long>(gap_count(VehGapClass::kBreakpoint)),
        static_cast<unsigned long long>(gap_mean(VehGapClass::kBreakpoint)),
        static_cast<unsigned long long>(gap_count(VehGapClass::kOther)),
        static_cast<unsigned long long>(gap_mean(VehGapClass::kOther)));
    if (veh_length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            veh_line,
            static_cast<std::size_t>(veh_length) < sizeof(veh_line)
                ? static_cast<std::size_t>(veh_length)
                : sizeof(veh_line) - 1U);
    }

    // Task 516: a third line, for what the code cache did between boundaries.
    //
    // 515 named the excess as breakpoints -- the INT3s the engine plants at
    // boundaries -- so the question became why this cache reaches a boundary so
    // much more often. These counters answer it: `residency` is how far the
    // cache runs from an entry before its first control transfer, and the five
    // reasons say which guest instruction let go.
    //
    // The five sum to `boundary`, and that identity is printed as `sum_ok` so a
    // reader can see the split holds rather than assuming it.
    const std::uint32_t reason_sum =
        aot.boundary_return + aot.boundary_indirect + aot.boundary_direct +
        aot.boundary_conditional + aot.boundary_other;
    char aot_line[448] = {};
    const int aot_length = std::snprintf(
        aot_line, sizeof(aot_line),
        "[repiu-live-aot] #%u entry=%llu boundary=%llu per_frame=%llu "
        "residency_mean=%llu ret=%llu ind=%llu dir=%llu cond=%llu oth=%llu "
        "reentry=%llu fallback=%llu retired=%llu quarantine=%llu "
        "translate=%llu sum_ok=%d\n",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned long long>(aot.cache_entry),
        static_cast<unsigned long long>(aot.boundary),
        static_cast<unsigned long long>(
            frames == 0U ? 0U : aot.boundary / frames),
        static_cast<unsigned long long>(
            aot.residency_samples == 0U
                ? 0U
                : aot.residency_instructions / aot.residency_samples),
        static_cast<unsigned long long>(aot.boundary_return),
        static_cast<unsigned long long>(aot.boundary_indirect),
        static_cast<unsigned long long>(aot.boundary_direct),
        static_cast<unsigned long long>(aot.boundary_conditional),
        static_cast<unsigned long long>(aot.boundary_other),
        static_cast<unsigned long long>(aot.reentry),
        static_cast<unsigned long long>(aot.legacy_fallback),
        static_cast<unsigned long long>(aot.retired_entry_trap),
        static_cast<unsigned long long>(aot.quarantine),
        static_cast<unsigned long long>(aot.dynamic_attempt),
        reason_sum == aot.boundary ? 1 : 0);
    if (aot_length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            aot_line,
            static_cast<std::size_t>(aot_length) < sizeof(aot_line)
                ? static_cast<std::size_t>(aot_length)
                : sizeof(aot_line) - 1U);
    }

    // Task 517: whether the trap-free reentry path is running at all.
    //
    // Printed as its own line for the same reason the others are: this answers
    // a different question from the ones above it, and a reader or a script
    // should not have to split a long line to get at it.
    //
    // `per_reentry` is the number that matters. Windows resolves 95.1% of its
    // reentries here; a value near zero says the reentries are going the other
    // way, through a trap.
    char glide_line[512] = {};
    const int glide_length = std::snprintf(
        glide_line, sizeof(glide_line),
        "[repiu-live-gdd] #%u patched=%llu verified=%llu resolved=%llu "
        "relinked=%llu content=%llu fixup=%llu entry=%llu "
        "success=%llu miss=%llu terminal=%llu per_reentry=%.4f "
        "at_target=%llu elsewhere=%llu other=%llu step_after=%llu clean=%llu\n",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned long long>(aot.glide_patched_sites),
        static_cast<unsigned long long>(aot.glide_verified_sites),
        static_cast<unsigned long long>(aot.glide_resolved_target),
        static_cast<unsigned long long>(aot.glide_relinked_cache_target),
        static_cast<unsigned long long>(aot.glide_relink_content),
        static_cast<unsigned long long>(aot.glide_relink_fixup),
        static_cast<unsigned long long>(aot.glide_entry),
        static_cast<unsigned long long>(aot.glide_success),
        static_cast<unsigned long long>(aot.glide_target_miss),
        static_cast<unsigned long long>(aot.glide_terminal_failure),
        aot.reentry == 0U
            ? 0.0
            : static_cast<double>(aot.glide_entry) /
                  static_cast<double>(aot.reentry),
        static_cast<unsigned long long>(aot.direct_trap_at_target),
        static_cast<unsigned long long>(aot.direct_trap_elsewhere),
        static_cast<unsigned long long>(aot.direct_other_elsewhere),
        static_cast<unsigned long long>(aot.direct_step_after),
        static_cast<unsigned long long>(aot.direct_clean));
    if (glide_length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            glide_line,
            static_cast<std::size_t>(glide_length) < sizeof(glide_line)
                ? static_cast<std::size_t>(glide_length)
                : sizeof(glide_line) - 1U);
    }

    // Task 526: the sites those follow-on breakpoints land on. Its own line
    // because a histogram does not fit the shape of the one above it.
    char site_line[384] = {};
    int site_length = std::snprintf(
        site_line, sizeof(site_line), "[repiu-live-site] #%u",
        static_cast<unsigned>(g_report_index));
    for (std::size_t slot = 0; slot < 8U && site_length > 0; ++slot)
    {
        if (aot.direct_trap_site_count[slot] == 0U)
        {
            break;
        }
        const int written = std::snprintf(
            site_line + site_length,
            sizeof(site_line) - static_cast<std::size_t>(site_length),
            " %08X=%u",
            static_cast<unsigned>(aot.direct_trap_site_address[slot]),
            static_cast<unsigned>(aot.direct_trap_site_count[slot]));
        if (written <= 0)
        {
            break;
        }
        site_length += written;
    }
    if (site_length > 0 &&
        static_cast<std::size_t>(site_length) < sizeof(site_line) - 1U)
    {
        site_line[site_length] = '\n';
        repiu::platform::WriteHostErrorStream(
            site_line, static_cast<std::size_t>(site_length) + 1U);
    }

    // Task 528: the instructions the boundary samples land on.
    //
    // Its own line because it answers "which instruction" rather than "how
    // many", and because the loader summary that used to carry this census is
    // never reached by a Linux run.
    char opcode_line[384] = {};
    int opcode_length = std::snprintf(
        opcode_line, sizeof(opcode_line),
        "[repiu-live-opcode] #%u samples=%llu",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned long long>(aot.opcode_samples));
    for (std::size_t slot = 0; slot < 4U && opcode_length > 0; ++slot)
    {
        if (aot.top_opcode_count[slot] == 0U)
        {
            break;
        }
        const int written = std::snprintf(
            opcode_line + opcode_length,
            sizeof(opcode_line) - static_cast<std::size_t>(opcode_length),
            " %02X=%u",
            static_cast<unsigned>(aot.top_opcode[slot]),
            static_cast<unsigned>(aot.top_opcode_count[slot]));
        if (written <= 0)
        {
            break;
        }
        opcode_length += written;
    }
    for (std::size_t slot = 0; slot < 4U && opcode_length > 0; ++slot)
    {
        if (aot.top_escape_count[slot] == 0U)
        {
            break;
        }
        const int written = std::snprintf(
            opcode_line + opcode_length,
            sizeof(opcode_line) - static_cast<std::size_t>(opcode_length),
            " 0F%02X=%u",
            static_cast<unsigned>(aot.top_escape[slot]),
            static_cast<unsigned>(aot.top_escape_count[slot]));
        if (written <= 0)
        {
            break;
        }
        opcode_length += written;
    }
    if (opcode_length > 0 &&
        static_cast<std::size_t>(opcode_length) < sizeof(opcode_line) - 1U)
    {
        opcode_line[opcode_length] = '\n';
        repiu::platform::WriteHostErrorStream(
            opcode_line, static_cast<std::size_t>(opcode_length) + 1U);
    }

    // Task 529: the ModRM group behind each effective FF boundary sample.
    char ff_line[384] = {};
    int ff_length = std::snprintf(
        ff_line, sizeof(ff_line),
        "[repiu-live-ff] #%u truncated=%u",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned>(aot.ff_modrm_truncated_count));
    for (std::size_t group = 0; group < 8U && ff_length > 0; ++group)
    {
        if (aot.ff_group_counts[group] == 0U)
        {
            continue;
        }
        const int written = std::snprintf(
            ff_line + ff_length,
            sizeof(ff_line) - static_cast<std::size_t>(ff_length),
            " /%u=%u",
            static_cast<unsigned>(group),
            static_cast<unsigned>(aot.ff_group_counts[group]));
        if (written <= 0)
        {
            break;
        }
        ff_length += written;
    }
    if (ff_length > 0 &&
        static_cast<std::size_t>(ff_length) < sizeof(ff_line) - 1U)
    {
        ff_line[ff_length] = '\n';
        repiu::platform::WriteHostErrorStream(
            ff_line, static_cast<std::size_t>(ff_length) + 1U);
    }

    // Task 530: identify which guest sites produce the FF /4 population.
    char ff_site_line[2048] = {};
    int ff_site_length = std::snprintf(
        ff_site_line, sizeof(ff_site_line),
        "[repiu-live-ff-site] #%u samples=%u truncated=%u overflow=%u "
        "resolved=%u unresolved=%u target_truncated=%u "
        "target_unsupported=%u target_unreadable=%u",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned>(aot.ff4_sample_count),
        static_cast<unsigned>(aot.ff4_modrm_truncated_count),
        static_cast<unsigned>(aot.ff4_site_overflow_count),
        static_cast<unsigned>(aot.ff4_target_resolved_count),
        static_cast<unsigned>(aot.ff4_target_unresolved_count),
        static_cast<unsigned>(aot.ff4_target_instruction_truncated_count),
        static_cast<unsigned>(aot.ff4_target_unsupported_count),
        static_cast<unsigned>(aot.ff4_target_memory_unreadable_count));
    if (ff_site_length > 0 &&
        static_cast<std::size_t>(ff_site_length) < sizeof(ff_site_line))
    {
        for (std::size_t mode = 0U;
             mode < kAotFfAddressingModeCount && ff_site_length > 0;
             ++mode)
        {
            const std::uint32_t count =
                aot.ff4_addressing_mode_counts[mode];
            if (count == 0U)
            {
                continue;
            }
            const int written = std::snprintf(
                ff_site_line + ff_site_length,
                sizeof(ff_site_line) - static_cast<std::size_t>(ff_site_length),
                " %s=%u",
                AotFfAddressingModeName(
                    static_cast<AotFfAddressingMode>(mode)),
                static_cast<unsigned>(count));
            if (written <= 0 ||
                static_cast<std::size_t>(written) >=
                    sizeof(ff_site_line) -
                        static_cast<std::size_t>(ff_site_length))
            {
                ff_site_length = -1;
                break;
            }
            ff_site_length += written;
        }
    }
    for (std::size_t slot = 0U;
         slot < kAotFfBoundarySiteHotspotCapacity && ff_site_length > 0;
         ++slot)
    {
        const AotFfBoundarySiteHotspot& site =
            aot.ff4_site_hotspots[slot];
        if (site.count == 0U)
        {
            continue;
        }
        const int written = std::snprintf(
            ff_site_line + ff_site_length,
            sizeof(ff_site_line) - static_cast<std::size_t>(ff_site_length),
            " site=0x%08X:%u:%s:0x%08X:c%u:m%u:d=0x%08X:p=0x%08X:"
            "t=0x%08X:r%u:f%u:pv%u:pc%u:dc%u:tc%u:spc%u:ppc%u:"
            "ir%u:iv=0x%08X:ivv%u:ic%u:br%u:bv=0x%08X:bvv%u:bc%u:"
            "ix%u:is%u:io%u:tx%u:ts%u:to%u",
            static_cast<unsigned>(site.guest_eip),
            static_cast<unsigned>(site.count),
            AotFfAddressingModeName(site.addressing_mode),
            static_cast<unsigned>(site.last_packed_bytes),
            static_cast<unsigned>(site.byte_change_count),
            static_cast<unsigned>(site.mode_change_count),
            static_cast<unsigned>(site.last_displacement),
            static_cast<unsigned>(site.last_pointer_address),
            static_cast<unsigned>(site.last_target),
            static_cast<unsigned>(site.target_read_count),
            static_cast<unsigned>(site.target_failure_count),
            site.last_pointer_address_valid ? 1U : 0U,
            static_cast<unsigned>(site.pointer_change_count),
            static_cast<unsigned>(site.displacement_change_count),
            static_cast<unsigned>(site.target_change_count),
            static_cast<unsigned>(site.target_change_with_same_pointer_count),
            static_cast<unsigned>(
                site.target_change_with_pointer_change_count),
            static_cast<unsigned>(site.last_index_register),
            static_cast<unsigned>(site.last_index_value),
            site.last_index_value_valid ? 1U : 0U,
            static_cast<unsigned>(site.index_value_change_count),
            static_cast<unsigned>(site.last_base_register),
            static_cast<unsigned>(site.last_base_value),
            site.last_base_value_valid ? 1U : 0U,
            static_cast<unsigned>(site.base_value_change_count),
            static_cast<unsigned>(site.index_value_observation_sample_count),
            static_cast<unsigned>(site.index_value_observation_slot_count),
            static_cast<unsigned>(site.index_value_observation_overflow_count),
            static_cast<unsigned>(site.index_transition_count),
            static_cast<unsigned>(site.index_transition_slot_count),
            static_cast<unsigned>(site.index_transition_overflow_count));
        if (written <= 0 ||
            static_cast<std::size_t>(written) >=
                sizeof(ff_site_line) - static_cast<std::size_t>(ff_site_length))
        {
            ff_site_length = -1;
            break;
        }
        ff_site_length += written;
        for (std::size_t index_slot = 0U;
             index_slot < kAotFfBoundaryIndexObservationCapacity &&
             ff_site_length > 0;
             ++index_slot)
        {
            const AotFfBoundaryIndexObservation& observation =
                site.index_value_observations[index_slot];
            if (!observation.valid)
            {
                continue;
            }
            const int observation_written = std::snprintf(
                ff_site_line + ff_site_length,
                sizeof(ff_site_line) - static_cast<std::size_t>(ff_site_length),
                " i%u=%u/0x%08X/%u",
                static_cast<unsigned>(index_slot),
                static_cast<unsigned>(observation.register_number),
                static_cast<unsigned>(observation.value),
                static_cast<unsigned>(observation.count));
            if (observation_written <= 0 ||
                static_cast<std::size_t>(observation_written) >=
                    sizeof(ff_site_line) -
                        static_cast<std::size_t>(ff_site_length))
            {
                ff_site_length = -1;
                break;
            }
            ff_site_length += observation_written;
        }
        for (std::size_t transition_slot = 0U;
             transition_slot < kAotFfBoundaryIndexTransitionCapacity &&
             ff_site_length > 0;
             ++transition_slot)
        {
            const AotFfBoundaryIndexTransition& transition =
                site.index_transitions[transition_slot];
            if (!transition.valid)
            {
                continue;
            }
            const int transition_written = std::snprintf(
                ff_site_line + ff_site_length,
                sizeof(ff_site_line) - static_cast<std::size_t>(ff_site_length),
                " tr%u=%u/0x%08X>%u/0x%08X",
                static_cast<unsigned>(transition_slot),
                static_cast<unsigned>(transition.from_register),
                static_cast<unsigned>(transition.from_value),
                static_cast<unsigned>(transition.to_register),
                static_cast<unsigned>(transition.to_value));
            if (transition_written <= 0 ||
                static_cast<std::size_t>(transition_written) >=
                    sizeof(ff_site_line) -
                        static_cast<std::size_t>(ff_site_length))
            {
                ff_site_length = -1;
                break;
            }
            ff_site_length += transition_written;
        }
    }
    if (ff_site_length > 0 &&
        static_cast<std::size_t>(ff_site_length) < sizeof(ff_site_line) - 1U)
    {
        ff_site_line[ff_site_length] = '\n';
        repiu::platform::WriteHostErrorStream(
            ff_site_line, static_cast<std::size_t>(ff_site_length) + 1U);
    }

    // Task 537: the resolved FF4 target interval has a separate boundary from
    // the site and transition attribution above.
    char ff_target_line[2048] = {};
    int ff_target_length = std::snprintf(
        ff_target_line, sizeof(ff_target_line),
        "[repiu-live-ff-target] #%u started=%u completed=%u active=%u "
        "candidate=%u mismatch=%u discarded=%u overflow=%u",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned>(
            aot.ff4_target_timing.interval_started_count),
        static_cast<unsigned>(
            aot.ff4_target_timing.interval_completed_count),
        aot.ff4_target_timing.interval_active ? 1U : 0U,
        aot.ff4_target_timing.candidate_valid ? 1U : 0U,
        static_cast<unsigned>(
            aot.ff4_target_timing.candidate_mismatch_count),
        static_cast<unsigned>(
            aot.ff4_target_timing.discarded_interval_count),
        static_cast<unsigned>(aot.ff4_target_timing.entry_overflow_count));
    for (std::size_t slot = 0U;
         slot < kAotFfTargetTimingEntryCapacity && ff_target_length > 0;
         ++slot)
    {
        const AotFfTargetTimingEntry& entry =
            aot.ff4_target_timing.entries[slot];
        if (!entry.valid)
        {
            continue;
        }
        const int written = std::snprintf(
            ff_target_line + ff_target_length,
            sizeof(ff_target_line) -
                static_cast<std::size_t>(ff_target_length),
            " e%u=s0x%08X:t0x%08X:c0x%08X:i%u/%u/0x%08X:n%u:sum%llu:"
            "min%llu:max%llu",
            static_cast<unsigned>(slot),
            static_cast<unsigned>(entry.source_guest_eip),
            static_cast<unsigned>(entry.target_guest_eip),
            static_cast<unsigned>(entry.cache_target),
            entry.index_value_valid ? 1U : 0U,
            static_cast<unsigned>(entry.index_register),
            static_cast<unsigned>(entry.index_value),
            static_cast<unsigned>(entry.interval_count),
            static_cast<unsigned long long>(entry.total_cycles),
            static_cast<unsigned long long>(entry.min_cycles),
            static_cast<unsigned long long>(entry.max_cycles));
        if (written <= 0 ||
            static_cast<std::size_t>(written) >=
                sizeof(ff_target_line) -
                    static_cast<std::size_t>(ff_target_length))
        {
            ff_target_length = -1;
            break;
        }
        ff_target_length += written;
    }
    if (ff_target_length > 0 &&
        static_cast<std::size_t>(ff_target_length) <
            sizeof(ff_target_line) - 1U)
    {
        ff_target_line[ff_target_length] = '\n';
        repiu::platform::WriteHostErrorStream(
            ff_target_line,
            static_cast<std::size_t>(ff_target_length) + 1U);
    }

    // Task 528: the INT 21h AH distribution without per-call trace output.
    char dos_line[192] = {};
    int dos_length = std::snprintf(
        dos_line, sizeof(dos_line),
        "[repiu-live-dos] #%u handled=%u int21=%u",
        static_cast<unsigned>(g_report_index),
        static_cast<unsigned>(dos.handled_interrupt_count),
        static_cast<unsigned>(dos.int21_count));
    for (std::size_t slot = 0; slot < 4U && dos_length > 0; ++slot)
    {
        if (dos.top_int21_ah_count[slot] == 0U)
        {
            break;
        }
        const int written = std::snprintf(
            dos_line + dos_length,
            sizeof(dos_line) - static_cast<std::size_t>(dos_length),
            " AH%02X=%u",
            static_cast<unsigned>(dos.top_int21_ah[slot]),
            static_cast<unsigned>(dos.top_int21_ah_count[slot]));
        if (written <= 0)
        {
            break;
        }
        dos_length += written;
    }
    if (dos_length > 0 &&
        static_cast<std::size_t>(dos_length) < sizeof(dos_line) - 1U)
    {
        dos_line[dos_length] = '\n';
        repiu::platform::WriteHostErrorStream(
            dos_line, static_cast<std::size_t>(dos_length) + 1U);
    }
    g_last_report_ticks = now;
    g_last_total = shares.total;
    g_last_frames = frames;
}

}  // namespace repiu::engine
