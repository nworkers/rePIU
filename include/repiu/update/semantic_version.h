#ifndef REPIU_UPDATE_SEMANTIC_VERSION_H_
#define REPIU_UPDATE_SEMANTIC_VERSION_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace repiu::update
{

// Issue #48. The project's `major.minor.patch` (VERSION, REPIU_VERSION) and a
// release tag's `v` form of it, compared as numbers so 0.0.10 follows 0.0.9.
struct SemanticVersion
{
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    friend auto operator<=>(const SemanticVersion&,
                            const SemanticVersion&) = default;
};

// Accepts `1.2.3` or `v1.2.3`, with surrounding whitespace (a VERSION file's
// trailing newline). Anything else -- a missing part, a suffix, an overflow --
// is no version at all.
[[nodiscard]] std::optional<SemanticVersion> ParseSemanticVersion(
    std::string_view text);

// The version this binary was built as (the repository's VERSION at configure
// time), as text; "unknown" when the build defined none.
[[nodiscard]] std::string_view BuildVersionText();

// `1.2.3`, without the `v`.
[[nodiscard]] std::string FormatSemanticVersion(const SemanticVersion& version);

}  // namespace repiu::update

#endif  // REPIU_UPDATE_SEMANTIC_VERSION_H_
