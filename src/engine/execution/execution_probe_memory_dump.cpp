#include "repiu/engine/execution_probe_memory_dump.h"

#include <cstdlib>
#include <cstring>
#include <fstream>

namespace repiu::engine
{
namespace
{

struct BaseRegisterName
{
    const char* text;
    Win32ExecutionProbeDumpBase base;
};

constexpr BaseRegisterName kBaseRegisterNames[] = {
    {"eax", Win32ExecutionProbeDumpBase::kEax},
    {"ebx", Win32ExecutionProbeDumpBase::kEbx},
    {"ecx", Win32ExecutionProbeDumpBase::kEcx},
    {"edx", Win32ExecutionProbeDumpBase::kEdx},
    {"esi", Win32ExecutionProbeDumpBase::kEsi},
    {"edi", Win32ExecutionProbeDumpBase::kEdi},
    {"ebp", Win32ExecutionProbeDumpBase::kEbp},
    {"esp", Win32ExecutionProbeDumpBase::kEsp},
};

char LowerAscii(char value)
{
    return (value >= 'A' && value <= 'Z')
               ? static_cast<char>(value - 'A' + 'a')
               : value;
}

bool EqualsIgnoreAsciiCase(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0')
    {
        if (LowerAscii(*left) != LowerAscii(*right))
        {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

bool ParseUnsigned(const char* text, std::uint32_t* out)
{
    if (text == nullptr || *text == '\0')
    {
        return false;
    }
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > UINT32_MAX)
    {
        return false;
    }
    *out = static_cast<std::uint32_t>(value);
    return true;
}

// Returns nullptr for an unset or empty variable so callers treat both the same
// way.
const char* ReadEnvironment(const char* name)
{
    const char* value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? value : nullptr;
}

}  // namespace

bool ResolveWin32ExecutionProbeDumpBase(
    const char* text, Win32ExecutionProbeDumpBase* out_base,
    std::uint32_t* out_absolute)
{
    if (text == nullptr || out_base == nullptr || out_absolute == nullptr)
    {
        return false;
    }
    for (const auto& entry : kBaseRegisterNames)
    {
        if (EqualsIgnoreAsciiCase(text, entry.text))
        {
            *out_base = entry.base;
            *out_absolute = 0;
            return true;
        }
    }
    std::uint32_t absolute = 0;
    if (!ParseUnsigned(text, &absolute))
    {
        return false;
    }
    *out_base = Win32ExecutionProbeDumpBase::kAbsolute;
    *out_absolute = absolute;
    return true;
}

Win32ExecutionProbeDumpRequest ReadWin32ExecutionProbeDumpRequest()
{
    Win32ExecutionProbeDumpRequest request;
    const char* path = ReadEnvironment("REPIU_EXECUTION_PROBE_DUMP_PATH");
    const char* byte_count_text =
        ReadEnvironment("REPIU_EXECUTION_PROBE_DUMP_BYTES");
    if (path == nullptr || byte_count_text == nullptr)
    {
        return request;
    }
    std::uint32_t byte_count = 0;
    if (!ParseUnsigned(byte_count_text, &byte_count) || byte_count == 0U ||
        byte_count > kWin32ExecutionProbeDumpMaxBytes)
    {
        return request;
    }
    const char* base_text = ReadEnvironment("REPIU_EXECUTION_PROBE_DUMP_BASE");
    if (base_text != nullptr &&
        !ResolveWin32ExecutionProbeDumpBase(
            base_text, &request.base, &request.absolute_base))
    {
        return request;
    }
    const char* offset_text =
        ReadEnvironment("REPIU_EXECUTION_PROBE_DUMP_OFFSET");
    if (offset_text != nullptr && !ParseUnsigned(offset_text, &request.offset))
    {
        return request;
    }
    const char* indirect_text =
        ReadEnvironment("REPIU_EXECUTION_PROBE_DUMP_INDIRECT");
    request.indirect = indirect_text != nullptr && indirect_text[0] == '1' &&
                       indirect_text[1] == '\0';
    request.byte_count = byte_count;
    request.path = path;
    request.configured = true;
    return request;
}

bool WriteWin32ExecutionProbeDump(
    const Win32ExecutionProbeDumpRequest& request,
    Win32ExecutionProbeDumpResult* result)
{
    if (result == nullptr || !request.configured || !result->captured ||
        result->written || request.path.empty() || result->byte_count == 0U ||
        result->bytes.size() < result->byte_count)
    {
        return false;
    }
    std::ofstream stream(request.path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(result->bytes.data()),
                 static_cast<std::streamsize>(result->byte_count));
    if (!stream.good())
    {
        return false;
    }
    stream.close();
    result->written = stream.good();
    return result->written;
}

}  // namespace repiu::engine
