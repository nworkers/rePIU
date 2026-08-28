#pragma once

#include <cstddef>
#include <cstdint>

namespace repiu::engine
{

// Task 367: identifies which instructions produce the dominant exception
// population.
//
// `hle`-provenance breakpoints are 42.54% of all exceptions, but the existing
// boundary census records only `bytes[0]`, so its largest entry -- `0F` at
// 119,235 -- is the two-byte opcode escape and says nothing about the
// instruction, while `66` and `26` are prefixes. This census skips legacy
// prefixes to record the effective opcode and separately records the second byte
// behind a `0F` escape. It counts only; nothing here changes behaviour.

constexpr std::size_t kWin32AotOpcodeHistogramSize = 256U;

// A malformed or truncated byte sequence must never let the prefix skip run past
// the sample, so the walk is bounded and the overflow is counted.
constexpr std::uint32_t kWin32AotMaxLegacyPrefixes = 4U;

struct Win32AotBoundaryOpcodeCensus
{
    // Effective opcode after legacy prefixes, for every sample.
    std::uint32_t effective_opcode_counts[kWin32AotOpcodeHistogramSize] = {};
    // Second byte of a `0F`-escaped instruction. This is what identifies the
    // largest population.
    std::uint32_t escape_opcode_counts[kWin32AotOpcodeHistogramSize] = {};
    std::uint32_t sample_count = 0;
    std::uint32_t escape_count = 0;
    std::uint32_t prefixed_count = 0;
    std::uint32_t segment_prefixed_count = 0;
    std::uint32_t operand_size_prefixed_count = 0;
    std::uint32_t address_size_prefixed_count = 0;
    std::uint32_t repeat_prefixed_count = 0;
    std::uint32_t lock_prefixed_count = 0;
    // Ran out of bytes before the opcode, or exceeded the prefix bound.
    std::uint32_t escape_truncated_count = 0;
    std::uint32_t prefix_overflow_count = 0;
    std::uint32_t empty_sample_count = 0;
};

bool IsX86LegacyPrefix(std::uint8_t value);

// Counts one boundary sample. `bytes`/`length` are the instruction bytes the
// dispatcher already captured.
void RecordAotBoundaryOpcodeSample(Win32AotBoundaryOpcodeCensus* census,
                                   const std::uint8_t* bytes,
                                   std::size_t length);

struct Win32AotOpcodeRank
{
    std::uint8_t opcode = 0;
    std::uint32_t count = 0;
};

// Fills `ranks` with the top `capacity` entries by count, descending, ties broken
// by opcode. Called at exit only.
void RankAotOpcodeHistogram(const std::uint32_t* counts,
                            Win32AotOpcodeRank* ranks,
                            std::size_t capacity);

}  // namespace repiu::engine
