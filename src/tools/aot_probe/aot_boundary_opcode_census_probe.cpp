#include "aot_boundary_opcode_census_probe.h"

#include "repiu/engine/aot_boundary_opcode_census.h"
#include "repiu/engine/aot_ff_boundary_attribution.h"
#include "repiu/engine/aot_ff_boundary_target_attribution.h"

#include "../../engine/execution/thread_context.h"

#include <cstring>
#include <iostream>
#include <memory>

namespace repiu::tools
{
namespace
{

using engine::kAotMaxLegacyPrefixes;
using engine::RankAotOpcodeHistogram;
using engine::RecordAotBoundaryOpcodeSample;
using engine::AotBoundaryOpcodeCensus;
using engine::AotOpcodeRank;
using engine::AotFfAddressingMode;
using engine::AotFfBoundaryAttribution;
using engine::AotFfBoundaryDecoded;
using engine::AotFfBoundarySite;
using engine::AotFfBoundarySiteHotspot;
using engine::kAotFfBoundarySiteCapacity;
using engine::DecodeAotFfBoundary;
using engine::RankAotFfBoundarySites;
using engine::RecordAotFfBoundarySample;
using engine::AotFfBoundaryTargetDecoded;
using engine::AotFfBoundaryTargetResult;
using engine::AotFfTargetStatus;
using engine::DecodeAotFfBoundaryTarget;
using engine::RecordAotFfBoundaryTargetSample;
using engine::ResolveAotFfBoundaryTarget;
using engine::BeginAotFfTargetTimingIfMatched;
using engine::CompleteAotFfTargetTiming;
using engine::SetAotFfTargetTimingCandidate;
using engine::AotFfTargetTimingEntry;

using CensusPtr = std::unique_ptr<AotBoundaryOpcodeCensus>;

CensusPtr MakeCensus()
{
    return std::make_unique<AotBoundaryOpcodeCensus>();
}

template <std::size_t N>
void Record(AotBoundaryOpcodeCensus* census,
            const std::uint8_t (&bytes)[N])
{
    RecordAotBoundaryOpcodeSample(census, bytes, N);
}

std::uint32_t HistogramTotal(const std::uint32_t* counts)
{
    std::uint32_t total = 0;
    for (std::size_t i = 0; i < engine::kAotOpcodeHistogramSize;
         ++i)
    {
        total += counts[i];
    }
    return total;
}

std::uint32_t FfGroupTotal(const std::uint32_t* counts)
{
    std::uint32_t total = 0;
    for (std::size_t i = 0; i < 8U; ++i)
    {
        total += counts[i];
    }
    return total;
}

}  // namespace

bool RunAotBoundaryOpcodeCensusProbe()
{
    using engine::IsX86LegacyPrefix;

    const bool prefix_set =
        IsX86LegacyPrefix(0x26U) && IsX86LegacyPrefix(0x2EU) &&
        IsX86LegacyPrefix(0x36U) && IsX86LegacyPrefix(0x3EU) &&
        IsX86LegacyPrefix(0x64U) && IsX86LegacyPrefix(0x65U) &&
        IsX86LegacyPrefix(0x66U) && IsX86LegacyPrefix(0x67U) &&
        IsX86LegacyPrefix(0xF0U) && IsX86LegacyPrefix(0xF2U) &&
        IsX86LegacyPrefix(0xF3U) &&
        // Opcodes that must never be mistaken for prefixes.
        !IsX86LegacyPrefix(0x0FU) && !IsX86LegacyPrefix(0x8CU) &&
        !IsX86LegacyPrefix(0x8EU) && !IsX86LegacyPrefix(0xEEU) &&
        !IsX86LegacyPrefix(0xCFU) && !IsX86LegacyPrefix(0x1FU);

    const CensusPtr census = MakeCensus();
    // A bare opcode.
    const std::uint8_t mov_sreg[] = {0x8CU, 0xD8U};
    Record(census.get(), mov_sreg);
    // Operand-size prefix in front of the same opcode: the census must resolve
    // past it rather than counting `66` as the instruction.
    const std::uint8_t prefixed[] = {0x66U, 0x8CU, 0xD8U};
    Record(census.get(), prefixed);
    // Segment override plus operand size.
    const std::uint8_t double_prefixed[] = {0x26U, 0x66U, 0x8EU, 0xD8U};
    Record(census.get(), double_prefixed);
    // Two-byte escape: `0F B2` is `LSS`, and the second byte is the identity.
    const std::uint8_t lss[] = {0x0FU, 0xB2U, 0x00U};
    Record(census.get(), lss);
    const std::uint8_t mov_cr[] = {0x0FU, 0x20U, 0xC0U};
    Record(census.get(), mov_cr);
    const std::uint8_t prefixed_escape[] = {0x26U, 0x0FU, 0xB2U, 0x00U};
    Record(census.get(), prefixed_escape);

    const bool effective =
        census->effective_opcode_counts[0x8CU] == 2U &&
        census->effective_opcode_counts[0x8EU] == 1U &&
        census->effective_opcode_counts[0x0FU] == 3U &&
        // Prefixes must not appear as opcodes at all.
        census->effective_opcode_counts[0x66U] == 0U &&
        census->effective_opcode_counts[0x26U] == 0U;
    const bool escape =
        census->escape_count == 3U &&
        census->escape_opcode_counts[0xB2U] == 2U &&
        census->escape_opcode_counts[0x20U] == 1U;
    const bool prefixes =
        census->sample_count == 6U &&
        census->prefixed_count == 3U &&
        census->segment_prefixed_count == 2U &&
        census->operand_size_prefixed_count == 2U &&
        census->prefix_overflow_count == 0U &&
        census->escape_truncated_count == 0U;
    // The gate the measurement rests on: one sample in, one opcode counted.
    const bool partition =
        HistogramTotal(census->effective_opcode_counts) ==
        census->sample_count;

    // A truncated escape has no second byte to attribute, and must be counted
    // rather than reading past the sample.
    const CensusPtr truncated = MakeCensus();
    const std::uint8_t escape_only[] = {0x0FU};
    Record(truncated.get(), escape_only);
    const bool truncation =
        truncated->escape_count == 1U &&
        truncated->escape_truncated_count == 1U &&
        HistogramTotal(truncated->escape_opcode_counts) == 0U &&
        HistogramTotal(truncated->effective_opcode_counts) ==
            truncated->sample_count;

    // A run of prefixes must stop at the bound instead of walking away.
    const CensusPtr overflow = MakeCensus();
    const std::uint8_t all_prefixes[] = {
        0x66U, 0x67U, 0xF0U, 0xF2U, 0x26U, 0x2EU, 0x8CU};
    Record(overflow.get(), all_prefixes);
    const bool bounded =
        overflow->prefix_overflow_count == 1U &&
        overflow->sample_count == 1U &&
        // Stopped at the bound, so the byte there is what gets counted.
        HistogramTotal(overflow->effective_opcode_counts) == 1U &&
        kAotMaxLegacyPrefixes == 4U;

    // Prefixes filling the whole sample leave no opcode visible.
    const CensusPtr prefix_only = MakeCensus();
    const std::uint8_t only_prefixes[] = {0x66U, 0x67U};
    Record(prefix_only.get(), only_prefixes);
    const bool no_opcode =
        prefix_only->sample_count == 1U &&
        prefix_only->escape_truncated_count == 1U &&
        HistogramTotal(prefix_only->effective_opcode_counts) == 0U;

    const CensusPtr ff = MakeCensus();
    const std::uint8_t ff_call[] = {0xFFU, 0xD0U};
    const std::uint8_t ff_jump[] = {0xFFU, 0xE0U};
    const std::uint8_t prefixed_ff_call[] = {0x66U, 0xFFU, 0x10U};
    Record(ff.get(), ff_call);
    Record(ff.get(), ff_jump);
    Record(ff.get(), prefixed_ff_call);
    const bool ff_groups =
        ff->effective_opcode_counts[0xFFU] == 3U &&
        ff->ff_group_counts[2] == 2U &&
        ff->ff_group_counts[4] == 1U &&
        ff->ff_modrm_truncated_count == 0U;

    const CensusPtr ff_truncated = MakeCensus();
    const std::uint8_t ff_only[] = {0xFFU};
    Record(ff_truncated.get(), ff_only);
    const bool ff_truncation =
        ff_truncated->effective_opcode_counts[0xFFU] == 1U &&
        ff_truncated->ff_modrm_truncated_count == 1U &&
        FfGroupTotal(ff_truncated->ff_group_counts) == 0U;

    const std::uint8_t ff_register[] = {0xFFU, 0xE0U};
    const std::uint8_t ff_absolute[] = {0xFFU, 0x25U};
    const std::uint8_t ff_base[] = {0xFFU, 0x60U};
    const std::uint8_t ff_sib[] = {0xFFU, 0x64U};
    const std::uint8_t ff_address16[] = {0x67U, 0xFFU, 0x20U};
    AotFfBoundaryDecoded decoded_register;
    AotFfBoundaryDecoded decoded_absolute;
    AotFfBoundaryDecoded decoded_base;
    AotFfBoundaryDecoded decoded_sib;
    AotFfBoundaryDecoded decoded_address16;
    DecodeAotFfBoundary(
        ff_register, sizeof(ff_register), &decoded_register);
    DecodeAotFfBoundary(
        ff_absolute, sizeof(ff_absolute), &decoded_absolute);
    DecodeAotFfBoundary(ff_base, sizeof(ff_base), &decoded_base);
    DecodeAotFfBoundary(ff_sib, sizeof(ff_sib), &decoded_sib);
    DecodeAotFfBoundary(
        ff_address16, sizeof(ff_address16), &decoded_address16);
    const bool ff_modes =
        decoded_register.effective_ff && decoded_register.modrm_present &&
        decoded_register.addressing_mode == AotFfAddressingMode::kRegister &&
        decoded_absolute.addressing_mode == AotFfAddressingMode::kAbsolute &&
        decoded_base.addressing_mode == AotFfAddressingMode::kBase &&
        decoded_sib.addressing_mode == AotFfAddressingMode::kSib &&
        decoded_address16.addressing_mode ==
            AotFfAddressingMode::kAddress16;

    AotFfBoundaryAttribution attribution;
    RecordAotFfBoundarySample(
        &attribution, 0x00001000U, ff_register, sizeof(ff_register));
    RecordAotFfBoundarySample(
        &attribution, 0x00001000U, ff_register, sizeof(ff_register));
    RecordAotFfBoundarySample(
        &attribution, 0x00002000U, ff_sib, sizeof(ff_sib));
    RecordAotFfBoundarySample(
        &attribution, 0x00001000U, ff_absolute, sizeof(ff_absolute));
    AotFfBoundarySiteHotspot hotspots[2] = {};
    RankAotFfBoundarySites(attribution, hotspots, 2U);
    const bool ff_sites =
        attribution.sample_count == 4U && attribution.site_count == 2U &&
        hotspots[0].guest_eip == 0x00001000U &&
        hotspots[0].count == 3U &&
        hotspots[0].byte_change_count == 1U &&
        hotspots[0].mode_change_count == 1U &&
        hotspots[0].addressing_mode == AotFfAddressingMode::kAbsolute &&
        hotspots[1].guest_eip == 0x00002000U && hotspots[1].count == 1U;

    AotFfBoundaryAttribution ff_site_attribution;
    for (std::uint32_t index = 0U;
         index < kAotFfBoundarySiteCapacity + 1U; ++index)
    {
        RecordAotFfBoundarySample(
            &ff_site_attribution,
            0x00003000U + index,
            ff_register,
            sizeof(ff_register));
    }
    const bool ff_site_overflow =
        ff_site_attribution.sample_count == kAotFfBoundarySiteCapacity + 1U &&
        ff_site_attribution.site_count == kAotFfBoundarySiteCapacity &&
        ff_site_attribution.site_overflow_count == 1U;

    AotFfBoundaryAttribution attribution_truncated;
    const std::uint8_t ff_attribution_only[] = {0xFFU};
    RecordAotFfBoundarySample(
        &attribution_truncated,
        0x00004000U,
        ff_attribution_only,
        sizeof(ff_attribution_only));
    const bool ff_attribution_truncation =
        attribution_truncated.sample_count == 0U &&
        attribution_truncated.modrm_truncated_count == 1U;

    const std::uint8_t ff_target_register[] = {0xFFU, 0xE2U};
    AotFfBoundaryTargetDecoded target_register_decoded;
    const AotFfTargetStatus target_register_status =
        DecodeAotFfBoundaryTarget(
            ff_target_register, sizeof(ff_target_register),
            &target_register_decoded);
    repiu::platform::GuestCpuContext target_registers;
    target_registers.Edx = 0x12345678U;
    engine::ThreadContext target_context;
    AotFfBoundaryTargetResult target_register_result;
    const AotFfTargetStatus target_register_resolved =
        ResolveAotFfBoundaryTarget(
            target_register_decoded,
            &target_context,
            &target_registers,
            &target_register_result);

    const std::uint8_t ff_target_absolute[] = {
        0x2EU, 0xFFU, 0x25U, 0x10U, 0x00U, 0x00U, 0x00U};
    AotFfBoundaryTargetDecoded target_absolute_decoded;
    const AotFfTargetStatus target_absolute_status =
        DecodeAotFfBoundaryTarget(
            ff_target_absolute, sizeof(ff_target_absolute),
            &target_absolute_decoded);
    const std::uint8_t ff_target_address16[] = {
        0x67U, 0xFFU, 0x20U};
    AotFfBoundaryTargetDecoded target_address16_decoded;
    const AotFfTargetStatus target_address16_status =
        DecodeAotFfBoundaryTarget(
            ff_target_address16, sizeof(ff_target_address16),
            &target_address16_decoded);
    const std::uint8_t ff_target_truncated[] = {
        0x2EU, 0xFFU, 0x24U, 0x95U};
    AotFfBoundaryTargetDecoded target_truncated_decoded;
    const AotFfTargetStatus target_truncated_status =
        DecodeAotFfBoundaryTarget(
            ff_target_truncated, sizeof(ff_target_truncated),
            &target_truncated_decoded);

    std::uint8_t target_memory[128] = {};
    const std::uint8_t ff_target_sib[] = {
        0x2EU, 0xFFU, 0x24U, 0x95U, 0x10U, 0x00U, 0x00U, 0x00U};
    std::memcpy(target_memory, ff_target_sib, sizeof(ff_target_sib));
    const std::uint32_t target_memory_base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(target_memory));
    target_context.runtime_base = target_memory_base;
    target_context.runtime_size = sizeof(target_memory);
    target_registers.Edx = 0U;
    const std::uint32_t target_pointer = target_memory_base + 0x10U;
    std::memcpy(target_memory + 0x04U, &target_pointer,
                sizeof(target_pointer));
    const std::uint32_t first_target = 0x12345678U;
    std::memcpy(target_memory + 0x10U, &first_target,
                sizeof(first_target));
    AotFfBoundaryAttribution target_attribution;
    RecordAotFfBoundarySample(
        &target_attribution, target_memory_base, target_memory, 4U);
    RecordAotFfBoundaryTargetSample(
        &target_attribution, &target_context, &target_registers,
        target_memory_base, target_memory, 4U);
    RecordAotFfBoundaryTargetSample(
        &target_attribution, &target_context, &target_registers,
        target_memory_base, target_memory, 4U);
    const std::uint32_t second_target = 0xCAFEBABEU;
    std::memcpy(target_memory + 0x10U, &second_target,
                sizeof(second_target));
    RecordAotFfBoundaryTargetSample(
        &target_attribution, &target_context, &target_registers,
        target_memory_base, target_memory, 4U);
    const std::uint32_t indexed_target = 0x0BADF00DU;
    std::memcpy(target_memory + 0x14U, &indexed_target,
                sizeof(indexed_target));
    target_registers.Edx = 1U;
    RecordAotFfBoundaryTargetSample(
        &target_attribution, &target_context, &target_registers,
        target_memory_base, target_memory, 4U);

