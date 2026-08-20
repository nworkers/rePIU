#ifndef REPIU_CONFIG_INI_DOCUMENT_H_
#define REPIU_CONFIG_INI_DOCUMENT_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::config
{

struct IniEntry
{
    std::string section;
    std::string key;
    std::string value;
    std::uint32_t line_number = 0;
};

// A parsed INI file.
//
// Parsing never fails. A config-file typo must not stop the game from running,
// so an unrecognized line is recorded in `warnings` and skipped while every
// other line is still applied.
class IniDocument
{
public:
    // Recognized syntax:
    //   ; comment           # comment
    //   [Section]
    //   key = value
    // Whitespace around keys and values is trimmed, and a value fully wrapped
    // in matching ' or " quotes is unwrapped. Entries appear in file order;
    // resolving a duplicate key is the caller's job, and FindLast implements
    // the last-one-wins rule the format documents.
    static IniDocument Parse(std::string_view text,
                             std::string_view origin_label);

    const std::vector<IniEntry>& entries() const
    {
        return entries_;
    }

    const std::vector<std::string>& warnings() const
    {
        return warnings_;
    }

    // The value of the last entry matching both names, or nullptr when absent.
    // Both comparisons use EqualsConfigName, so case and underscores are
    // ignored on either side.
    const std::string* FindLast(std::string_view section,
                                std::string_view key) const;

private:
    std::vector<IniEntry> entries_;
    std::vector<std::string> warnings_;
};

}  // namespace repiu::config

#endif  // REPIU_CONFIG_INI_DOCUMENT_H_
