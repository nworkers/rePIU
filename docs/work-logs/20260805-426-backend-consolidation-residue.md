# Task 426 작업 로그 — backend 축소 후 남은 죽은 코드 정리

설계: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §4 ·
지시: [20260805-426](../work-orders/20260805-426-backend-consolidation-residue.md) ·
선행: [Task 425](20260805-425-execution-backend-consolidation.md)

## 1. 왜 이 작업이 생겼는가

Task 425는 backend를 둘로 줄이면서 **`aot`와 `aot-dynamic`에서만 성립하던 조건 세
곳을 남겼습니다.** Task 425 작업 지시가 죽은 분기 하나만 지목했고, 나머지를 훑지
않았기 때문입니다. 셋 다 Task 425 §0에서 확인한 사실 — `aot_placement != nullptr`이면
backend는 반드시 `dynamic` — 때문에 구조적으로 도달 불가입니다.

## 2. 한 일

### 2.1 `hle_reentry_reject_backend` 제거

`TryResumeAotAfterHandledHle`는 `aot_placement == nullptr`이면 먼저 반환하므로,
그 뒤의 backend 검사는 절대 성립할 수 없고 카운터는 영구히 0이었습니다. 거부 사유를
설명한다는 funnel 버킷이 구조적으로 0만 낼 수 있으면 읽는 사람을 오도합니다.

가드, `ThreadContext`와 `Win32MinimalExecutionAttempt`의 필드, telemetry 대입,
로그 필드와 합계 항을 모두 제거했습니다. **funnel 로그가 8필드에서 7필드로
바뀝니다.**

### 2.2 부수 효과 — 두 로그 줄의 불일치가 해소됐습니다

count 줄과 share 줄이 서로 다른 필드 집합을 쓰고 있었습니다.

```
Task 425까지
  count: not-pending/backend/segment-write/outside-arena/quarantined/span-unsafe/success/total
  share: not-pending/segment-write/outside-arena/quarantined/span-unsafe/success
```

share 줄에는 원래 `backend`가 없었습니다. 이제 두 줄의 버킷 집합이 같아집니다.
의도한 변경은 아니지만 기록해 둘 만한 개선입니다.

### 2.3 항진 조건과 중복 검사 제거

* `aot_runtime_dispatch.cpp`의 `dynamic_translation` 지역 변수와 이를 감싸던
  `if`를 제거하고 `aot_dynamic_attempt_count` 증가를 무조건 수행합니다. 조건이 항상
  참이었으므로 **증가 횟수는 바뀌지 않습니다.**
* `aot_dbt_glide_gate_dispatch.cpp`의 backend 검사를 제거했습니다. 바로 다음 줄
  `!context->aot_dbt_glide_direct_dispatch`가 이를 포함합니다 — 그 플래그는
  trampoline이 `ExecutionBackendUsesDynamicTranslation`이 참일 때만 켭니다.

### 2.4 `task347` 파싱 갱신

정규식에서 `(\d+)` 하나를 빼고 **group 번호를 함께 내렸습니다**(span-unsafe 6→5,
success 7→6, total 8→7). 정규식만 고치면 조용히 잘못된 값을 읽으므로 같은 편집에서
처리했습니다.

## 3. 건드리지 않은 것

`execution_trampoline.cpp`의 `NativeLinearSpanEnabled(context->execution_backend)`
두 곳은 **살아 있습니다.** VEH는 legacy 실행에서도 돌고, 그때 backend가 `kLegacy`라
resolver가 설정값 기반으로 답합니다. `native_linear_span.h`의 `Resolve*` backend
인자도 같은 이유로 유지했습니다. 설계 §4.5의 판단이 여기서 확인됐습니다.

`ARCHITECTURE.md`에는 funnel 형식 서술이 없어 갱신 대상이 아니었습니다. 지시 §7의
해당 항목은 공전입니다. `docs/analysis/`와 `docs/design/`의 funnel 인용은
`... quarantined/success:` 형태로 필드 위치에 의존하지 않아 그대로 유효합니다.
`docs/guides/`에는 이 줄을 읽는 절차가 없습니다.

## 4. 검증

| # | 확인 | 결과 |
|---:|---|---|
| 1 | Release 빌드 | exit 0 |
| 2 | `aot_probe` 전체 | exit 0, `_policy=false` 없음 |
| 3 | funnel 7필드 + 합계 검산 | `0/0/0/0/0/45057/45057` — 6개 버킷 합 45,057 == total |
| 4 | 동등성 | 아래 표 |
| 5 | `task347` group 번호 | 실제 기록 데이터로 확인 (§4.2) |
| 6 | `task347` 완주 | 45초 1회 exit 0, frames 2011 |

### 4.1 동등성 (Task 425 실행과 비교, 둘 다 arena base `0x03000000`)

| 항목 | Task 425 | Task 426 |
|---|---|---|
| guarded segment-load enabled/sites | true/54 | true/54 |
| guarded segment-read enabled/sites | true/43 | true/43 |
| direct-edge dispatch enabled/sites | true/10 | true/10 |
| timer safe points enabled/sites | true/550 | true/550 |
| segment-pop success/fallback | 36/4 | 36/4 |
| segment-load success/fallback | 8/5 | 8/5 |
| generation publishes/quarantines | 0/0 | 0/0 |
| funnel 필드 수 | 8 | **7** |
| funnel `backend` 버킷 | **0** | (제거) |