    SetAotFfTargetTimingCandidate(
        &target_attribution.target_timing,
        target_memory_base,
        indexed_target,
        true,
        2U,
        1U);
    const bool target_timing_started =
        BeginAotFfTargetTimingIfMatched(
            &target_attribution.target_timing,
            indexed_target,
            0x00ABCDEFU,
            1000U);
    const bool target_timing_completed =
        CompleteAotFfTargetTiming(
            &target_attribution.target_timing,
            1100U);
    const AotFfTargetTimingEntry& target_timing_entry =
        target_attribution.target_timing.entries[0];

    const std::uint8_t ff_target_unreadable[] = {
        0x2EU, 0xFFU, 0x24U, 0x95U, 0x00U, 0x00U, 0x00U, 0x00U};
    std::memcpy(target_memory + 0x20U, ff_target_unreadable,
                sizeof(ff_target_unreadable));
    target_registers.Edx = 0xFFFFFFF0U;
    const std::uint32_t unreadable_eip = target_memory_base + 0x20U;
    RecordAotFfBoundarySample(
        &target_attribution, unreadable_eip, target_memory + 0x20U, 4U);
    RecordAotFfBoundaryTargetSample(
        &target_attribution, &target_context, &target_registers,
        unreadable_eip, target_memory + 0x20U, 4U);
    const AotFfBoundarySite& target_site = target_attribution.sites[0];
    AotFfBoundarySiteHotspot target_hotspot;
    RankAotFfBoundarySites(
        target_attribution, &target_hotspot, 1U);
    const bool ff_target_attribution =
        target_register_status == AotFfTargetStatus::kResolved &&
        target_register_decoded.register_form &&
        target_register_resolved == AotFfTargetStatus::kResolved &&
        target_register_result.target_valid &&
        target_register_result.target == 0x12345678U &&
        target_absolute_status == AotFfTargetStatus::kResolved &&
        target_absolute_decoded.has_displacement &&
        target_absolute_decoded.displacement == 0x10U &&
        target_absolute_decoded.instruction_size == 7U &&
        target_address16_status ==
            AotFfTargetStatus::kUnsupportedAddressSize &&
        target_truncated_status ==
            AotFfTargetStatus::kInstructionTruncated &&
        target_attribution.target_resolved_count == 4U &&
        target_attribution.target_unresolved_count == 1U &&
        target_attribution.target_memory_unreadable_count == 1U &&
        target_attribution.site_count == 2U &&
        target_site.target_read_count == 4U &&
        target_site.target_failure_count == 0U &&
        target_site.last_displacement == target_pointer &&
        target_site.last_pointer_address == target_pointer + 4U &&
        target_site.last_pointer_address_valid &&
        target_site.last_target == indexed_target &&
        target_site.pointer_change_count == 1U &&
        target_site.target_change_count == 2U &&
        target_site.target_change_with_same_pointer_count == 1U &&
        target_site.target_change_with_pointer_change_count == 1U &&
        target_site.last_index_register == 2U &&
        target_site.last_index_value == 1U &&
        target_site.last_index_value_valid &&
        target_site.index_value_change_count == 1U &&
        target_site.last_base_register == 0U &&
        target_site.last_base_value == 0U &&
        !target_site.last_base_value_valid &&
        target_site.base_value_change_count == 0U &&
        target_site.index_value_observation_sample_count == 4U &&
        target_site.index_value_observation_slot_count == 2U &&
        target_site.index_value_observation_overflow_count == 0U &&
        target_site.index_value_observations[0].valid &&
        target_site.index_value_observations[0].register_number == 2U &&
        target_site.index_value_observations[0].value == 0U &&
        target_site.index_value_observations[0].count == 3U &&
        target_site.index_value_observations[1].valid &&
        target_site.index_value_observations[1].register_number == 2U &&
        target_site.index_value_observations[1].value == 1U &&
        target_site.index_value_observations[1].count == 1U &&
        target_site.index_transition_count == 1U &&
        target_site.index_transition_slot_count == 1U &&
        target_site.index_transition_overflow_count == 0U &&
        target_site.index_transitions[0].valid &&
        target_site.index_transitions[0].from_register == 2U &&
        target_site.index_transitions[0].from_value == 0U &&
        target_site.index_transitions[0].to_register == 2U &&
        target_site.index_transitions[0].to_value == 1U &&
        target_hotspot.guest_eip == target_site.guest_eip &&
        target_hotspot.index_value_observation_sample_count == 4U &&
        target_hotspot.index_value_observation_slot_count == 2U &&
        target_hotspot.index_value_observation_overflow_count == 0U &&
        target_hotspot.index_value_observations[0].valid &&
        target_hotspot.index_value_observations[0].value == 0U &&
        target_hotspot.index_value_observations[0].count == 3U &&
        target_hotspot.index_value_observations[1].valid &&
        target_hotspot.index_value_observations[1].value == 1U &&
        target_hotspot.index_value_observations[1].count == 1U &&
        target_hotspot.index_transition_count == 1U &&
        target_hotspot.index_transition_slot_count == 1U &&
        target_hotspot.index_transition_overflow_count == 0U &&
        target_hotspot.index_transitions[0].valid &&
        target_hotspot.index_transitions[0].from_value == 0U &&
        target_hotspot.index_transitions[0].to_value == 1U &&
        target_site.displacement_change_count == 0U &&
        target_attribution.sites[1].target_failure_count == 1U &&
        target_timing_started && target_timing_completed &&
        target_attribution.target_timing.interval_started_count == 1U &&
        target_attribution.target_timing.interval_completed_count == 1U &&
        !target_attribution.target_timing.interval_active &&
        target_timing_entry.valid &&
        target_timing_entry.source_guest_eip == target_memory_base &&
        target_timing_entry.target_guest_eip == indexed_target &&
        target_timing_entry.cache_target == 0x00ABCDEFU &&
        target_timing_entry.index_value_valid &&
        target_timing_entry.index_register == 2U &&
        target_timing_entry.index_value == 1U &&
        target_timing_entry.interval_count == 1U &&
        target_timing_entry.total_cycles == 100U &&
        target_timing_entry.min_cycles == 100U &&
        target_timing_entry.max_cycles == 100U;

