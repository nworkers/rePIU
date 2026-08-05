# Task 425 작업 지시 — backend를 legacy / dynamic으로 축소하고 개명

설계: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §4~§6 ·
선결: [Task 424](20260805-424-dbt-build-option-toggles.md)

**Task 424의 완료 기준이 충족되기 전에는 시작하지 않습니다.**

## 0. 선결 확인 — 이것부터 하고 나머지를 시작합니다

설계 §4.3의 죽은 분기를 지우려면 **AOT dispatcher가 `legacy`에서 도달 불가능**해야
합니다. `AttemptWin32GuestStackAotExecution` 경로가 `use_aot_backend`가 참일 때만
진입하고, `ThreadContext::execution_backend`가 그 경로에서만 dispatcher에 도달하는지
코드로 따라가 확인합니다.

**도달 가능하다면 §2.3의 분기 삭제를 취소하고**, 조건을 남긴 채 나머지 축소만
진행합니다. 그 판단과 근거를 작업 로그에 적습니다. 이 확인 없이 지우지 않습니다.

## 1. enum과 파서

`include/repiu/runtime/execution_backend.h`

```cpp
enum class ExecutionBackend
{
    kLegacy,
    kDynamic
};

bool ParseExecutionBackend(std::string_view value, ExecutionBackend* backend);
std::string_view ExecutionBackendName(ExecutionBackend backend);
bool ExecutionBackendUsesDynamicTranslation(ExecutionBackend backend);
```

`src/runtime/execution_backend.cpp`

* `"legacy"` → `kLegacy`, `"dynamic"` → `kDynamic`. **그 외는 전부 `false` 반환.**
* `"aot"`, `"aot-dynamic"`, `"aot-dbt"`에 별칭을 두지 않습니다.
* `ExecutionBackendUsesAot`와 `ExecutionBackendUsesImmediateHleReentry`를 삭제하고
  세 호출 지점을 `ExecutionBackendUsesDynamicTranslation`으로 통일합니다.

## 2. 호출 지점

### 2.1 `src/host/win32/main.cpp`

| 위치 | 변경 |
|---|---|
| 에러 문구 | `"REPIU_EXECUTION_BACKEND must be legacy or dynamic"` |
| `use_aot_backend` | `use_dynamic_backend`로 개명, `ExecutionBackendUsesDynamicTranslation` 호출 |
| `== kAotDbt` 11곳 | `use_dynamic_backend`로 치환 |

`== kAotDbt` 조건이 `use_dynamic_backend`와 동치가 되므로 각 build option 줄은
`use_dynamic_backend && ResolvePromotedToggle(...)` 형태가 됩니다. **Task 384 / 386 /
390 / 291 / 424 주석은 전부 보존합니다.**

로그 문자열의 `AOT-DBT` 접두사는 설계 §4.4에 따라 **그대로 둡니다**. 내부 기법
이름이며 가이드와 분석 문서가 이 줄을 이름으로 인용합니다.

### 2.2 나머지 호출 지점

| 파일 | 내용 |
|---|---|
| `src/platform/win32/aot/aot_dbt_dispatch.cpp` | `UsesImmediateHleReentry` → `UsesDynamicTranslation`. `hle_reentry_reject_backend` 카운터와 주석 유지 |
| `src/platform/win32/aot/aot_dbt_glide_gate_dispatch.cpp` | `kAotDbt` → `kDynamic` |
| `src/platform/win32/execution/execution_trampoline.cpp` | `kAotDbt` → `kDynamic` |
| `src/platform/win32/native_linear_span.cpp` | `kAotDbt` → `kDynamic` |
| `src/platform/win32/glide_opengl_backend.cpp` | `SetExecutionBackend` — 창 제목에 나가는 이름이 `dynamic`으로 바뀌는 것을 확인만 합니다 |
| `src/platform/win32/execution/thread_context.h` | 주석의 backend 이름 |

`native_linear_span.h`의 `Resolve*` 함수는 **backend 인자를 유지합니다**(설계 §4.5).

### 2.3 `aot_runtime_dispatch.cpp`의 죽은 분기

§0이 통과했을 때만 수행합니다.

