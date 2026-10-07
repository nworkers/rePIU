# 환경 변수 토글 목록과 정리 대상

코드가 읽는 `REPIU_*` 환경 변수를 전수 조사해, 기능을 켜고 끄는 토글 중 삭제할 수
있는 것을 가린 결과를 누적한다. 조사일 2026-10-07, 기준 커밋 `be19e9c`(v0.0.206).

## 조사 방법

* `src/`·`include/`에서 `"REPIU_..."` 문자열을 모두 뽑았다 — **191개**.
* 이름에 TRACE·LOG·CENSUS·PROFILE·DUMP·PROBE·SAMPLE 등이 든 **105개는 진단용**으로
  분류해 이번 범위에서 뺐다. 경로·타임아웃 같은 설정값도 뺐다.
* 남은 기능 토글 약 45개는 판정식(`ResolvePromotedToggle`/`ResolveOptInToggle` 또는 직접
  `getenv`)을 읽어 **기본값**을 확정했다.
* "마지막 사용"은 그 변수를 언급한 `docs/` 문서 파일명의 가장 최근 날짜다. 측정 기록을
  대신하는 **근사치**다.
* CI(`.github`)는 아래 후보를 하나도 쓰지 않는다. 과거 작업의 A/B 스크립트 일부만
  참조한다(각 절에 적음).

## 확인됨

### 1번 묶음 — 기능은 남기고 끄기 스위치만 삭제 (15개, issue #20)

2026-08 초에 기본 켜기로 승격된 뒤 꺼진 경로를 쓴 기록이 없다.

| 묶음 | 변수 | 승격 / 마지막 사용 |
|---|---|---|
| AOT-DBT 디스패치 | `REPIU_AOT_DBT_PORT_IO_DISPATCH`(Task 386), `REPIU_AOT_DBT_RETURN_MISS_DISPATCH`, `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH`, `REPIU_AOT_DBT_TIMER_SAFE_POINTS` | 08-01~08-05 |
| 캐시 패치·반환 테이블 | `REPIU_AOT_INLINE_CACHE_PATCH_INLINE`, `REPIU_AOT_DIRECT_RETURN_TABLE` | 08-07 / 09-26 |
| Glide setter 생략 | `REPIU_GLIDE_SETTER_ELIDE`, `_BATCH3`, `_BATCH4`, `_TEXTURE` | 07-30~08-28. 한 기능에 스위치 4개, `_BATCH4`는 언급 문서 0 |
| Glide 기타 | `REPIU_GLIDE_DRAW_BATCH`, `REPIU_GLIDE_HOST_WAIT` | 08-28 / 07-28 |
| Glide 옛 동작 복원 | `REPIU_GLIDE_GATE_PUMP` | 07-30. `=1`이 옛 pump를 되살리는 역방향 스위치 |
| 기타 | `REPIU_PORT_IO_DELAY_LOOP`, `REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE` | 08-04 / 07-26 |

참조하는 절차·스크립트: `docs/guides/glide-setter-elision-testing.md`(종료된 A/B 절차),
`docs/guides/gameplay-scene-capture.md`, `docs/guides/pumpit3-stall-reproduction.md`
(Task 414 이전 정지 재현), `scripts/task365_glide_setter_state_elision.ps1`,
`scripts/task414_delay_loop_ab.ps1`.

### 2번 묶음 — 기능과 스위치를 함께 삭제할 옵트인 실험 (11개 묶음, 보류)

기본 꺼짐인 채 승격되지 않았고 7~8월 이후 쓴 기록이 없다.

| 변수 | 근거 |
|---|---|
| `REPIU_AOT_DBT_POST_HLE_TRANSLATE` | 문서 판정 "무효 — 경로에 진입조차 하지 않음" |
| `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH` | 1차 측정 경로(08-01~05) |
| `REPIU_AOT_DBT_INDIRECT` | "기본 꺼짐 유지"(07-24~08-05) |
| `REPIU_NATIVE_REGION` | 기본 clean-function fast path로 대체(08-28) |
| `REPIU_NATIVE_LINEAR_SPAN_CACHE`, `_JUMPS`, `_WRITES` | 07-24~26, `_WRITES`는 "조기 종료, opt-in으로만 유지" |
| `REPIU_AOT_RETIRED_SPAN_REENTRY` | 07-26 |
| `REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME` | Task 346 이전 동작 복원 스위치(07-28) |
| `REPIU_AOT_QUARANTINE_FIRST_WRITE`, `REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE`, `REPIU_AOT_STRICT_SPANNING_ENTRY`, `REPIU_AOT_PATCH_WIDE_PROTECT` | 07-28~08-04, 실험·A/B 전용 |

참조하는 스크립트: `scripts/benchmark_native_linear_span.ps1`, `task283`~`task287`,
`task347`, `task413`.

### 유지

* **최근 승격, 롤백 보험 필요**: `REPIU_AOT_DBT_SUPERBLOCK`,
  `REPIU_TIMER_HANDLER_CACHE_ENTRY`(10-07), `REPIU_TIMER_RETURN_PAD`,
  `REPIU_GLIDE_LFB_HIGH_PRECISION`, `REPIU_PIC_TIMER_IN_SERVICE`, `REPIU_GUEST_CLI_HOLD`,
  `REPIU_EVENT_CLOCK`, `REPIU_AOT_REENTRY_MEMO`, `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH`,
  `REPIU_LINUX_X64_SAFE_POINT_INJECTION`(Linux).