Task 425 실행의 `backend` 버킷이 실제로 `0`이었다는 것이 로그에 남아 있어, 제거
근거가 측정으로도 확인됩니다.

`hle_reentry_success`는 Task 425의 32,079 대 Task 426의 45,057이었습니다. **이 값은
1초 wall-clock 동안 몇 번 재진입했는지이므로 실행마다 달라집니다** — legacy 예외
카운트와 같은 성질이며, 결정적 값은 위 표대로 전부 일치합니다.

### 4.2 `task347` group 번호 — 실제 기록으로 확인

45초 실행의 원본 로그 줄과 스크립트가 CSV에 기록한 값을 대조했습니다.

```
로그:   ...span-unsafe/success/total: 0/0/0/0/0/6968/6968
기록:   reentry_span_unsafe=0  reentry_success=6968  reentry_total=6968
```

group 5·6·7이 정확히 대응합니다. group 번호를 안 고쳤다면 `span_unsafe`가 0 대신
6968을, `total`이 범위 밖을 읽었을 것입니다.

## 5. 남은 것

* 사용자가 머지를 요청하면 `VERSION`을 `0.0.133`으로 올리고 squash 머지 후
  `v0.0.133` annotated tag를 붙입니다.
* `../rePIU-baseline` worktree는 정리 대상입니다.

---

# Task 426 Work Log — clearing the residue left by the backend consolidation

Design: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §4. Work
order: [20260805-426](../work-orders/20260805-426-backend-consolidation-residue.md).
Prerequisite: [Task 425](20260805-425-execution-backend-consolidation.md).

## 1. Why this task exists

Task 425 shrank the backend set but **left three conditions that held only for `aot` and
`aot-dynamic`**, because its work order named only one dead branch and no sweep was made for
the rest. All three are structurally unreachable given Task 425 §0's finding that a non-null
`aot_placement` implies the `dynamic` backend.

## 2. What was done

`TryResumeAotAfterHandledHle` returns early on a null placement, so the backend guard after it
could never fire and `hle_reentry_reject_backend` was permanently zero — a funnel bucket that
claims to explain rejections while structurally unable to count any. The guard, both struct
fields, the telemetry assignment, and the log field and summand are gone, taking the funnel
line **from eight fields to seven**.

That exposed and fixed an inconsistency: the count line carried a `backend` bucket the
companion share line never had, so the two lines named different bucket sets. They now agree.

The `dynamic_translation` local and its `if` in `aot_runtime_dispatch.cpp` are gone, making the
`aot_dynamic_attempt_count` increment unconditional — the count is unchanged, since the
condition always held. The redundant backend check in `aot_dbt_glide_gate_dispatch.cpp` is gone
too, subsumed by `!context->aot_dbt_glide_direct_dispatch`, which the trampoline sets only when
the backend uses dynamic translation.

`task347`'s regex lost one `(\d+)` **and its group numbers came down in the same edit**
(span-unsafe 6→5, success 7→6, total 8→7); fixing only the regex would have silently read the
wrong values.

## 3. What was left alone

The two `NativeLinearSpanEnabled(context->execution_backend)` sites in the trampoline are
**live** — the VEH runs under legacy, where the resolver answers from the setting — so the
backend parameter on the `native_linear_span.h` resolvers stays, confirming design §4.5.
`ARCHITECTURE.md` describes no funnel format, so that work-order item was vacuous; the
`docs/analysis/` and `docs/design/` citations use the position-independent
`… quarantined/success:` form and remain valid; and no guide reads this line.

## 4. Verification

Release build exit 0 and full `aot_probe` exit 0 with no `_policy=false`. The funnel prints
seven fields, `0/0/0/0/0/45057/45057`, whose six buckets sum exactly to the total. Against the
Task 425 run at the same arena base, every deterministic value matched — guarded segment-load
`true/54`, segment-read `true/43`, direct-edge `true/10`, timer safe points `true/550`,
segment-pop `36/4`, segment-load `8/5`, generation `0/0` — and Task 425's log preserves the
`backend` bucket reading **0**, so the removal is justified by measurement as well as by
structure. `hle_reentry_success` differed (32,079 against 45,057) because it counts re-entries
within one wall-clock second and varies per run.

The group renumbering was checked against real recorded data rather than by inspection: a
45-second run logged `…span-unsafe/success/total: 0/0/0/0/0/6968/6968` and the script wrote
`reentry_span_unsafe=0`, `reentry_success=6968`, `reentry_total=6968`. Left unrenumbered,
`span_unsafe` would have read 6968 and `total` would have run off the end. `task347` itself
completed at 45 seconds with 2,011 frames.

## 5. Remaining

On a merge request, bump `VERSION` to `0.0.133`, squash into `main`, and tag `v0.0.133`
locally. The `../rePIU-baseline` worktree can be removed.