```cpp
// 이전
dynamic_translation_failed =
    (!dynamic_translation && !retired_target) ||
    !RequestAotDynamicTranslation(context, target, ...);

// 이후 — 첫 항은 정적 전용 aot backend 전용이었습니다 (Task 425)
dynamic_translation_failed =
    !RequestAotDynamicTranslation(context, target, ...);
```

`dynamic_translation` 지역 변수는 `aot_dynamic_attempt_count` 증가에 계속 쓰이므로
지우지 않습니다. **short-circuit 순서가 바뀌면 `RequestAotDynamicTranslation`의
호출 횟수가 달라지므로**, 위 형태에서 호출 조건이 넓어지지 않는지 확인합니다
(`dynamic_translation`이 항상 참이면 이전 식도 항상 호출했습니다).

## 3. probe

### 3.1 `execution_backend_probe.cpp`

케이스 표를 두 행으로 줄이고, **거부 케이스를 명시적으로 추가**합니다.

```cpp
{"legacy",  ExecutionBackend::kLegacy,  false},
{"dynamic", ExecutionBackend::kDynamic, true},
```

```cpp
// Task 425: 옛 이름은 별칭이 아니라 거부입니다. 옛 절차가 조용히
// 다른 backend로 실행되지 않게 하는 성질을 여기서 고정합니다.
!ParseExecutionBackend("aot", &unchanged) &&
!ParseExecutionBackend("aot-dynamic", &unchanged) &&
!ParseExecutionBackend("aot-dbt", &unchanged) &&
unchanged == ExecutionBackend::kDynamic
```

기존의 `"unknown"` 거부와 `nullptr` 거부 케이스는 유지합니다.

### 3.2 `native_linear_span_probe.cpp`

`kAot` / `kAotDynamic`을 쓰는 케이스 16곳을 정리합니다. **"dbt가 아닌 backend에서는
기본 OFF, 명시 지정이면 ON"이라는 성질은 반드시 남깁니다** — 반례 backend만
`kLegacy`로 바꿉니다.

```cpp
// 이전
!ResolveNativeLinearSpanEnabled(ExecutionBackend::kAotDynamic, "") &&
 ResolveNativeLinearSpanEnabled(ExecutionBackend::kAotDynamic, "1") &&
 ResolveNativeLinearSpanEnabled(ExecutionBackend::kAot, "true") &&

// 이후
!ResolveNativeLinearSpanEnabled(ExecutionBackend::kLegacy, "") &&
 ResolveNativeLinearSpanEnabled(ExecutionBackend::kLegacy, "1") &&
 ResolveNativeLinearSpanEnabled(ExecutionBackend::kLegacy, "true") &&
```

중복된 반례가 생기면 합치되, **행 수가 줄었다는 이유로 성질을 빼지 않습니다.**

## 4. 스크립트 11개

`"aot-dbt"` → `"dynamic"`으로 바꿉니다.

```
scripts/task283_indirect_call_jump_split.ps1
scripts/task284_call_return_trace.ps1
scripts/task285_call_step_probe.ps1
scripts/task286_dispatch_site_lifetime_ab.ps1
scripts/task287_direct_linear_span_ab.ps1
scripts/task347_release_axis_reattribution.ps1
scripts/task365_glide_setter_state_elision.ps1
scripts/task411_guest_position_census.ps1
scripts/task413_patch_protection_ab.ps1
scripts/task414_delay_loop_ab.ps1
```

추가로 두 벤치마크 스크립트는 형태가 다릅니다.

| 스크립트 | 변경 |
|---|---|
| `benchmark_aot_inline_cache.ps1` | `"aot-dynamic"` → `"dynamic"` |
| `benchmark_native_linear_span.ps1` | `ValidateSet("aot-dynamic","aot-dbt")` → `ValidateSet("dynamic")`, 기본값 `"dynamic"`. **이 스크립트의 A/B 의도가 두 backend 비교였다면**, Task 424의 `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH` 축으로 다시 세우고 그 사실을 스크립트 주석과 작업 로그에 남깁니다 |

`$prevBackend` 저장·복원 로직은 그대로 둡니다.

