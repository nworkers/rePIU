# Task 425 작업 로그 — backend를 legacy / dynamic으로 축소하고 개명

설계: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) ·
지시: [20260805-425](../work-orders/20260805-425-execution-backend-consolidation.md) ·
선행: [Task 424](20260805-424-dbt-build-option-toggles.md)

## 1. 선결 확인 (지시 §0) — 통과

AOT dispatcher가 `legacy`에서 도달 불가능함을 코드로 확인했습니다.

* `execution_trampoline.cpp`의 비 AOT 진입점 4개(`RunWin32ExecutionThread` 호출부
  4592·4633·4679·4721)는 **호출 지점에서** `aot_placement = nullptr`과
  `ExecutionBackend::kLegacy`를 하드코딩합니다. 사용자가 고른 backend 값이 전달되지
  않습니다.
* `AttemptWin32GuestStackAotExecution`만 실제 placement와 사용자 backend를 넘기고,
  `aot_placement.placed`가 거짓이면 조기 반환합니다.
* `main.cpp`는 그 진입점을 `use_dynamic_backend`일 때만 선택합니다.

따라서 `context->execution_backend == kLegacy`인 경우가 곧 `aot_placement == nullptr`인
경우이며, AOT dispatcher는 전부 `aot_placement != nullptr`를 전제로 가드돼 있습니다.
지시 §2.3의 죽은 분기를 삭제할 조건이 충족됐습니다.

## 2. 한 일

* `ExecutionBackend`를 `{ kLegacy, kDynamic }`으로 줄이고 `legacy`·`dynamic`만
  파싱합니다. 옛 이름 셋은 별칭 없이 거부합니다.
* `ExecutionBackendUsesAot`와 `ExecutionBackendUsesImmediateHleReentry`를 삭제하고
  `ExecutionBackendUsesDynamicTranslation` 하나로 통일했습니다.
* `main.cpp`의 `== kAotDbt` 11곳을 `use_dynamic_backend`로 치환하고 에러 문구를
  갱신했습니다. Task 384/386/390/291/424 주석은 모두 보존했습니다.
* `aot_runtime_dispatch.cpp`에서 정적 전용 `aot` backend를 위해 있던
  `(!dynamic_translation && !retired_target)` 항을 제거했습니다. `dynamic_translation`이
  항상 참이므로 이전 식도 `RequestAotDynamicTranslation`을 매번 호출했고, 호출 빈도는
  바뀌지 않습니다. 지역 변수는 `aot_dynamic_attempt_count`에 계속 쓰이므로 남겼습니다.
* probe 두 개를 갱신했습니다. `execution_backend_probe`는 두 행으로 줄이고 옛 이름
  셋의 **거부**를 명시 케이스로 고정했습니다. `native_linear_span_probe`의 반례
  backend는 `kLegacy`로 옮기되 "dynamic이 아닌 backend는 미지정 OFF, 명시 지정이면
  ON"이라는 성질은 유지했고, 긍정 철자 세 가지를 모두 legacy에서 확인합니다.
* 스크립트 12개, 가이드 6개, `ARCHITECTURE.md`를 갱신하고 명명 규칙 절을 넣었습니다.
* `docs/analysis/` 두 문서에 이름 대응 주석을 넣고 과거 측정 기록은 보존했습니다.

## 3. 개명하지 않은 것

설계 §4.4대로 backend 식별자만 바꿨습니다. `REPIU_AOT_DBT_*` env 8개, `aot_dbt_*.cpp`
24개, `enable_dbt_*` 필드, `ExecutionTimeBucket::kAotDynamicTranslate`,
`kAotDbt*FallbackReasonCount` 상수, 로그 줄의 `AOT-DBT` 접두사는 그대로입니다.

`ARCHITECTURE.md`에서는 backtick으로 감싼 backend 이름만 치환했습니다. 파일 경로
`docs/design/20260724-280-aot-dbt-four-stage-roadmap.md`가 깨지지 않게 하기 위해서입니다.
Task 277 A/B 서술처럼 제거된 backend를 **대조군으로 인용한 과거 기록**은 이름을 남기고
"Task 425에서 제거"를 덧붙였습니다. 이름만 바꾸면 두 backend를 비교했다는 사실이
사라지기 때문입니다.

## 4. 검증

