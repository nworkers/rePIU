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

### 2번 묶음 — 기능과 스위치를 함께 삭제한 옵트인 실험 (11개 묶음, issue #22에서 삭제)

기본 꺼짐인 채 승격되지 않았고 7~8월 이후 쓴 기록이 없다. issue #22(2026-10-08)에서
기능과 스위치를 함께 삭제했다. 간접 디스패치 경로에서만 무장되던 진단
`REPIU_AOT_DBT_CALL_STEP`(Task 285)도 같이 지웠다. 설계는
`docs/design/20261008-i022-delete-abandoned-opt-in-experiments.md`.

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

참조하던 스크립트(조사 때의 기록을 바로잡음): span 변수는 `task287`과
`benchmark_native_linear_span.ps1`만, `REPIU_AOT_DBT_INDIRECT`는 `task283`~`task287`과
벤치마크 스크립트, `REPIU_AOT_PATCH_WIDE_PROTECT`는 `task413`이 썼다. `task347`은
`REPIU_AOT_DBT_POST_HLE_TRANSLATE`를 비우기만 했다. #22에서 `task347`을 뺀 일곱 개를
지웠고 `task347`에서는 지워진 변수 줄만 뺐다.

삭제에서 확인한 사실:

* 이전 동작을 되살리던 다섯 스위치(`SEGMENT_WRITE_BLOCKS_RESUME`, `QUARANTINE_FIRST_WRITE`,
  `QUARANTINE_ON_GENERATION_FAILURE`, `STRICT_SPANNING_ENTRY`, `PATCH_WIDE_PROTECT`)는
  모두 조건 하나를 더하는 형태여서, 같은 동작이 기본 경로의 안전 분기(기록표 넘침, 억제
  집합 포화, 빈 패치 범위)로는 계속 도달 가능하다.
* span 쓰기 실험의 `write fault-cancel` 집계는 기본 span에서도 쓰인다. `push` 같은 암묵적
  스택 write는 명시적 memory write 판정에 걸리지 않아 span 안에 들어갈 수 있고, 감시
  page에서 fault를 낸다.
* `VehExitSite`의 세 값(`kCallStepProbe`, `kNativeRegionReturn`, `kNativeRegionSensitive`)은
  번호를 지키기 위해 퇴역 자리로 남겼다.

### 진단 이름 뒤의 토글 — 2차 조사(2026-10-09, 기준 `2855b3e` v0.0.208)

1차 조사는 이름만 보고 진단용 105개를 뺐다. 2차 조사에서 남은 162개를 다시 뽑아
판정식을 읽었더니, 진단처럼 보이는 이름 안에 **기본 켜기 스위치**와 **게스트 동작을
바꾸는 실험 스위치**가 섞여 있었다. 기본 켜기 판정은 `value == nullptr ||` 형태의 직접
`getenv`와 `ResolvePromotedToggle` 호출을 모두 훑어 확인했다.

#### 3번 묶음 — issue #24에서 삭제(2026-10-09)

설계는 `docs/design/20261009-i024-retire-hidden-toggles.md`.

끄기 스위치(기능 유지, 1번 묶음과 같은 형태):

| 변수 | 승격 | 끄면 |
|---|---|---|
| `REPIU_GLIDE_DRAW_ENTRY_POINTS` | Task 420, 08-05 | point·AA·polygon 진입점 7종을 요청만 받고 그리지 않음 |
| `REPIU_TIMER_TICK_BACKLOG` | Task 432, 08-06 기본 켜기 전환 | tick을 합쳐 버림(Task 432 이전 동작) |
| `REPIU_JAMMA_SNAPSHOT` | Task 403, 08-02 | 매 읽기 조회. `REPIU_JAMMA_SNAPSHOT_US=0`도 같은 효과라 끄는 길이 둘이다. `_US`는 주기 설정으로만 남긴다 |

실험(기능과 스위치 함께, 2번 묶음과 같은 형태):

| 변수 | 근거 |
|---|---|
| `REPIU_DOS4GW_MEMORY_PATH_PROBE=pharlap` | Task 611 판정 "AH=4Ah까지 가지만 할당은 풀지 못함". 두 곳에서 게스트 경로를 바꾼다 |
| `REPIU_DPMI_1E7F_PROBE_SUCCESS`(같은 블록의 `REPIU_DPMI_1E7F_TRACE`와 함께) | Task 599. 진단용으로 CF만 지워 가짜 성공을 돌려줬다. Task 606이 이 호출을 x64 word 스택 lowering 결함의 반환 주소 손상으로 확정했고 수정 뒤 기본 실행에서 사라졌으므로(`EXE_DESIGN.ko.md` 첫 절) 실행 없이 삭제했다 |