## 5. 문서

| 문서 | 변경 |
|---|---|
| `ARCHITECTURE.md` | backend 절(약 625~637행), "AOT-DBT 실행 정책 기반" 절(약 669~690행), Task 276 backend 열거. `aot-dbt`/`AOT-DBT` 47곳 중 **backend 이름을 가리키는 것만** 바꾸고, 기법·로그 줄 이름은 유지 |
| `ARCHITECTURE.md` (신규 절) | **세 층위 명명 규칙**을 명시 — 설계 §4.4.2 표를 그대로 옮깁니다 |
| `docs/guides/` 6개 파일 | `execution-stall-eip-census.md`(3), `gameplay-scene-capture.md`(2), `glide-setter-elision-testing.md`(2), `port-io-arena-attribution.md`(2), `pumpit3-stall-reproduction.md`(2), `cd-audio-position-census.md`(1) — 절차 문구를 `dynamic`으로 |
| `docs/analysis/aot-execution-backend.md` | 상단에 옛 이름 → 새 이름 대응 주석. **과거 측정 기록은 그대로** |
| `docs/analysis/current-execution-frontier.md` | 같은 방식. 앞으로 수행할 절차 문구만 갱신 |
| `docs/work-logs/` | **손대지 않습니다** (시간순 증거) |

설계 §5를 그대로 적용합니다. analysis·kb 파일을 추가하거나 이름을 바꾸지 않으므로
`README.md` 색인은 대상이 아닙니다.

### 5.1 명명 규칙 절 (필수)

`ARCHITECTURE.md`의 backend 절 바로 뒤에 다음을 넣습니다. `dynamic` backend를
`REPIU_AOT_*` 노브로 조율하는 구조가 처음 보는 사람에게 걸리는 지점이므로,
이 절이 그 seam을 메웁니다.

| 층위 | 이름 | 가리키는 것 |
|---|---|---|
| 실행 정책 | `legacy` / `dynamic` | 사용자가 `REPIU_EXECUTION_BACKEND`로 고르는 노브 |
| 하위 시스템 | `AOT` | 실행 전에 code cache를 계획·생성·배치하는 단계 |
| 번역 계층 | `DBT` | 그 cache 위에서 런타임에 동작하는 번역·dispatch |

`AOT`가 여전히 정확한 이름이라는 근거(`dynamic`에서도 정적 단계가 매 실행 수행됨)를
한 문장으로 함께 적습니다. 설계 §4.4.1을 참조로 겁니다.

## 6. 검증

### 6.1 빌드와 probe

```powershell
cmd /c scripts\build_win32_x86_release.bat
```

`aot_probe` 전체 통과. `execution_backend_policy=true`와
`native_linear_span_policy=true`를 확인합니다.

### 6.2 동등성 — 개명이 동작을 바꾸지 않았음

`REPIU_EXECUTION_BACKEND=dynamic` 1초 smoke를, **Task 424 완료 시점의 `aot-dbt` 1초
smoke**와 비교합니다. `REPIU_EEPROM_PATH`는 실행마다 격리합니다.

| 확인 | 통과 조건 |
|---|---|
| `Win32 execution backend` | `dynamic` |
| guarded segment-load/read enabled/sites | Task 424 값과 동일 |
| direct-edge / timer safe point sites | Task 424 값과 동일 |
| `Win32 AOT generation publishes/quarantines` | 격리 **0** |
| `Win32 AOT generation failure addresses` | **0** |
| dispatch entry/exit | 균형 유지 |

**site 수가 하나라도 다르면 개명이 동작을 바꾼 것입니다.** 원인을 찾기 전까지
진행하지 않습니다.

### 6.3 거부

| 설정 | 통과 조건 |
|---|---|
| `REPIU_EXECUTION_BACKEND=aot-dbt` | exit 1 + `must be legacy or dynamic` |
| `REPIU_EXECUTION_BACKEND=aot` | 동일 |
| `REPIU_EXECUTION_BACKEND=aot-dynamic` | 동일 |

### 6.4 legacy 회귀

