# Task 427 작업 로그 — 호출자 없는 실행 진입점 2개 삭제

지시: [20260805-427](../work-orders/20260805-427-unreferenced-execution-entry-points.md) ·
발견 경위: [Task 426](20260805-426-backend-consolidation-residue.md) 사후 조사

## 1. 한 일

`AttemptWin32MinimalExecution`과 `AttemptWin32GuestStackExecution`의 선언과 정의를
제거했습니다. 그 외에는 아무것도 바꾸지 않았습니다.

**backend 축소와 무관한 코드입니다.** `git log -S`로 확인한 도입 시점이
`58db6f2`·`baa89f4`이고 마지막 변경이 `d1673e2`(Task 233, v0.0.60)이므로, 초기
bring-up 진입점이 상위 경로가 발전하면서 호출자를 잃은 것입니다. Tasks 424~426이
만든 죽은 코드가 아닙니다.

## 2. 어떻게 죽은 코드라고 판정했는가

두 방향으로 확인했습니다.

1. **정적 검색:** `src`, `include`, `tests`, `tools` 전체에서 각 이름의 참조가
   선언 1건과 정의 1건뿐이었습니다.
2. **링커:** 삭제 후 Release 빌드가 exit 0으로 통과했습니다. 어딘가 호출자가
   있었다면 링크 오류로 드러납니다.

같은 조사에서 **소스 파일과 헤더는 삭제 대상이 없음**도 확인했습니다.
`src/` 아래 모든 `.cpp`가 `CMakeLists.txt`에 등재돼 있고, `src/`·`include/` 아래
모든 `.h`가 최소 한 곳에서 `#include`됩니다.

## 3. 검증

| # | 확인 | 결과 |
|---:|---|---|
| 1 | Release 빌드 | exit 0 |
| 2 | `aot_probe` 전체 | exit 0, `_policy=false` 없음 |
| 3 | `dynamic` 1초 smoke | Task 426 값 유지 (아래) |
| 4 | legacy 미지정 smoke | exit 0, 정상 진행 |

### 3.1 `dynamic` 동등성 (Task 426과 비교, 둘 다 arena base `0x03000000`)

| 항목 | Task 426 | Task 427 |
|---|---|---|
| guarded segment-load enabled/sites | true/54 | true/54 |
| guarded segment-read enabled/sites | true/43 | true/43 |
| direct-edge dispatch enabled/sites | true/10 | true/10 |
| timer safe points enabled/sites | true/550 | true/550 |
| funnel 필드 수 | 7 | 7 |
| segment-pop success/fallback | 36/4 | 36/4 |
| segment-load success/fallback | 8/5 | 8/5 |
| generation publishes/quarantines | 0/0 | 0/0 |

### 3.2 legacy 회귀 — 이번 작업에서 가장 중요한 확인

삭제한 두 함수가 **legacy 래퍼**였으므로 legacy 경로를 직접 확인했습니다.
`REPIU_EXECUTION_BACKEND` 미지정 1초 pumpit3 실행이 exit 0으로 끝나고 backend가
`legacy`로 찍혔습니다. 예외 dispatch 수는 61,666으로, 최근 측정값 60,989·62,897과
같은 범위입니다. **이 값은 1초 wall-clock 동안의 single-step 횟수라 실행마다
달라지므로**, 판정은 범위와 정상 종료로 합니다.

---

# Task 427 Work Log — deleting two unreferenced execution entry points

Work order: [20260805-427](../work-orders/20260805-427-unreferenced-execution-entry-points.md).
Surfaced by the [Task 426](20260805-426-backend-consolidation-residue.md) follow-up sweep.

## 1-2. What was done and how deadness was established

Removed the declarations and definitions of `AttemptWin32MinimalExecution` and
`AttemptWin32GuestStackExecution`, and nothing else. **This is unrelated to the backend
consolidation**: `git log -S` places their introduction at `58db6f2` and `baa89f4`, last touched
by `d1673e2` (Task 233, v0.0.60) — early bring-up entry points that lost their callers as the
paths above them matured.

Deadness was established two ways: a static search across `src`, `include`, `tests`, and `tools`
found only one declaration and one definition per name, and the Release build linked cleanly
afterwards, which any surviving caller would have broken. The same sweep confirmed **no source
file or header is deletable** — every `.cpp` under `src/` is listed in `CMakeLists.txt`, and
every `.h` under `src/` and `include/` is included somewhere.

## 3. Verification

Release build exit 0; full `aot_probe` exit 0 with no `_policy=false`; the one-second `dynamic`
smoke held every Task 426 value (guarded segment-load `true/54`, segment-read `true/43`,
direct-edge `true/10`, timer safe points `true/550`, a seven-field funnel, segment-pop `36/4`,
segment-load `8/5`, generation `0/0`) at the same arena base.

**The legacy smoke matters most here**, since the deleted wrappers were the legacy ones: with
the backend unset, a one-second pumpit3 run exited 0 and reported the `legacy` backend, with
61,666 exception dispatches against the recent 60,989 and 62,897. That figure counts
single-steps within one wall-clock second and varies per run, so the verdict rests on the range
and the clean exit.
