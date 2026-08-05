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
    else if (value == "dynamic")
    {
        *backend = ExecutionBackend::kDynamic;
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
        case ExecutionBackend::kDynamic:
            return "dynamic";
    }
    return "legacy";
}

bool ExecutionBackendUsesDynamicTranslation(ExecutionBackend backend)
{
    return backend == ExecutionBackend::kDynamic;
}

}  // namespace repiu::runtime
