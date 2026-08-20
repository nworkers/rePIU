#ifndef REPIU_CONFIG_CONFIG_NAME_H_
#define REPIU_CONFIG_CONFIG_NAME_H_

#include <string_view>

namespace repiu::config
{

// The single comparison rule for every identifier that appears in a config
// file: section names, entry keys, host key names, and modifier names.
//
// Case and underscores are ignored, so "Keypad7", "KEYPAD_7", and "keypad_7"
// are one name. Underscores are skipped rather than treated as separators
// because the generated file uses them for readability while a hand-edited
// file may not.
//
// Header-only and allocation-free: the JAMMA binding loader compares names
// while resolving the configuration, and nothing here may allocate.
constexpr bool EqualsConfigName(std::string_view left, std::string_view right)
{
    auto to_lower = [](char value) -> char
    {
        return value >= 'A' && value <= 'Z'
            ? static_cast<char>(value - 'A' + 'a')
            : value;
    };

    std::size_t left_index = 0;
    std::size_t right_index = 0;
    while (true)
    {
        while (left_index < left.size() && left[left_index] == '_')
        {
            ++left_index;
        }
        while (right_index < right.size() && right[right_index] == '_')
        {
            ++right_index;
        }

        const bool left_done = left_index == left.size();
        const bool right_done = right_index == right.size();
        if (left_done || right_done)
        {
            return left_done && right_done;
        }

        if (to_lower(left[left_index]) != to_lower(right[right_index]))
        {
            return false;
        }
        ++left_index;
        ++right_index;
    }
}

}  // namespace repiu::config

#endif  // REPIU_CONFIG_CONFIG_NAME_H_
