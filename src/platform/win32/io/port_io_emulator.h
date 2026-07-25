#pragma once

#include "thread_context.h"
#include <cstdint>
#include <string>

namespace repiu::platform::win32
{

void RecordPortIo(ThreadContext* context,
                  std::uint32_t address,
                  std::uint32_t opcode,
                  std::uint16_t port,
                  std::uint32_t width,
                  std::uint32_t value,
                  bool is_input,
                  bool handled,
                  const std::string& result);

bool IsPortIoTraceCandidate(std::uint16_t port,
                            std::uint32_t width,
                            bool is_input);

bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context);

} // namespace repiu::platform::win32
