#include "repiu/update/semantic_version.h"

#include <charconv>
#include <limits>

namespace repiu::update
{
namespace
{

bool IsSpace(char character)
{
    return character == ' ' || character == '\t' || character == '\r' ||
        character == '\n';
}

// Reads one decimal part up to `separator` (or the end when it is '\0').
bool ReadPart(std::string_view* text, char separator, std::uint32_t* value)
{
    const std::size_t end = separator == '\0' ? text->size()
                                              : text->find(separator);
    if (end == 0U || end == std::string_view::npos)
    {
        return false;
    }
    const std::string_view part = text->substr(0, end);
    for (char character : part)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
    }
    std::uint64_t parsed = 0;
    const auto result =
        std::from_chars(part.data(), part.data() + part.size(), parsed);
    if (result.ec != std::errc{} ||
        parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    *value = static_cast<std::uint32_t>(parsed);
    text->remove_prefix(separator == '\0' ? end : end + 1U);
    return true;
}

}  // namespace

std::optional<SemanticVersion> ParseSemanticVersion(std::string_view text)
{
    while (!text.empty() && IsSpace(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && IsSpace(text.back()))
    {
        text.remove_suffix(1);
    }
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
    {
        text.remove_prefix(1);
    }
    SemanticVersion version;
    if (!ReadPart(&text, '.', &version.major) ||
        !ReadPart(&text, '.', &version.minor) ||
        !ReadPart(&text, '\0', &version.patch) || !text.empty())
    {
        return std::nullopt;
    }
    return version;
}

std::string_view BuildVersionText()
{
#if defined(REPIU_VERSION)
    return REPIU_VERSION;
#else
    return "unknown";
#endif
}

std::string FormatSemanticVersion(const SemanticVersion& version)
{
    return std::to_string(version.major) + "." +
        std::to_string(version.minor) + "." + std::to_string(version.patch);
}

}  // namespace repiu::update
