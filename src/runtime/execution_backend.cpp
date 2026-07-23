#include "repiu/runtime/execution_backend.h"

namespace repiu::runtime
{

bool ParseExecutionBackend(std::string_view value,
                           ExecutionBackend* backend)
{
    if (backend == nullptr)
    {
        return false;
    }
    if (value == "legacy")
    {
        *backend = ExecutionBackend::kLegacy;
    }
    else if (value == "aot")
    {
        *backend = ExecutionBackend::kAot;
    }
    else if (value == "aot-dynamic")
    {
        *backend = ExecutionBackend::kAotDynamic;
    }
    else if (value == "aot-dbt")
    {
        *backend = ExecutionBackend::kAotDbt;
    }
    else
    {
        return false;
    }
    return true;
}

std::string_view ExecutionBackendName(ExecutionBackend backend)
{
    switch (backend)
    {
        case ExecutionBackend::kLegacy:
            return "legacy";
        case ExecutionBackend::kAot:
            return "aot";
        case ExecutionBackend::kAotDynamic:
            return "aot-dynamic";
        case ExecutionBackend::kAotDbt:
            return "aot-dbt";
    }
    return "legacy";
}

bool ExecutionBackendUsesAot(ExecutionBackend backend)
{
    return backend != ExecutionBackend::kLegacy;
}

bool ExecutionBackendUsesDynamicTranslation(ExecutionBackend backend)
{
    return backend == ExecutionBackend::kAotDynamic ||
           backend == ExecutionBackend::kAotDbt;
}

bool ExecutionBackendUsesImmediateHleReentry(ExecutionBackend backend)
{
    return backend == ExecutionBackend::kAotDbt;
}

}  // namespace repiu::runtime
