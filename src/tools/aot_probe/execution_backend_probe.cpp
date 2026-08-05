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
    // Task 425: 옛 이름은 별칭이 아니라 거부입니다. 옛 절차가 조용히 다른
    // backend로 실행되지 않는다는 성질을 여기서 고정합니다. 거부된 값은
    // 출력 인자를 건드리지 않아야 합니다.
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

    std::cout << "execution_backend_policy="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