* **승격은 오래됐지만 보류**: `REPIU_AOT_GUARDED_SEGMENT_LOAD/POP/READ` — v0.0.206(#18)에서
  슬롯 내부를 교체해, `=0`이 새 슬롯의 문제를 가려낼 유일한 비상 수단이다.
* **판정 미정 실험**: `REPIU_GLIDE_ASYNC_PRESENT`(09-28 측정 중),
  `REPIU_GLIDE_LFB_STAGING_REUSE`(09-22).
* **사용자 설정·진단 도구**: `REPIU_LAUNCHER`, `REPIU_DISABLE_NATIVE_FAST_PATH`,
  `REPIU_NATIVE_LINEAR_SPAN`, 경로·타임아웃 설정값.

## 미확정

* 진단용 105개 중 더 이상 쓰지 않는 것. 런타임 비용이 거의 없어 이번 조사에서 뺐다.

---

# Environment toggle inventory and cleanup targets

An exhaustive survey of the `REPIU_*` environment variables the code reads, sorting the
feature toggles into those that can be deleted. Surveyed 2026-10-07 at `be19e9c` (v0.0.206).

## Method

All `"REPIU_..."` strings in `src/` and `include/` — **191**. The **105** whose names carry
TRACE, LOG, CENSUS, PROFILE, DUMP, PROBE, SAMPLE and the like are diagnostics and were left
out, as were path and timeout settings. For the ~45 remaining feature toggles the default was
settled by reading the resolver (`ResolvePromotedToggle`/`ResolveOptInToggle` or a direct
`getenv`). "Last used" is the newest date among `docs/` file names that mention the variable —
an **approximation** standing in for measurement records. CI (`.github`) uses none of the
candidates; only some historical A/B scripts reference them (listed per group).

## Confirmed

* **Group 1 — delete the kill switch, keep the feature (15, issue #20).** Promoted to
  default-on in early 2026-08 with no recorded use of the off path since:
  `REPIU_AOT_DBT_PORT_IO_DISPATCH` (Task 386), `REPIU_AOT_DBT_RETURN_MISS_DISPATCH`,
  `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH`, `REPIU_AOT_DBT_TIMER_SAFE_POINTS`,
  `REPIU_AOT_INLINE_CACHE_PATCH_INLINE`, `REPIU_AOT_DIRECT_RETURN_TABLE`,
  `REPIU_GLIDE_SETTER_ELIDE` with `_BATCH3`/`_BATCH4`/`_TEXTURE` (four switches for one
  feature; `_BATCH4` appears in no document), `REPIU_GLIDE_DRAW_BATCH`,
  `REPIU_GLIDE_HOST_WAIT`, `REPIU_GLIDE_GATE_PUMP` (a reverse switch whose `=1` restores the
  old pump), `REPIU_PORT_IO_DELAY_LOOP`, `REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE`. Referenced
  by the concluded A/B procedures in `docs/guides/glide-setter-elision-testing.md`,
  `gameplay-scene-capture.md`, `pumpit3-stall-reproduction.md` (reproducing the pre-Task-414
  stall) and `scripts/task365_*`, `task414_*`.
* **Group 2 — delete feature and switch, abandoned opt-in experiments (11 groups, deferred).**
  Never promoted, unused since July–August: `REPIU_AOT_DBT_POST_HLE_TRANSLATE` (judged
  "ineffective — the path is never even entered"), `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH`,
  `REPIU_AOT_DBT_INDIRECT`, `REPIU_NATIVE_REGION` (superseded by the default clean-function
  fast path), `REPIU_NATIVE_LINEAR_SPAN_CACHE`/`_JUMPS`/`_WRITES`,
  `REPIU_AOT_RETIRED_SPAN_REENTRY`, `REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME`,
  `REPIU_AOT_QUARANTINE_FIRST_WRITE`, `REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE`,
  `REPIU_AOT_STRICT_SPANNING_ENTRY`, `REPIU_AOT_PATCH_WIDE_PROTECT`. Referenced by
  `scripts/benchmark_native_linear_span.ps1`, `task283`–`task287`, `task347`, `task413`.
* **Keep**: recent promotions whose rollback insurance still matters
  (`REPIU_AOT_DBT_SUPERBLOCK`, `REPIU_TIMER_HANDLER_CACHE_ENTRY`, `REPIU_TIMER_RETURN_PAD`,
  `REPIU_GLIDE_LFB_HIGH_PRECISION`, `REPIU_PIC_TIMER_IN_SERVICE`, `REPIU_GUEST_CLI_HOLD`,
  `REPIU_EVENT_CLOCK`, `REPIU_AOT_REENTRY_MEMO`, `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH`,
  `REPIU_LINUX_X64_SAFE_POINT_INJECTION`); the long-promoted but held
  `REPIU_AOT_GUARDED_SEGMENT_LOAD/POP/READ` (slot internals replaced in v0.0.206, so `=0` is
  the only escape hatch for the new slots); undecided experiments
  (`REPIU_GLIDE_ASYNC_PRESENT`, `REPIU_GLIDE_LFB_STAGING_REUSE`); and user settings or
  diagnostic tools (`REPIU_LAUNCHER`, `REPIU_DISABLE_NATIVE_FAST_PATH`,
  `REPIU_NATIVE_LINEAR_SPAN`, paths and timeouts).

## Unresolved

* Which of the 105 diagnostics are no longer used. They cost almost nothing at run time and
  were left out of this survey.
