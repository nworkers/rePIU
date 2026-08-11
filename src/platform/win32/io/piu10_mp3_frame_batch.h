#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace repiu::platform::win32
{

struct ThreadContext;

struct Piu10Mp3FrameBatchPlan
{
    std::span<const std::uint8_t> bytes;
    std::uint32_t* source_cursor = nullptr;
    std::uint32_t* frame_byte_count = nullptr;
    std::uint32_t service_cursor_threshold = 0U;
    std::uint32_t service_counter_limit = 0U;
};

bool BuildPiu10Mp3FrameBatchPlan(
    ThreadContext* context, std::uint32_t guest_source,
    std::uint32_t guest_stack_pointer, std::size_t maximum_bytes,
    Piu10Mp3FrameBatchPlan* plan);

bool CommitPiu10Mp3FrameBatch(
    const Piu10Mp3FrameBatchPlan& plan, std::size_t accepted_bytes,
    std::uint32_t* guest_ecx);

std::size_t TransferPiu10Mp3FrameTail(
    ThreadContext* context, std::uint32_t guest_source,
    std::uint32_t guest_stack_pointer, std::uint8_t current_byte,
    std::uint32_t* guest_ecx);

}  // namespace repiu::platform::win32
