#ifndef REPIU_ENGINE_EXECUTION_PROBE_MEMORY_DUMP_H_
#define REPIU_ENGINE_EXECUTION_PROBE_MEMORY_DUMP_H_

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::engine
{

// Upper bound on a single dump. The capture buffer is allocated once before
// execution starts and the bytes never travel through the telemetry snapshot,
// so this is the whole host cost of the diagnostic. One megabyte is large
// enough to hold a decoded asset buffer next to the structure that references
// it, which is what locating a producer of a short buffer requires. A larger
// request is rejected rather than silently truncated.
constexpr std::uint32_t kExecutionProbeDumpMaxBytes = 1048576U;

enum class ExecutionProbeDumpBase : std::uint32_t
{
    kEax = 0,
    kEbx,
    kEcx,
    kEdx,
    kEsi,
    kEdi,
    kEbp,
    kEsp,
    kAbsolute,
    kCount,
};

struct ExecutionProbeDumpRequest
{
    bool configured = false;
    ExecutionProbeDumpBase base = ExecutionProbeDumpBase::kEax;
    // Used only when `base` is kAbsolute.
    std::uint32_t absolute_base = 0;
    std::uint32_t offset = 0;
    // When set, the four bytes at base+offset are a guest pointer and the dump
    // starts there instead. This follows a pointer argument passed on the stack.
    bool indirect = false;
    std::uint32_t byte_count = 0;
    std::string path;
};

struct ExecutionProbeDumpResult
{
    bool captured = false;
    bool written = false;
    std::uint32_t base_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t byte_count = 0;
    // Sized to `byte_count` at configuration time so the first-hit recorder
    // never allocates while handling an exception.
    std::vector<std::uint8_t> bytes;
};

// Parses the REPIU_EXECUTION_PROBE_DUMP_* group. The returned request has
// `configured` false when the group is absent, malformed, or over the bound.
ExecutionProbeDumpRequest ReadExecutionProbeDumpRequest();

// Resolves a base register name or absolute address. Returns false when `text`
// names neither.
bool ResolveExecutionProbeDumpBase(
    const char* text, ExecutionProbeDumpBase* out_base,
    std::uint32_t* out_absolute);

// Writes a captured dump to `request.path`. Returns false when nothing was
// captured, no path was given, or the file could not be written.
bool WriteExecutionProbeDump(
    const ExecutionProbeDumpRequest& request,
    ExecutionProbeDumpResult* result);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_EXECUTION_PROBE_MEMORY_DUMP_H_
