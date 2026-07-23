#ifndef REPIU_RUNTIME_EXECUTION_BACKEND_H_
#define REPIU_RUNTIME_EXECUTION_BACKEND_H_

#include <string_view>

namespace repiu::runtime
{

enum class ExecutionBackend
{
    kLegacy,
    kAot,
    kAotDynamic,
    kAotDbt
};

bool ParseExecutionBackend(std::string_view value,
                           ExecutionBackend* backend);

std::string_view ExecutionBackendName(ExecutionBackend backend);

bool ExecutionBackendUsesAot(ExecutionBackend backend);

bool ExecutionBackendUsesDynamicTranslation(ExecutionBackend backend);

bool ExecutionBackendUsesImmediateHleReentry(ExecutionBackend backend);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_EXECUTION_BACKEND_H_