#### 일회성 진단 — issue #25에서 9개 삭제, 4개 유지(2026-10-09)

이미 답이 나온 질문을 위해 넣은 진단으로 올렸던 후보다. 코드에서 다시 읽어 "살아 있는
기구를 재는 재사용 계측이거나 가이드·스크립트가 쓰면 유지"로 갈랐다. 설계는
`docs/design/20261009-i025-delete-one-off-diagnostics.md`.

* **삭제(9)**: 아래 Glide 7개, `REPIU_LOWMEM_TRACE`, `REPIU_AOT_PROBE_GUEST`.
* **유지(4)**: `REPIU_AOT_DBT_CALL_TRACE`(trace 순번이 call frame에 저장돼 반환 대조에 쓰임,
  #22 설계도 유지), `REPIU_AOT_RETIRED_TRAP_PROFILE`(살아 있는 retired trap의 계측, ARCHITECTURE
  절과 probe 있음), `REPIU_GLIDE_SETTER_CENSUS`/`_PHASE`(캡처·생략 검증 가이드와 `task364`·
  `task365` 스크립트가 씀).

조사 때의 후보 목록:

* Glide 렌더 초기 작업(07-22~08-01): `REPIU_GLIDE_CALL_AUDIT`, `REPIU_GLIDE_TEX_CENSUS`,
  `REPIU_GLIDE_DRAW_CENSUS`, `REPIU_GLIDE_TRI_CENSUS`, `REPIU_GLIDE_FRAME_DUMP`,
  `REPIU_DUMP_LFB_BMP`, `REPIU_GLIDE_VERTEX_DEPTH_CENSUS`
* `REPIU_LOWMEM_TRACE`: v0.0.81 저지대 수정용. 언급 문서 0건이고 `RecordLowMemoryAccess`가
  같은 정보를 기록한다