    AotOpcodeRank ranks[4] = {};
    RankAotOpcodeHistogram(census->effective_opcode_counts, ranks, 4U);
    const bool ranking =
        ranks[0].opcode == 0x0FU && ranks[0].count == 3U &&
        ranks[1].opcode == 0x8CU && ranks[1].count == 2U &&
        ranks[2].opcode == 0x8EU && ranks[2].count == 1U &&
        ranks[3].count == 0U;

    AotBoundaryOpcodeCensus untouched;
    RecordAotBoundaryOpcodeSample(nullptr, mov_sreg, sizeof(mov_sreg));
    RecordAotBoundaryOpcodeSample(&untouched, nullptr, 4U);
    RecordAotBoundaryOpcodeSample(&untouched, mov_sreg, 0U);
    RankAotOpcodeHistogram(nullptr, ranks, 4U);
    const bool inert =
        untouched.sample_count == 0U &&
        untouched.empty_sample_count == 2U &&
        ranks[0].count == 0U;

    const bool all = prefix_set && effective && escape && prefixes &&
        partition && truncation && bounded && no_opcode && ff_groups &&
        ff_truncation && ff_modes && ff_sites && ff_site_overflow &&
        ff_attribution_truncation && ff_target_attribution && ranking &&
        inert;
    std::cout << "aot_boundary_opcode_census_prefix_set="
              << (prefix_set ? "true" : "false")
              << "\naot_boundary_opcode_census_effective="
              << (effective ? "true" : "false")
              << "\naot_boundary_opcode_census_escape="
              << (escape ? "true" : "false")
              << "\naot_boundary_opcode_census_prefixes="
              << (prefixes ? "true" : "false")
              << "\naot_boundary_opcode_census_partition="
              << (partition ? "true" : "false")
              << "\naot_boundary_opcode_census_truncation="
              << (truncation ? "true" : "false")
              << "\naot_boundary_opcode_census_bounded="
              << (bounded ? "true" : "false")
              << "\naot_boundary_opcode_census_no_opcode="
              << (no_opcode ? "true" : "false")
              << "\naot_boundary_opcode_census_ff_groups="
              << (ff_groups ? "true" : "false")
              << "\naot_boundary_opcode_census_ff_truncation="
              << (ff_truncation ? "true" : "false")
              << "\naot_ff_boundary_attribution_modes="
              << (ff_modes ? "true" : "false")
              << "\naot_ff_boundary_attribution_sites="
              << (ff_sites ? "true" : "false")
              << "\naot_ff_boundary_attribution_overflow="
              << (ff_site_overflow ? "true" : "false")
              << "\naot_ff_boundary_attribution_truncation="
              << (ff_attribution_truncation ? "true" : "false")
              << "\naot_ff_boundary_target_attribution="
              << (ff_target_attribution ? "true" : "false")
              << "\naot_ff_target_timing="
              << (target_timing_started && target_timing_completed
                      ? "true" : "false")
              << "\naot_boundary_opcode_census_ranking="
              << (ranking ? "true" : "false")
              << "\naot_boundary_opcode_census_inert="
              << (inert ? "true" : "false")
              << "\naot_boundary_opcode_census_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
