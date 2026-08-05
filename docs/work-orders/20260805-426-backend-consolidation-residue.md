# Task 426 작업 지시 — backend 축소 후 남은 죽은 코드 정리

설계: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §4 ·
선행: [Task 425](20260805-425-execution-backend-consolidation.md)

Task 425는 backend를 `legacy`와 `dynamic` 둘로 줄였지만, `aot`와 `aot-dynamic`에서만
성립하던 조건 세 곳을 남겨 두었습니다. 셋 다 이제 **구조적으로 도달 불가**입니다.

## 0. 근거 — 왜 도달 불가인가

Task 425 §0에서 확인한 사실이 전제입니다. `execution_trampoline.cpp`의 비 AOT 진입점
4개가 호출 지점에서 `aot_placement = nullptr`과 `kLegacy`를 하드코딩하므로,
**`aot_placement != nullptr`이면 backend는 반드시 `dynamic`입니다.**

## 1. `hle_reentry_reject_backend` 제거

`TryResumeAotAfterHandledHle`는 `aot_placement == nullptr`이면 먼저 반환합니다.
따라서 그 뒤의 backend 검사는 절대 성립하지 않고, 카운터는 영구히 0입니다.
거부 사유를 설명한다는 funnel 버킷이 구조적으로 0만 낼 수 있으면 읽는 사람을
오도하므로 제거합니다.

| 파일 | 변경 |
|---|---|
| `src/platform/win32/aot/aot_dbt_dispatch.cpp` | backend 검사 블록 제거. Task 340 주석은 나머지 가드에 맞게 유지 |
| `src/platform/win32/execution/thread_context.h` | 필드 제거 |
| `include/repiu/platform/win32/execution_trampoline.h` | 필드 제거 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 대입 제거 |
| `src/host/win32/main.cpp` | 합계 항과 로그 형식에서 제거 |

**로그 형식이 8필드에서 7필드로 바뀝니다.** 이것이 이 작업의 유일한 외부 영향입니다.

```
이전: not-pending/backend/segment-write/outside-arena/quarantined/span-unsafe/success/total
이후: not-pending/segment-write/outside-arena/quarantined/span-unsafe/success/total
```

## 2. `task347` 파싱 갱신

`scripts/task347_release_axis_reattribution.ps1`이 이 줄을 정규식으로 읽습니다.
`(\d+)` 하나를 빼고 **group 번호를 함께 내립니다.**

| 값 | 이전 group | 이후 group |
|---|---:|---:|
| span-unsafe | 6 | 5 |
| success | 7 | 6 |
| total | 8 | 7 |

group 번호를 안 고치면 조용히 잘못된 값을 읽습니다. 정규식만 고치는 실수를
하지 않도록 **두 곳을 같은 편집에서 처리합니다.**

## 3. 항진 조건 제거

`src/platform/win32/aot/aot_runtime_dispatch.cpp`의 `dynamic_translation`은 이제 항상
참입니다. `if (dynamic_translation)` 가드를 없애고 `aot_dynamic_attempt_count` 증가를
무조건 수행합니다. **증가 횟수는 바뀌지 않습니다** — 조건이 항상 참이었으므로 매번
증가했습니다. 지역 변수는 다른 용도가 없으면 함께 제거합니다.

## 4. 중복 검사 제거

`src/platform/win32/aot/aot_dbt_glide_gate_dispatch.cpp`의 backend 검사는 바로 다음
줄 `!context->aot_dbt_glide_direct_dispatch`에 포함됩니다. 그 플래그는
`execution_trampoline.cpp`에서 `ExecutionBackendUsesDynamicTranslation`이 참일 때만
켜집니다. backend 검사만 지우고 플래그 검사는 남깁니다.

## 5. 건드리지 않을 것

`execution_trampoline.cpp`의 `NativeLinearSpanEnabled(context->execution_backend)`
두 곳(약 1363행·3061행)은 **살아 있습니다.** VEH는 legacy 실행에서도 돌고, 그때
resolver가 설정값 기반으로 답합니다. `native_linear_span.h`의 `Resolve*` backend
인자도 이 때문에 유지합니다.

