#include "aot_boundary_opcode_census_probe.h"

#include "repiu/engine/aot_boundary_opcode_census.h"

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
        partition && truncation && bounded && no_opcode && ranking && inert;
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
              << "\naot_boundary_opcode_census_ranking="
              << (ranking ? "true" : "false")
              << "\naot_boundary_opcode_census_inert="
              << (inert ? "true" : "false")
              << "\naot_boundary_opcode_census_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
