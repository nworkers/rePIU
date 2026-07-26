#include "repiu/hle/glide_implementation_issue.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace repiu::hle
{
namespace
{

constexpr std::size_t KindIndex(const GlideImplementationIssueKind kind)
{
    return static_cast<std::size_t>(kind);
}

std::size_t CapturedArgumentCount(const std::uint32_t argument_byte_count)
{
    return std::min<std::size_t>(argument_byte_count / sizeof(std::uint32_t),
                                 kGlideImplementationIssueArgumentCapacity);
}

}  // namespace

GlideImplementationIssueRecordResult GlideImplementationIssueTracker::Record(
    const GlideImplementationIssueKind kind, const std::uint16_t ordinal,
    const std::string_view name, const std::string_view reason, const std::string_view detail,
    const std::uint32_t argument_byte_count,
    const std::array<std::uint32_t, kGlideImplementationIssueArgumentCapacity>& arguments)
{
    const auto kind_index = KindIndex(kind);
    if (kind_index >= totals_.size())
    {
        ++overflow_count_;
        return GlideImplementationIssueRecordResult::kOverflow;
    }

    ++totals_[kind_index];
    const auto argument_count = CapturedArgumentCount(argument_byte_count);
    for (auto& observation : observations_)
    {
        if (observation.kind != kind || observation.ordinal != ordinal ||
            observation.argument_byte_count != argument_byte_count || observation.name != name ||
            observation.reason != reason ||
            !std::equal(arguments.begin(),
                        arguments.begin() + static_cast<std::ptrdiff_t>(argument_count),
                        observation.arguments.begin()))
        {
            continue;
        }

        ++observation.count;
        return GlideImplementationIssueRecordResult::kRepeated;
    }

    if (observations_.size() >= kGlideImplementationIssueRecordCapacity)
    {
        ++overflow_count_;
        return GlideImplementationIssueRecordResult::kOverflow;
    }

    GlideImplementationIssueObservation observation{};
    observation.kind = kind;
    observation.ordinal = ordinal;
    observation.argument_byte_count = argument_byte_count;
    observation.count = 1u;
    observation.arguments = arguments;
    observation.name.assign(name);
    observation.reason.assign(reason);
    observation.detail.assign(detail);
    observations_.push_back(std::move(observation));
    return GlideImplementationIssueRecordResult::kInserted;
}

std::uint64_t GlideImplementationIssueTracker::total(const GlideImplementationIssueKind kind) const
{
    const auto kind_index = KindIndex(kind);
    return kind_index < totals_.size() ? totals_[kind_index] : 0u;
}

std::uint64_t GlideImplementationIssueTracker::overflow_count() const
{
    return overflow_count_;
}

const std::vector<GlideImplementationIssueObservation>&
GlideImplementationIssueTracker::observations() const
{
    return observations_;
}

const char* GlideImplementationIssueKindName(const GlideImplementationIssueKind kind)
{
    switch (kind)
    {
        case GlideImplementationIssueKind::kUnimplementedFunction:
            return "GLIDE_UNIMPLEMENTED_FUNCTION";
        case GlideImplementationIssueKind::kUnsupportedArgument:
            return "GLIDE_UNSUPPORTED_ARGUMENT";
        case GlideImplementationIssueKind::kBackendFailure:
            return "GLIDE_BACKEND_FAILURE";
        case GlideImplementationIssueKind::kAbiReject:
            return "GLIDE_ABI_REJECT";
    }
    return "GLIDE_UNKNOWN_ISSUE";
}

bool IsGlideImplementationIssueFatal(const GlideImplementationIssueKind kind)
{
    return kind != GlideImplementationIssueKind::kBackendFailure;
}

std::string FormatGlideImplementationIssue(const GlideImplementationIssueObservation& observation,
                                           const std::string_view action)
{
    std::ostringstream stream;
    stream << GlideImplementationIssueKindName(observation.kind) << " action=" << action
           << " ordinal=" << observation.ordinal << " name=" << observation.name
           << " reason=" << observation.reason << " detail=\"" << observation.detail << '"'
           << " argument_bytes=" << observation.argument_byte_count
           << " count=" << observation.count << " args=";
    const auto argument_count = CapturedArgumentCount(observation.argument_byte_count);
    for (std::size_t index = 0U; index < argument_count; ++index)
    {
        if (index != 0U)
        {
            stream << ',';
        }
        stream << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
               << observation.arguments[index];
    }
    return stream.str();
}

}  // namespace repiu::hle