* 7월 AOT 조사: `REPIU_AOT_PROBE_GUEST`, `REPIU_AOT_DBT_CALL_TRACE`(Task 284, 쓰던
  스크립트는 #22에서 삭제), `REPIU_AOT_RETIRED_TRAP_PROFILE`(Task 306, 전용 probe 포함)
* 판단 필요: `REPIU_GLIDE_SETTER_CENSUS`/`_PHASE`(Task 364). 상시 켜진 setter 생략을 재던
  도구다

#### 함께 찾은 낡은 기록

* `current-execution-frontier.md`의 "지금 켜져 있는/꺼져 있는 것" 표에 #20·#22에서 지운
  변수 5개가 남아 있고 `REPIU_TIMER_TICK_BACKLOG`와 이후 승격된 `REPIU_AOT_DBT_SUPERBLOCK`을
  OFF로 적었다(#24에서 고쳤다).
* README가 코드가 읽지 않는 `REPIU_DUMP_TEXTURE_BMP`를 설명한다. 실제 변수는
  `REPIU_GLIDE_TEX_DUMP`다(#24에서 고쳤다).

### 유지

* **2차 조사에서 유지로 판정**: `REPIU_GLIDE_SWAP_WAIT_TICKS`(10-05, 최근),
  `REPIU_AOT_INDIRECT_CACHE_SLOTS`(09-26 크래시 bisect에 사용),
  `REPIU_GLIDE_RENDEZVOUS_SPIN_US`(코어가 적은 환경용 설정), `REPIU_NATIVE_SAMPLING`,
  `REPIU_LINUX_X64_GUEST_ESP_TRACE`(fault 핸들러가 읽는 Linux x64 도구, 문서는 없음),
  `REPIU_EXECUTION_TRACE_*`·`REPIU_EXECUTION_PROBE_DUMP_*` 계열.
* **최근 승격, 롤백 보험 필요**: `REPIU_AOT_DBT_SUPERBLOCK`,
  `REPIU_TIMER_HANDLER_CACHE_ENTRY`(10-07), `REPIU_TIMER_RETURN_PAD`,
  `REPIU_GLIDE_LFB_HIGH_PRECISION`, `REPIU_PIC_TIMER_IN_SERVICE`, `REPIU_GUEST_CLI_HOLD`,
  `REPIU_EVENT_CLOCK`, `REPIU_AOT_REENTRY_MEMO`, `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH`,
  `REPIU_LINUX_X64_SAFE_POINT_INJECTION`(Linux).
* **보류했다가 삭제(issue #30, 2026-10-09)**: `REPIU_AOT_GUARDED_SEGMENT_LOAD/POP/READ` —
  v0.0.206(#18)에서 슬롯 내부를 교체해 `=0`을 비상 수단으로 남겼고, v0.0.207~v0.0.210 동안
  새 슬롯 회귀가 보고되지 않아 재개 조건을 채웠다. 1번 묶음과 같은 형태로 스위치만 지웠다.
* **판정 미정 실험**: `REPIU_GLIDE_ASYNC_PRESENT`(09-28 측정 중),
  `REPIU_GLIDE_LFB_STAGING_REUSE`(09-22).
* **사용자 설정·진단 도구**: `REPIU_LAUNCHER`, `REPIU_DISABLE_NATIVE_FAST_PATH`,
  `REPIU_NATIVE_LINEAR_SPAN`, 경로·타임아웃 설정값.

## 미확정

* 2차 조사가 다루지 않은 나머지 진단(2026-08 중순 이후 언급된 것들)의 사용 여부. 위
  일회성 진단은 언급 날짜가 이른 것부터 읽어 고른 것이고 전수 판정이 아니다.

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
* **Group 2 — feature and switch deleted, abandoned opt-in experiments (11 groups, deleted in
  issue #22 on 2026-10-08, with `REPIU_AOT_DBT_CALL_STEP`, a Task 285 diagnostic only the
  indirect dispatch path armed).** Never promoted, unused since July–August: `REPIU_AOT_DBT_POST_HLE_TRANSLATE` (judged
  "ineffective — the path is never even entered"), `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH`,
  `REPIU_AOT_DBT_INDIRECT`, `REPIU_NATIVE_REGION` (superseded by the default clean-function
  fast path), `REPIU_NATIVE_LINEAR_SPAN_CACHE`/`_JUMPS`/`_WRITES`,
  `REPIU_AOT_RETIRED_SPAN_REENTRY`, `REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME`,
  `REPIU_AOT_QUARANTINE_FIRST_WRITE`, `REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE`,
  `REPIU_AOT_STRICT_SPANNING_ENTRY`, `REPIU_AOT_PATCH_WIDE_PROTECT`. Corrected script record:
  only `task287` and `benchmark_native_linear_span.ps1` used the span variables,
  `task283`–`task287` and the benchmark used `REPIU_AOT_DBT_INDIRECT`, `task413` used
  `REPIU_AOT_PATCH_WIDE_PROTECT`, and `task347` only cleared `REPIU_AOT_DBT_POST_HLE_TRANSLATE`;
  #22 deleted all but `task347`, which only lost that line. Found while deleting: the five
  rollback switches each added one condition, so the same behavior stays reachable through the
  default path's safety branches (table overflow, a full suppression set, an empty patch
  range); the span-writes experiment's `write fault-cancel` count is used by default spans too,
  because an implicit stack write such as `push` passes the explicit-write check and can fault
  on a watched page; and `VehExitSite`'s three unused values keep their slots so later numbers
  do not move.
* **Toggles behind diagnostic names — second survey (2026-10-09 at `2855b3e`, v0.0.208).**
  The first survey dropped 105 variables by name alone. Re-reading the resolvers of the 162
  that remain (every direct `getenv` of the form `value == nullptr ||` and every
  `ResolvePromotedToggle`) found **default-on kill switches** and **experiment switches that
  change guest behavior** among them.
  * **Group 3, deleted in issue #24 (2026-10-09).** Kill switches (keep the feature): `REPIU_GLIDE_DRAW_ENTRY_POINTS`
    (Task 420, 08-05; off stops drawing seven point/AA/polygon entry points),
    `REPIU_TIMER_TICK_BACKLOG` (Task 432 made it default-on on 08-06),
    `REPIU_JAMMA_SNAPSHOT` (Task 403, 08-02; `REPIU_JAMMA_SNAPSHOT_US=0` is a second off
    switch, so `_US` stays only as the interval). Experiments (delete with the feature):
    `REPIU_DOS4GW_MEMORY_PATH_PROBE=pharlap` (Task 611: "reaches AH=4Ah but does not solve
    allocation"), `REPIU_DPMI_1E7F_PROBE_SUCCESS` with `REPIU_DPMI_1E7F_TRACE` (Task 599: cleared
    only CF to fake success) — deleted without a run, because Task 606 traced the call to a
    return address corrupted by the x64 word-stack lowering defect and the default run no
    longer makes it.
  * **One-off diagnostics, issue #25 (2026-10-09): nine deleted, four kept.** Re-read in code
    and split by "keep a reusable measurement of a live mechanism, or anything a guide or script
    uses": deleted the seven Glide probes below, `REPIU_LOWMEM_TRACE` and
    `REPIU_AOT_PROBE_GUEST`; kept `REPIU_AOT_DBT_CALL_TRACE` (its sequence number is stored in
    call frames for return matching, and #22 kept it), `REPIU_AOT_RETIRED_TRAP_PROFILE` (measures
    live retired traps, with an ARCHITECTURE section and a probe) and
    `REPIU_GLIDE_SETTER_CENSUS`/`_PHASE` (used by the capture and elision guides and the
    `task364`/`task365` scripts). The candidates as surveyed: the Glide bring-up
    probes (07-22 to 08-01) `REPIU_GLIDE_CALL_AUDIT`, `_TEX_CENSUS`, `_DRAW_CENSUS`,
    `_TRI_CENSUS`, `_FRAME_DUMP`, `REPIU_DUMP_LFB_BMP`, `REPIU_GLIDE_VERTEX_DEPTH_CENSUS`;
    `REPIU_LOWMEM_TRACE` (v0.0.81, undocumented, duplicated by `RecordLowMemoryAccess`); the
    July AOT probes `REPIU_AOT_PROBE_GUEST`, `REPIU_AOT_DBT_CALL_TRACE` (Task 284; its scripts
    went in #22), `REPIU_AOT_RETIRED_TRAP_PROFILE` (Task 306, with its probe). Undecided:
    `REPIU_GLIDE_SETTER_CENSUS`/`_PHASE` (Task 364), which measured the now always-on setter
    elision.
  * **Stale records found.** The "currently on/off" table in `current-execution-frontier.md`
    still lists five variables deleted in #20/#22 and shows `REPIU_TIMER_TICK_BACKLOG` and the
    later-promoted `REPIU_AOT_DBT_SUPERBLOCK` as off;
    README describes `REPIU_DUMP_TEXTURE_BMP`, which the code does not read (it reads
    `REPIU_GLIDE_TEX_DUMP`). Both were fixed in #24.
* **Keep**: from the second survey, `REPIU_GLIDE_SWAP_WAIT_TICKS` (10-05, recent),
  `REPIU_AOT_INDIRECT_CACHE_SLOTS` (used in a 09-26 crash bisect),
  `REPIU_GLIDE_RENDEZVOUS_SPIN_US` (a setting for machines with few cores),
  `REPIU_NATIVE_SAMPLING`, `REPIU_LINUX_X64_GUEST_ESP_TRACE` (an undocumented Linux x64 tool the
  fault handler reads) and the `REPIU_EXECUTION_TRACE_*`/`REPIU_EXECUTION_PROBE_DUMP_*` family;
  recent promotions whose rollback insurance still matters
  (`REPIU_AOT_DBT_SUPERBLOCK`, `REPIU_TIMER_HANDLER_CACHE_ENTRY`, `REPIU_TIMER_RETURN_PAD`,
  `REPIU_GLIDE_LFB_HIGH_PRECISION`, `REPIU_PIC_TIMER_IN_SERVICE`, `REPIU_GUEST_CLI_HOLD`,
  `REPIU_EVENT_CLOCK`, `REPIU_AOT_REENTRY_MEMO`, `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH`,
  `REPIU_LINUX_X64_SAFE_POINT_INJECTION`); the long-promoted
  `REPIU_AOT_GUARDED_SEGMENT_LOAD/POP/READ`, held because v0.0.206 replaced their slot
  internals, lost their kill switches in issue #30 once v0.0.207 to v0.0.210 showed no
  regression; undecided experiments
  (`REPIU_GLIDE_ASYNC_PRESENT`, `REPIU_GLIDE_LFB_STAGING_REUSE`); and user settings or
  diagnostic tools (`REPIU_LAUNCHER`, `REPIU_DISABLE_NATIVE_FAST_PATH`,
  `REPIU_NATIVE_LINEAR_SPAN`, paths and timeouts).

## Unresolved

* Whether the diagnostics the second survey did not cover (those mentioned from mid-2026-08 on)
  are still used. The one-off list above was picked by reading the earliest-mentioned ones
  first; it is not an exhaustive verdict.
