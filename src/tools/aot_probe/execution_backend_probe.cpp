#include "execution_backend_probe.h"

#include "repiu/runtime/execution_backend.h"

#include <iostream>

namespace repiu::tools
{

bool RunExecutionBackendProbe()
{
    using runtime::ExecutionBackend;

    const struct
    {
        const char* name;
        ExecutionBackend backend;
        bool dynamic;
    } cases[] = {
        {"legacy", ExecutionBackend::kLegacy, false},
        {"dynamic", ExecutionBackend::kDynamic, true},
    };

    bool all = true;
    for (const auto& test : cases)
    {
        ExecutionBackend parsed = ExecutionBackend::kLegacy;
        all = all &&
            runtime::ParseExecutionBackend(test.name, &parsed) &&
            parsed == test.backend &&
            runtime::ExecutionBackendName(parsed) == test.name &&
            runtime::ExecutionBackendUsesDynamicTranslation(parsed) ==
                test.dynamic;
    }

    ExecutionBackend unchanged = ExecutionBackend::kDynamic;
    // Task 425: the old names are rejected rather than aliased. This pins the
    // property that a stale procedure cannot silently run a different backend,
    // and a rejected value must leave the output argument untouched.
    const bool legacy_names_rejected =
        !runtime::ParseExecutionBackend("aot", &unchanged) &&
        !runtime::ParseExecutionBackend("aot-dynamic", &unchanged) &&
        !runtime::ParseExecutionBackend("aot-dbt", &unchanged) &&
        unchanged == ExecutionBackend::kDynamic;
    all = all && legacy_names_rejected;

    const bool invalid_rejected =
        !runtime::ParseExecutionBackend("unknown", &unchanged) &&
        unchanged == ExecutionBackend::kDynamic &&
        !runtime::ParseExecutionBackend("legacy", nullptr);
    all = all && invalid_rejected;

    // Task 435: unset and empty resolving to `dynamic` is the product default.
    // Without this assertion the default could quietly revert inside the host.
    // An unknown value is rejected, returning false so the caller can stop.
    ExecutionBackend resolved = ExecutionBackend::kLegacy;
    const bool default_is_dynamic =
        runtime::ResolveExecutionBackend(nullptr, &resolved) &&
        resolved == runtime::kDefaultExecutionBackend &&
        resolved == ExecutionBackend::kDynamic;
    resolved = ExecutionBackend::kLegacy;
    const bool empty_is_dynamic =
        runtime::ResolveExecutionBackend("", &resolved) &&
        resolved == ExecutionBackend::kDynamic;
    resolved = ExecutionBackend::kDynamic;
    const bool explicit_legacy_kept =
        runtime::ResolveExecutionBackend("legacy", &resolved) &&
        resolved == ExecutionBackend::kLegacy;
    const bool unknown_resolve_rejected =
        !runtime::ResolveExecutionBackend("aot-dbt", &resolved) &&
        !runtime::ResolveExecutionBackend("", nullptr);
    all = all && default_is_dynamic && empty_is_dynamic &&
        explicit_legacy_kept && unknown_resolve_rejected;

    std::cout << "execution_backend_policy="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
