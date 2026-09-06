#ifndef REPIU_ENGINE_GUEST_WRITE_TRACE_H_
#define REPIU_ENGINE_GUEST_WRITE_TRACE_H_

#include <cstdint>

namespace repiu::platform
{
struct GuestCpuContext;
}

namespace repiu::engine
{

enum class GuestWriteTraceEvent : std::uint32_t
{
    kHle = 0,
    kNativeFault,
    kNativeComplete,
};

std::uint32_t GuestWriteTraceAddress();
bool GuestWriteTraceMatches(
    std::uint32_t destination,
    std::uint32_t byte_count);
bool GuestWriteTracePageMatches(std::uint32_t guest_address);
void RecordGuestWriteTrace(
    GuestWriteTraceEvent event,
    std::uint32_t execution_address,
    std::uint32_t guest_source,
    std::uint32_t destination,
    std::uint32_t byte_count,
    const void* bytes = nullptr,
    const repiu::platform::GuestCpuContext* registers = nullptr);
void DumpGuestWriteTraceTail(int file_descriptor);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GUEST_WRITE_TRACE_H_
