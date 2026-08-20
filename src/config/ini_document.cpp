#include "repiu/config/ini_document.h"

#include "repiu/config/config_name.h"

#include <sstream>

namespace repiu::config
{
namespace
{

constexpr std::string_view kWhitespace = " \t\r\f\v";

std::string_view Trim(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(kWhitespace);
    if (first == std::string_view::npos)
    {
        return std::string_view();
    }
    const std::size_t last = text.find_last_not_of(kWhitespace);
    return text.substr(first, last - first + 1);
}

// Values may be quoted so a mapping can be written as 'Q', matching how key
// literals are usually spelled. Only a matching pair wraps, and only when it
// wraps the whole value; an interior quote is left alone.
std::string_view Unquote(std::string_view value)
{
    if (value.size() < 2)
    {
        return value;
    }
    const char first = value.front();
    if ((first == '\'' || first == '"') && value.back() == first)
    {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string DescribeLine(std::string_view origin_label,
                         std::uint32_t line_number,
                         std::string_view reason,
                         std::string_view content)
{
    std::ostringstream stream;
    stream << origin_label << ":" << line_number << ": " << reason << " \""
           << content << "\"";
    return stream.str();
}

}  // namespace

IniDocument IniDocument::Parse(std::string_view text,
                               std::string_view origin_label)
{
    IniDocument document;
    std::string current_section;
    std::uint32_t line_number = 0;
    std::size_t offset = 0;

    while (offset <= text.size())
    {
        const std::size_t line_end = text.find('\n', offset);
        const std::string_view raw_line = line_end == std::string_view::npos
            ? text.substr(offset)
            : text.substr(offset, line_end - offset);
        offset = line_end == std::string_view::npos ? text.size() + 1
                                                    : line_end + 1;
        ++line_number;

        const std::string_view line = Trim(raw_line);
        if (line.empty() || line.front() == ';' || line.front() == '#')
        {
            continue;
        }

        if (line.front() == '[')
        {
            if (line.back() != ']')
            {
                document.warnings_.push_back(DescribeLine(
                    origin_label, line_number, "unterminated section header",
                    line));
                continue;
            }
            current_section =
                std::string(Trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string_view::npos)
        {
            document.warnings_.push_back(DescribeLine(
                origin_label, line_number, "expected key = value", line));
            continue;
        }

        const std::string_view key = Trim(line.substr(0, separator));
        if (key.empty())
        {
            document.warnings_.push_back(
                DescribeLine(origin_label, line_number, "empty key", line));
            continue;
        }

        // An empty value is deliberate rather than an error: it is how a
        // config file turns an input off.
        const std::string_view value =
            Unquote(Trim(line.substr(separator + 1)));

        IniEntry entry;
        entry.section = current_section;
        entry.key = std::string(key);
        entry.value = std::string(value);
        entry.line_number = line_number;
        document.entries_.push_back(std::move(entry));
    }

    return document;
}

const std::string* IniDocument::FindLast(std::string_view section,
                                         std::string_view key) const
{
    const std::string* result = nullptr;
    for (const IniEntry& entry : entries_)
    {
        if (EqualsConfigName(entry.section, section) &&
            EqualsConfigName(entry.key, key))
        {
            result = &entry.value;
        }
    }
    return result;
}

}  // namespace repiu::config
