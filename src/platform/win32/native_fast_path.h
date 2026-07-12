#pragma once

#include <atomic>
#include <cstdint>
#include <unordered_map>

struct _CONTEXT;
using CONTEXT = _CONTEXT;

namespace repiu::platform::win32::detail
{

struct NativeFastPathState
{
    bool active = false;
    std::uint32_t return_address = 0;
    std::uint32_t saved_dr0 = 0;
    std::uint32_t saved_dr6 = 0;
    std::uint32_t saved_dr7 = 0;
    std::atomic<std::uint32_t> entry_count{0};
    std::atomic<std::uint32_t> return_count{0};
    std::atomic<std::uint32_t> cancel_count{0};
    std::uint32_t last_entry = 0;
    std::uint32_t last_return = 0;
    std::uint32_t previous_eip = 0;
    std::unordered_map<std::uint32_t, std::int8_t> verification_cache;
    std::atomic<std::uint32_t> verified_count{0};
    std::atomic<std::uint32_t> rejected_count{0};
    std::atomic<std::uint32_t> last_rejected_instruction{0};
    std::atomic<std::uint32_t> last_rejected_opcode{0};
    std::atomic<std::uint32_t> last_rejected_candidate{0};
    std::atomic<std::uint32_t> last_rejected_bytes_low{0};
    std::atomic<std::uint32_t> last_rejected_bytes_high{0};
};

bool TryEnterNativeFastPath(CONTEXT* context,
                            NativeFastPathState* state,
                            std::uint32_t runtime_base,
                            std::uint32_t runtime_size);
void LeaveNativeFastPath(CONTEXT* context,
                         NativeFastPathState* state,
                         bool returned);

}  // namespace repiu::platform::win32::detail
