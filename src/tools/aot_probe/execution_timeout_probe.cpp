#include "execution_timeout_probe.h"

#include "repiu/runtime/execution_timeout.h"

#include <iostream>

namespace repiu::tools
{

bool RunExecutionTimeoutProbe()
{
    using runtime::kDefaultExecutionTimeoutMilliseconds;
    using runtime::kUnlimitedExecutionTimeoutMilliseconds;
    using runtime::ResolveExecutionTimeoutMilliseconds;

    // Task 435: the default budget is unlimited. A procedure that needs a bound
    // states its own, and this assertion pins that contract.
    const bool default_is_unlimited =
        kDefaultExecutionTimeoutMilliseconds ==
            kUnlimitedExecutionTimeoutMilliseconds &&
        ResolveExecutionTimeoutMilliseconds(nullptr) ==
            kUnlimitedExecutionTimeoutMilliseconds &&
        ResolveExecutionTimeoutMilliseconds("") ==
            kUnlimitedExecutionTimeoutMilliseconds;

    // An explicit `0` has always meant "no limit" and still does.
    const bool explicit_zero_is_unlimited =
        ResolveExecutionTimeoutMilliseconds("0") ==
        kUnlimitedExecutionTimeoutMilliseconds;

    const bool explicit_budget_kept =
        ResolveExecutionTimeoutMilliseconds("1000") == 1000U &&
        ResolveExecutionTimeoutMilliseconds("45000") == 45000U &&
        ResolveExecutionTimeoutMilliseconds("1") == 1U;

    // A trailing remainder or a non-numeric value falls back to the default.
    // Reading only half of `1000ms` and accepting 1,000 would run a procedure
    // on a budget it never asked for.
    const bool malformed_falls_back =
        ResolveExecutionTimeoutMilliseconds("1000ms") ==
            kDefaultExecutionTimeoutMilliseconds &&
        ResolveExecutionTimeoutMilliseconds(" 1000") ==
            kDefaultExecutionTimeoutMilliseconds &&
        ResolveExecutionTimeoutMilliseconds("-1") ==
            kDefaultExecutionTimeoutMilliseconds &&
        ResolveExecutionTimeoutMilliseconds("infinite") ==
            kDefaultExecutionTimeoutMilliseconds &&
        ResolveExecutionTimeoutMilliseconds("99999999999") ==
            kDefaultExecutionTimeoutMilliseconds;

    const bool all = default_is_unlimited && explicit_zero_is_unlimited &&
        explicit_budget_kept && malformed_falls_back;

    std::cout << "execution_timeout_policy=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
