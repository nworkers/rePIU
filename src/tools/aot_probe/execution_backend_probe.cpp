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
        bool aot;
        bool dynamic;
        bool immediate_hle_reentry;
    } cases[] = {
        {"legacy", ExecutionBackend::kLegacy, false, false, false},
        {"aot", ExecutionBackend::kAot, true, false, false},
        {"aot-dynamic", ExecutionBackend::kAotDynamic, true, true, false},
        {"aot-dbt", ExecutionBackend::kAotDbt, true, true, true},
    };

    bool all = true;
    for (const auto& test : cases)
    {
        ExecutionBackend parsed = ExecutionBackend::kLegacy;
        all = all &&
            runtime::ParseExecutionBackend(test.name, &parsed) &&
            parsed == test.backend &&
            runtime::ExecutionBackendName(parsed) == test.name &&
            runtime::ExecutionBackendUsesAot(parsed) == test.aot &&
            runtime::ExecutionBackendUsesDynamicTranslation(parsed) ==
                test.dynamic &&
            runtime::ExecutionBackendUsesImmediateHleReentry(parsed) ==
                test.immediate_hle_reentry;
    }

    ExecutionBackend unchanged = ExecutionBackend::kAotDbt;
    const bool invalid_rejected =
        !runtime::ParseExecutionBackend("unknown", &unchanged) &&
        unchanged == ExecutionBackend::kAotDbt &&
        !runtime::ParseExecutionBackend("legacy", nullptr);
    all = all && invalid_rejected;

    std::cout << "execution_backend_policy="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