| # | 확인 | 결과 |
|---:|---|---|
| 1 | Release 빌드 | exit 0 |
| 2 | `aot_probe` 전체 | exit 0. `execution_backend_policy=true`, `linear_span_policy=true`, `env_toggle_policy=true` 포함 전 항목 |
| 3 | `dynamic` 1초 smoke 동등성 | 아래 표 — **전 항목 일치** |
| 4 | 옛 이름 거부 | `aot-dbt`/`aot`/`aot-dynamic`/`bogus` 모두 exit 1 + `must be legacy or dynamic` |
| 5 | legacy (미지정) | exit 0, AOT 이미지 미생성, 구조 동일 |
| 6 | 스크립트 실연 | `task411` 45초 1회 **healthy**, frames 1170 |

### 4.1 동등성 (지시 §6.2)

Task 424 완료 시점 `aot-dbt` 실행과 비교했습니다. 두 실행 모두 arena base
`0x03000000`, EEPROM 격리.

| 항목 | Task 424 (`aot-dbt`) | Task 425 (`dynamic`) |
|---|---|---|
| indirect inline-cache slots | 4 | 4 |
| guarded segment-pop enabled | true | true |
| guarded segment-load enabled/sites | true/54 | true/54 |
| guarded segment-read enabled/sites | true/43 | true/43 |
| return-miss dispatch enabled | true | true |
| direct-edge dispatch enabled/sites | true/10 | true/10 |
| superblock HLE dispatch | false | false |
| Port-I/O dispatch | true | true |
| segment-override dispatch | false | false |
| Glide gate direct dispatch | true | true |
| timer safe points enabled/sites | true/550 | true/550 |
| segment-pop success/fallback | 36/4 | 36/4 |
| segment-load success/fallback | 8/5 | 8/5 |
| generation publishes/quarantines | 0/0 | 0/0 |
| generation failure addresses | 0 | 0 |

site 수와 런타임 카운터가 모두 같으므로 개명이 동작을 바꾸지 않았습니다.

### 4.2 legacy 회귀 (지시 §6.4)

`REPIU_EXECUTION_BACKEND` 미지정 1초 pumpit3 실행에서 backend가 `legacy`로 찍히고
AOT 관련 줄이 하나도 출력되지 않습니다 — `use_dynamic_backend`가 거짓일 때 이미지를
만들지 않는 성질이 유지됩니다.

예외 dispatch 수는 baseline 62,897 대 변경 후 60,989였습니다. **이 값은 1초 wall-clock
동안 몇 번 single-step했는지이므로 실행마다 달라집니다.** 결정적 값이 아니며, 같은
자릿수와 동일한 구조(`AOT-DBT HLE host dispatch 0/0/0/0`)가 판정 기준입니다.

### 4.3 지시 §6.5의 오류 정정

작업 지시는 `task411`로 `$prevBackend` 복원이 검증된다고 적었지만, **이 스크립트에는
복원 로직이 없습니다**(설정만 합니다). 지시를 정정했습니다. 복원 로직을 가진
스크립트들은 문자열 리터럴만 바뀌므로 diff 검토로 갈음합니다.

또한 `DurationSeconds=5`로 처음 돌렸을 때 `stalled`로 분류됐는데, 이는 회귀가 아니라
**분류기의 구조적 결과**입니다. Glide 창이 9~10초에 열리므로 5초 실행은 `frames <= 1`이
보장되고, 분류기는 `frames <= 1 && traces <= 6`을 `stalled`로 봅니다. 45초로 다시 돌리니
`healthy`, frames 1170이었습니다. 지시에 45초 이상을 쓰라는 조건을 추가했습니다.

baseline 바이너리로 같은 스크립트를 돌리는 A/B는 성립하지 않습니다 — 갱신된 스크립트가
`dynamic`을 넘기는데 baseline 바이너리는 그 값을 거부합니다. 개명이 스크립트에 실제로
반영됐다는 확인은 됩니다.

## 5. 남은 것

* 사용자가 머지를 요청하면 `VERSION`을 `0.0.133`으로 올리고 squash 머지 후
  `v0.0.133` annotated tag를 붙입니다.
* A/B용으로 만든 `../rePIU-baseline` worktree는 baseline 바이너리가 `dynamic`을
  거부하므로 이후 비교에는 쓸 수 없습니다. 정리 대상입니다.

---

# Task 425 Work Log — consolidating backends to legacy / dynamic and renaming

Design: [20260805-424](../design/20260805-424-execution-backend-consolidation.md). Work order:
[20260805-425](../work-orders/20260805-425-execution-backend-consolidation.md). Prerequisite:
[Task 424](20260805-424-dbt-build-option-toggles.md).

## 1. Prerequisite check (§0) — passed