## 6. 검증

| # | 확인 | 통과 조건 |
|---:|---|---|
| 1 | Release 빌드 | exit 0 |
| 2 | `aot_probe` 전체 | 전 항목 통과 |
| 3 | `dynamic` 1초 smoke | funnel 줄이 **7필드**로 출력되고 total이 나머지 합과 일치 |
| 4 | 동등성 | Task 425 표의 site 수와 런타임 카운터가 그대로 |
| 5 | funnel 값 보존 | `backend`를 뺀 나머지 6개 값이 Task 425 실행과 같은 분포 |
| 6 | `task347` 파싱 | 스크립트가 funnel을 읽고 끝까지 동작 |

3번은 **합계 검산이 핵심입니다.** 필드를 빼면서 합계 항을 빼먹으면 total이 어긋납니다.

## 7. 완료 기준

1. 세 조건이 모두 제거됐고 §5의 두 곳은 남아 있습니다.
2. funnel 로그가 7필드이고 합계가 맞습니다.
3. `task347`이 새 형식을 정확한 group 번호로 읽습니다.
4. Task 425 동등성 표가 유지됩니다.
5. `ARCHITECTURE.md`의 funnel 서술과 작업 로그를 갱신했습니다.

---

# Task 426 Work Order — clearing the residue left by the backend consolidation

Design: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §4.
Prerequisite: [Task 425](20260805-425-execution-backend-consolidation.md).

Task 425 reduced the backends to `legacy` and `dynamic` but left three conditions that held
only for `aot` and `aot-dynamic`. All three are now **structurally unreachable**, because the
four non-AOT entry points in `execution_trampoline.cpp` hard-code a null placement and
`kLegacy` at the call site — so a non-null `aot_placement` implies the `dynamic` backend.

## 1. Remove `hle_reentry_reject_backend`

`TryResumeAotAfterHandledHle` returns early on a null placement, so the backend check after it
can never fire and the counter is permanently zero. A funnel bucket that claims to explain
rejections while being structurally incapable of counting any is misleading, so the guard, the
two struct fields, the telemetry assignment, and the log field all go. **The log format drops
from eight fields to seven** — the only external effect of this task.

## 2. Update `task347` parsing

`scripts/task347_release_axis_reattribution.ps1` reads that line by regex. Remove one `(\d+)`
**and renumber the groups in the same edit**: span-unsafe 6→5, success 7→6, total 8→7. Fixing
only the regex would silently read the wrong values.

## 3-4. Remove the tautology and the redundant check

In `aot_runtime_dispatch.cpp`, `dynamic_translation` is always true, so the `if` around the
`aot_dynamic_attempt_count` increment goes and the increment becomes unconditional — the count
is unchanged, since the condition already held on every call. Drop the local if nothing else
uses it. In `aot_dbt_glide_gate_dispatch.cpp`, drop the backend check but keep
`!context->aot_dbt_glide_direct_dispatch`, which subsumes it because the trampoline sets that
flag only when `ExecutionBackendUsesDynamicTranslation` holds.

## 5. Leave alone

The two `NativeLinearSpanEnabled(context->execution_backend)` sites in the trampoline are
**live**: the VEH runs under legacy too, where the resolver answers from the setting. The
backend parameter on the `native_linear_span.h` resolvers stays for the same reason.

## 6-7. Verification and completion

Release build and full `aot_probe` pass; a one-second `dynamic` smoke prints the funnel with
**seven** fields whose total equals the sum of the rest — the decisive check, since dropping a
field without dropping its summand would break the total; the Task 425 equivalence table still
holds; the six surviving funnel values keep their distribution; and `task347` parses the new
format with the corrected group numbers. Done when all three conditions are gone, the two §5
sites remain, and `ARCHITECTURE.md` plus the work log are updated.