`REPIU_EXECUTION_BACKEND`를 **미지정**한 채 1초 smoke를 돌려 legacy 경로가
변경 전과 같이 진행하는지 확인합니다. `use_dynamic_backend`가 거짓일 때
AOT 이미지를 만들지 않는 성질이 유지돼야 합니다.

### 6.5 스크립트 1개 실연

`scripts/task411_guest_position_census.ps1`을 실제로 한 번 돌려, 갱신한 backend 값으로
스크립트가 끝까지 동작하는지 확인합니다. 11개 전부를 돌릴 필요는 없습니다.

**`DurationSeconds`는 45 이상을 씁니다.** Glide 창이 9~10초에 열리므로 그보다 짧으면
`frames <= 1`이 보장되어 분류기가 구조적으로 `stalled`을 냅니다. 이 스크립트에는
`$prevBackend` 복원 로직이 없으므로(설정만 합니다) 복원은 이 실행으로 검증되지
않습니다 — 복원 로직을 가진 스크립트는 문자열 리터럴만 바뀌므로 diff 검토로 갈음합니다.

## 7. 완료 기준

1. §0의 도달 가능성 확인이 끝났고 결론이 작업 로그에 있습니다.
2. `aot_probe` 전체가 통과합니다.
3. §6.2의 site 수가 Task 424 기준선과 **전부 일치**합니다.
4. 옛 이름 세 개가 모두 exit 1로 거부됩니다.
5. legacy 미지정 실행에 회귀가 없습니다.
6. 스크립트 11개와 벤치마크 2개, 가이드 6개, `ARCHITECTURE.md`가 갱신됐고,
   §5.1의 명명 규칙 절이 들어갔습니다.
7. analysis 두 문서에 이름 대응 주석이 들어갔고, 과거 기록은 보존됐습니다.
8. 작업 로그를 남겼습니다.

## 8. 머지

사용자가 머지를 요청하면 `VERSION`을 `0.0.132` → `0.0.133`으로 올리고, 작업 브랜치를
squash해 `main`에 머지한 뒤 `v0.0.133` annotated tag를 붙입니다. tag push는 사용자가
직접 수행합니다.

---

# Task 425 Work Order — consolidate backends to legacy / dynamic and rename

Design: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §4-§6.
Prerequisite: [Task 424](20260805-424-dbt-build-option-toggles.md), which must be complete
first.

## 0. Prerequisite check — before anything else

Deleting the dead branch in §2.3 requires that the AOT dispatcher be **unreachable under
`legacy`**. Follow the code from `AttemptWin32GuestStackAotExecution` through
`ThreadContext::execution_backend` and confirm it. **If it is reachable, cancel the branch
deletion**, keep the condition, proceed with the rest of the consolidation, and record the
finding in the work log. Do not delete without this check.

## 1. Enum and parser

`ExecutionBackend` becomes `{ kLegacy, kDynamic }`; only `"legacy"` and `"dynamic"` parse and
everything else returns false, with **no alias** for `aot`, `aot-dynamic`, or `aot-dbt`. Delete
`ExecutionBackendUsesAot` and `ExecutionBackendUsesImmediateHleReentry` and route their call
sites through `ExecutionBackendUsesDynamicTranslation`.

## 2. Call sites

In `main.cpp`: the error message becomes `must be legacy or dynamic`, `use_aot_backend` is
renamed `use_dynamic_backend`, and the eleven `== kAotDbt` comparisons become that boolean, so
each build option reads `use_dynamic_backend && ResolvePromotedToggle(...)`. **Every Task-number
comment (384 / 386 / 390 / 291 / 424) is preserved**, and the `AOT-DBT` prefix in log strings
**stays** per design §4.4 — it names the technique, and guides and analysis documents cite those
lines by name.

Elsewhere the change is mechanical: `aot_dbt_dispatch.cpp` (predicate swap, keeping the
`hle_reentry_reject_backend` counter and its comment), `aot_dbt_glide_gate_dispatch.cpp`,
`execution_trampoline.cpp`, `native_linear_span.cpp`, `thread_context.h` comments, and
`glide_opengl_backend.cpp` where only the window-title string changes. The
`ResolveNativeLinearSpan*` functions **keep their backend parameter** (design §4.5).

