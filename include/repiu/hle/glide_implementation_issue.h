#ifndef REPIU_HLE_GLIDE_IMPLEMENTATION_ISSUE_H
#define REPIU_HLE_GLIDE_IMPLEMENTATION_ISSUE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::hle
{

constexpr std::size_t kGlideImplementationIssueArgumentCapacity = 8u;
constexpr std::size_t kGlideImplementationIssueRecordCapacity = 128u;

enum class GlideImplementationIssueKind : std::uint8_t
{
    kUnimplementedFunction = 0u,
    kUnsupportedArgument,
    kBackendFailure,
    kAbiReject,
};

enum class GlideImplementationIssueRecordResult : std::uint8_t
{
    kRepeated = 0u,
    kInserted,
    kOverflow,
};

struct GlideImplementationIssueObservation
{
    GlideImplementationIssueKind kind = GlideImplementationIssueKind::kUnimplementedFunction;
    std::uint16_t ordinal = 0u;
    std::uint32_t argument_byte_count = 0u;
    std::uint64_t count = 0u;
    std::array<std::uint32_t, kGlideImplementationIssueArgumentCapacity> arguments{};
    std::string name;
    std::string reason;
    std::string detail;
};

class GlideImplementationIssueTracker
{
public:
    GlideImplementationIssueRecordResult Record(
        GlideImplementationIssueKind kind, std::uint16_t ordinal, std::string_view name,
        std::string_view reason, std::string_view detail, std::uint32_t argument_byte_count,
        const std::array<std::uint32_t, kGlideImplementationIssueArgumentCapacity>& arguments);

    [[nodiscard]] std::uint64_t total(GlideImplementationIssueKind kind) const;
    [[nodiscard]] std::uint64_t overflow_count() const;
    [[nodiscard]] const std::vector<GlideImplementationIssueObservation>& observations() const;

private:
    static constexpr std::size_t kKindCount = 4u;

    std::array<std::uint64_t, kKindCount> totals_{};
    std::uint64_t overflow_count_ = 0u;
    std::vector<GlideImplementationIssueObservation> observations_;
};

[[nodiscard]] const char* GlideImplementationIssueKindName(GlideImplementationIssueKind kind);
[[nodiscard]] bool IsGlideImplementationIssueFatal(GlideImplementationIssueKind kind);
[[nodiscard]] std::string FormatGlideImplementationIssue(
    const GlideImplementationIssueObservation& observation, std::string_view action);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_IMPLEMENTATION_ISSUE_H
