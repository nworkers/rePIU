#ifndef REPIU_RUNTIME_EXECUTION_BACKEND_H_
#define REPIU_RUNTIME_EXECUTION_BACKEND_H_

#include <string_view>

namespace repiu::runtime
{

// Task 425: the backend names the execution policy a user selects. The subsystem
// that builds the static code cache is still called `AOT`, and the translation
// layer running on that cache at runtime is still called `DBT`. The three names
// refer to three different layers.
enum class ExecutionBackend
{
    kLegacy,
    kDynamic
};

// Task 435: the backend chosen when nothing is set. It is `dynamic` because
// every promotion of this generation was measured on that path and because the
// run procedures and measurement scripts all specify it; `legacy` remains
// selectable as the regression control.
inline constexpr ExecutionBackend kDefaultExecutionBackend =
    ExecutionBackend::kDynamic;

// The old names `aot`, `aot-dynamic`, and `aot-dbt` are rejected rather than
// aliased, so a stale procedure cannot silently run a different backend.
bool ParseExecutionBackend(std::string_view value,
                           ExecutionBackend* backend);

// Resolves one environment value into a backend. Unset and empty mean the
// default; an unknown value is rejected so the caller can report it and stop,
// which is better than silently running a different execution policy.
bool ResolveExecutionBackend(const char* value, ExecutionBackend* backend);

std::string_view ExecutionBackendName(ExecutionBackend backend);

// With only two backends, building the static cache, translating dynamically,
// and re-entering immediately after an HLE boundary are one and the same
// condition; separate names would only imply policies that do not differ. A
// third backend would reintroduce the real distinctions with their evidence.
bool ExecutionBackendUsesDynamicTranslation(ExecutionBackend backend);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_EXECUTION_BACKEND_H_