If §0 passed, the `(!dynamic_translation && !retired_target)` disjunct in
`aot_runtime_dispatch.cpp` is removed. Keep the `dynamic_translation` local — it still drives
`aot_dynamic_attempt_count` — and confirm the rewrite does not widen how often
`RequestAotDynamicTranslation` is called.

## 3. Probes

`execution_backend_probe.cpp` shrinks to two rows and **gains explicit rejection cases** for
`aot`, `aot-dynamic`, and `aot-dbt` alongside the existing `unknown` and `nullptr` rejections —
that is where "old names are rejected, not aliased" is pinned. In `native_linear_span_probe.cpp`
the sixteen `kAot`/`kAotDynamic` cases move to `kLegacy`, **keeping the property that non-dynamic
backends default OFF but honour an explicit setting**; merge duplicate counterexamples if they
arise, but never drop a property just because the row count shrank.

## 4. Eleven scripts

Replace `"aot-dbt"` with `"dynamic"` in the ten `task*.ps1` scripts and
`benchmark_native_linear_span.ps1`; change `benchmark_aot_inline_cache.ps1` from
`"aot-dynamic"` to `"dynamic"`; and reduce the `ValidateSet` in
`benchmark_native_linear_span.ps1` to `dynamic`. **If that script's A/B intent was a comparison
between the two backends**, re-found it on Task 424's `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH` axis
and note that in the script comment and the work log. Leave the `$prevBackend` save/restore
logic alone.

## 5. Documentation

Update `ARCHITECTURE.md`'s backend section, its AOT-DBT policy section, and the Task 276
enumeration — changing only the 47 `aot-dbt`/`AOT-DBT` occurrences that name **the backend**,
not those naming the technique or a log line. **Add a new naming-layers section** right after
the backend section, carrying design §4.4.2's table verbatim — `legacy`/`dynamic` name the
execution policy, `AOT` the ahead-of-time cache subsystem, `DBT` the runtime translation layer
— plus one sentence on why `AOT` remains accurate (the static stage runs on every `dynamic`
execution), linked to design §4.4.1. This section is what closes the seam of a `dynamic` backend
tuned through `REPIU_AOT_*` knobs. Update the six guides
(`execution-stall-eip-census`, `gameplay-scene-capture`, `glide-setter-elision-testing`,
`port-io-arena-attribution`, `pumpit3-stall-reproduction`, `cd-audio-position-census`). Add an
old-to-new name note atop `aot-execution-backend.md` and `current-execution-frontier.md`,
**leaving their historical measurements intact**, and do not touch `docs/work-logs/`. No
analysis or kb file is added or renamed, so no `README.md` index update applies.

## 6. Verification

Build Release and pass the full `aot_probe`, including `execution_backend_policy=true` and
`native_linear_span_policy=true`.

**Equivalence is the decisive check:** a one-second `dynamic` smoke must match the `aot-dbt`
smoke taken at the end of Task 424 on the guarded segment-load/read counts, the direct-edge and
timer-safe-point site counts, zero quarantines, zero generation-failure addresses, and balanced
dispatch entry/exit, with `Win32 execution backend` now reading `dynamic` and a per-run isolated
`REPIU_EEPROM_PATH`. **Any differing site count means the rename changed behaviour**; stop and
find the cause before continuing.

Then confirm all three old names exit 1 with the new message, that an unset run still takes the
legacy path with no AOT image built, and run `task411_guest_position_census.ps1` once end to end
— that single script exercises both the new value and the `$prevBackend` restore.

## 7. Completion

The §0 finding is recorded; the probe passes; every site count matches the Task 424 baseline;
all three old names are rejected; legacy shows no regression; eleven scripts, two benchmarks,
six guides, and `ARCHITECTURE.md` are updated with the §5.1 naming-layers section present; the
two analysis documents carry the name note with their history preserved; and the work log is
written.

## 8. Merge

On the user's merge request, bump `VERSION` from `0.0.132` to `0.0.133`, squash the branch into
`main`, and apply an annotated `v0.0.133` tag locally; the user pushes the tag.
