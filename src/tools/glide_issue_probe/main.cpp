#include "repiu/hle/glide_implementation_issue.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{

using repiu::hle::GlideImplementationIssueKind;
using repiu::hle::GlideImplementationIssueRecordResult;
using repiu::hle::GlideImplementationIssueTracker;

bool Check(const bool condition, const char* message)
{
    if (condition)
    {
        return true;
    }
    std::cerr << "glide_issue_probe failure: " << message << '\n';
    return false;
}

}  // namespace

int main()
{
    std::array<std::uint32_t, repiu::hle::kGlideImplementationIssueArgumentCapacity> arguments{};
    arguments[0] = 3u;

    GlideImplementationIssueTracker tracker;
    if (!Check(tracker.Record(GlideImplementationIssueKind::kUnimplementedFunction, 17u,
                              "_GRUNIMPLEMENTED@4", "catalog-default-handler",
                              "ABI-preserving default", 4u,
                              arguments) == GlideImplementationIssueRecordResult::kInserted,
               "first issue was not inserted") ||
        !Check(tracker.Record(GlideImplementationIssueKind::kUnimplementedFunction, 17u,
                              "_GRUNIMPLEMENTED@4", "catalog-default-handler",
                              "repeated detail may differ", 4u,
                              arguments) == GlideImplementationIssueRecordResult::kRepeated,
               "duplicate issue was not coalesced") ||
        !Check(tracker.total(GlideImplementationIssueKind::kUnimplementedFunction) == 2u,
               "unimplemented total was not preserved") ||
        !Check(tracker.observations().size() == 1u && tracker.observations()[0].count == 2u,
               "duplicate observation count is incorrect"))
    {
        return 1;
    }

    const std::string fatal_line =
        repiu::hle::FormatGlideImplementationIssue(tracker.observations()[0], "continue");
    const std::string expected_fatal_line =
        "GLIDE_UNIMPLEMENTED_FUNCTION action=continue ordinal=17"
        " name=_GRUNIMPLEMENTED@4 reason=catalog-default-handler"
        " detail=\"ABI-preserving default\" argument_bytes=4 count=2"
        " args=0x00000003";
    if (!Check(fatal_line == expected_fatal_line, "stable fatal log format is incorrect"))
    {
        return 1;
    }
    std::cout << "[repiu-fatal] " << fatal_line << '\n';
    std::cout << "[critical] FATAL " << fatal_line << '\n';

    arguments[0] = 7u;
    if (!Check(tracker.Record(GlideImplementationIssueKind::kUnimplementedFunction, 17u,
                              "_GRUNIMPLEMENTED@4", "catalog-default-handler", {}, 4u,
                              arguments) == GlideImplementationIssueRecordResult::kInserted,
               "argument variant was not retained") ||
        !Check(tracker.observations().size() == 2u,
               "argument variant did not create a unique observation") ||
        !Check(repiu::hle::IsGlideImplementationIssueFatal(
                   GlideImplementationIssueKind::kUnsupportedArgument),
               "unsupported argument must be fatal") ||
        !Check(!repiu::hle::IsGlideImplementationIssueFatal(
                   GlideImplementationIssueKind::kBackendFailure),
               "backend failure must remain error severity") ||
        !Check(std::string(repiu::hle::GlideImplementationIssueKindName(
                   GlideImplementationIssueKind::kAbiReject)) == "GLIDE_ABI_REJECT",
               "ABI category name is unstable"))
    {
        return 1;
    }

    GlideImplementationIssueTracker overflow_tracker;
    arguments.fill(0u);
    for (std::size_t index = 0u; index <= repiu::hle::kGlideImplementationIssueRecordCapacity;
         ++index)
    {
        const auto result = overflow_tracker.Record(
            GlideImplementationIssueKind::kBackendFailure, static_cast<std::uint16_t>(index),
            "_GRPROBE@0", "reason-" + std::to_string(index), {}, 0u, arguments);
        const auto expected = index < repiu::hle::kGlideImplementationIssueRecordCapacity
                                  ? GlideImplementationIssueRecordResult::kInserted
                                  : GlideImplementationIssueRecordResult::kOverflow;
        if (!Check(result == expected, "record capacity policy is incorrect"))
        {
            return 1;
        }
    }

    if (!Check(overflow_tracker.observations().size() ==
                   repiu::hle::kGlideImplementationIssueRecordCapacity,
               "record capacity was exceeded") ||
        !Check(overflow_tracker.overflow_count() == 1u, "overflow count is incorrect") ||
        !Check(overflow_tracker.total(GlideImplementationIssueKind::kBackendFailure) ==
                   repiu::hle::kGlideImplementationIssueRecordCapacity + 1u,
               "overflowed issue was not included in totals"))
    {
        return 1;
    }

    std::cout << "glide_issue_probe=pass\n";
    return 0;
}