The AOT dispatcher is unreachable under `legacy`. The four non-AOT entry points in
`execution_trampoline.cpp` hard-code `aot_placement = nullptr` and `ExecutionBackend::kLegacy`
**at the call site**, so the user's backend value never reaches them; only
`AttemptWin32GuestStackAotExecution` passes a real placement and the selected backend, and it
returns early unless `aot_placement.placed`; and `main.cpp` selects that entry point only when
`use_dynamic_backend`. So `execution_backend == kLegacy` coincides exactly with
`aot_placement == nullptr`, and every AOT dispatcher guards on a non-null placement. The dead
branch in §2.3 could therefore be deleted.

## 2. What was done

The enum became `{ kLegacy, kDynamic }` with only `legacy` and `dynamic` parsing and the three
old names rejected without aliases; `ExecutionBackendUsesAot` and
`ExecutionBackendUsesImmediateHleReentry` collapsed into
`ExecutionBackendUsesDynamicTranslation`; the eleven `== kAotDbt` comparisons in `main.cpp`
became `use_dynamic_backend` with every Task-number comment preserved; and the
`(!dynamic_translation && !retired_target)` disjunct — present only for the static-only `aot`
backend — was removed, leaving call frequency unchanged because `dynamic_translation` is always
true there and the request already ran on every call. The local stays, since it still drives
`aot_dynamic_attempt_count`.

Both probes were updated: `execution_backend_probe` shrank to two rows and pins the **rejection**
of all three old names as explicit cases, while `native_linear_span_probe` moved its
counterexample backend to `kLegacy`, keeping the property that a non-dynamic backend defaults
OFF but honours an explicit setting and now covering all three affirmative spellings there.
Twelve scripts, six guides, and `ARCHITECTURE.md` were updated, the naming-layers section added,
and both cumulative analysis documents gained a name-mapping note with their measurements intact.

## 3. What was not renamed

Per design §4.4 only the backend identifier changed. The eight `REPIU_AOT_DBT_*` variables, the
twenty-four `aot_dbt_*` sources, the `enable_dbt_*` fields, `kAotDynamicTranslate`, the
`kAotDbt*FallbackReasonCount` constants, and the `AOT-DBT` log prefixes all stand. In
`ARCHITECTURE.md` only backtick-quoted backend names were replaced, so the file path
`docs/design/20260724-280-aot-dbt-four-stage-roadmap.md` stayed intact, and historical passages
citing a removed backend as an **A/B contrast arm** kept their names with "removed in Task 425"
appended — renaming them would erase the fact that two backends were compared.

## 4. Verification

Release build exit 0. Full `aot_probe` exit 0 including `execution_backend_policy`,
`linear_span_policy`, and `env_toggle_policy` all true. The one-second `dynamic` smoke matched
the Task 424 `aot-dbt` baseline on **every** line — inline-cache slots 4, guarded pop/load/read
`true`, `true/54`, `true/43`, return-miss `true`, direct-edge `true/10`, superblock and
segment-override `false`, Port-I/O and Glide-gate-direct `true`, timer safe points `true/550`,
segment-pop `36/4`, segment-load `8/5`, generation publishes/quarantines `0/0`, generation
failure addresses `0` — with both runs at arena base `0x03000000`. `aot-dbt`, `aot`,
`aot-dynamic`, and `bogus` all exit 1 with `must be legacy or dynamic`. An unset run takes the
legacy path with no AOT lines emitted at all.

Legacy exception-dispatch counts were 62,897 (baseline) against 60,989 (after). **That figure
counts single-steps taken within one wall-clock second and varies per run**; the verdict rests
on the identical structure, not on equality.

### 4.1 Two corrections to the work order

§6.5 claimed `task411` verifies `$prevBackend` restoration, but **that script has no restore
logic** — it only sets the variable. The order has been corrected; scripts that do restore
changed only a string literal and are covered by diff review.

The first `task411` run, at `DurationSeconds=5`, classified as `stalled`. That is a structural
property of the classifier, not a regression: the Glide window opens at 9-10 s, so a five-second
run is guaranteed `frames <= 1`, which with `traces <= 6` is the stalled signature. Rerun at 45
seconds it reported **healthy** with 1,170 frames. The order now requires 45 seconds or more.
Running the same script against the baseline binary is not a valid A/B, since the updated script
passes `dynamic` and the baseline rejects it — which does confirm the rename reached the scripts.

## 5. Remaining

On a merge request, bump `VERSION` to `0.0.133`, squash into `main`, and tag `v0.0.133`
locally. The `../rePIU-baseline` worktree is no longer usable for comparison, because the
baseline binary rejects `dynamic`; it can be removed.
