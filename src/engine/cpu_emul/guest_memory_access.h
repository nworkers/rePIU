#pragma once

// Guest linear-memory and shadow-memory access primitives extracted from
// execution_trampoline.cpp (Phase 1 increment 5). Called throughout the
// instruction handlers; NoteSuccessfulAotGuestWrite (AOT hook) stays in the
// trampoline and is declared in execution_internal.h.

#include "thread_context.h"

#include <cstdint>
#include <string>

namespace repiu::engine
{

bool IsGuestRangeReadable(ThreadContext* context, const void* source, std::uint32_t byte_count);
bool IsGuestRangeWritable(ThreadContext* context, void* destination, std::uint32_t byte_count);

bool WriteGuestUInt16(ThreadContext* context, void* destination, std::uint16_t value);
bool WriteGuestUInt8(ThreadContext* context, void* destination, std::uint8_t value);
bool WriteGuestUInt32(ThreadContext* context, void* destination, std::uint32_t value);
bool ReadGuestUInt32(ThreadContext* context, const void* source, std::uint32_t* value);

void WriteShadowMemory(ThreadContext* context, std::uint32_t destination, std::uint32_t value, std::uint32_t byte_count);
bool ReadShadowUInt32(ThreadContext* context, std::uint32_t source, std::uint32_t* value);
bool ReadShadowUInt8(ThreadContext* context, std::uint32_t source, std::uint8_t* value);

bool AppendConsoleOutput(ThreadContext* context, const void* source, std::uint32_t byte_count, bool stderr_stream = false);
bool ReadGuestAsciz(ThreadContext* context, std::uint32_t address, std::uint32_t max_length, std::string* value);

} // namespace repiu::engine
