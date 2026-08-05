#ifndef REPIU_RUNTIME_EXECUTION_BACKEND_H_
#define REPIU_RUNTIME_EXECUTION_BACKEND_H_

#include <string_view>

namespace repiu::runtime
{

// Task 425: backend는 사용자가 고르는 실행 정책 이름입니다. 정적 code cache를
// 만드는 하위 시스템은 계속 `AOT`, 그 cache 위에서 런타임에 동작하는 번역 계층은
// 계속 `DBT`라고 부릅니다. 세 이름은 서로 다른 층을 가리킵니다.
//
// Task 425: the backend names the execution policy a user selects. The subsystem
// that builds the static code cache is still called `AOT`, and the translation
// layer running on that cache at runtime is still called `DBT`. The three names
// refer to three different layers.
enum class ExecutionBackend
{
    kLegacy,
    kDynamic
};

// 옛 이름 `aot`, `aot-dynamic`, `aot-dbt`는 별칭이 아니라 거부입니다. 옛 절차가
// 조용히 다른 backend로 실행되지 않게 합니다.
//
// The old names `aot`, `aot-dynamic`, and `aot-dbt` are rejected rather than
// aliased, so a stale procedure cannot silently run a different backend.
bool ParseExecutionBackend(std::string_view value,
                           ExecutionBackend* backend);

std::string_view ExecutionBackendName(ExecutionBackend backend);

// backend가 둘뿐이므로 정적 cache 생성, 동적 번역, HLE 직후 즉시 재진입은 모두
// 같은 조건입니다. 세 이름을 따로 두면 서로 다른 정책이라는 인상만 남습니다.
// 세 번째 backend가 생기면 실제 정책 차이를 근거와 함께 다시 도입합니다.
//
// With only two backends, building the static cache, translating dynamically,
// and re-entering immediately after an HLE boundary are one and the same
// condition; separate names would only imply policies that do not differ. A
// third backend would reintroduce the real distinctions with their evidence.
bool ExecutionBackendUsesDynamicTranslation(ExecutionBackend backend);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_EXECUTION_BACKEND_H_
