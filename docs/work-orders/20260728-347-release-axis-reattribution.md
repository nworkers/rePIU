# 20260728-347 작업 지시: 복귀 경로 수리 이후 실행 축 재귀속 / Work order: Re-attributing the execution axis

## 한국어

### 배경 — Tasks 331~346이 무엇을 바꿨나

성능 기준을 Release로 옮긴 뒤(Task 331) 여섯 번의 수정이 있었고, 그때마다 축이
바뀌었습니다. **지금 남아 있는 귀속 수치는 대부분 그 수정들 이전 것입니다.**

| Task | 무엇을 고쳤나 | 측정된 효과 |
|---|---|---|
| 331 | Release 실행 계약, append 재귀속 | 동적 번역이 전체의 1.04%임을 확인 |
| 333 | host poll loop의 `Sleep(1)` → command 대기 | rendezvous 22.3배, 프레임 3.16배 |
| 334 | cache→guest 선형 탐색 → 이진 탐색 | 호출당 266배, 프레임 1.79배 |
| 335 | gate 진입마다의 중복 `PumpEvents` 제거 | gate 비중 -3.5%p, 프레임 +5.5% |
| 342 | quarantine을 반복 쓰기에만 | **프레임 2.21배** |
| 344 | quarantine 판정을 주소별로 | quarantine 0, 프레임 변화 없음 |
| 346 | 세그먼트 쓰기 뒤에도 재접기 후 복귀 | 복귀 success 95.1%, 프레임 +10.6% |

측정만 한 작업도 방향을 여러 번 바꿨습니다. Task 336(예외 전이 재가격), 337(예외
census), 339~341(복귀 차단 원인 추적), 343(특권 명령 트랩), 345(`SUPERBLOCK` 재판정).

**누적:** Task 331 시점 Release 60초 프레임 **275** → 현재 중앙값 **3,456**(약 12.6배).

### 왜 지금 재귀속인가

Task 346으로 post-HLE 복귀 success가 55.6% → **95.1%** 가 됐습니다. 그 결과:

* single-step 예외가 baseline 대비 크게 줄고 `INT3` 경계 트랩이 늘었습니다
  (예외 구성이 `79.24% / 19.59%`에서 `41.3% / 55.4%` 쪽으로 이동).
* **따라서 Task 336의 "커널 전이 27.7~30.4%"와 Task 337의 구간 분포는 이제 낡았습니다.**

**Task 338에서 이미 한 번, 낡은 축 위에서 대상을 골랐다가 잘못 골랐습니다.** 같은
실수를 반복하지 않기 위해 다음 최적화 대상을 고르기 **전에** 축을 다시 잽니다.

Task 347 지시서 작성 뒤에도 실행 축을 바꾸는 두 수정이 추가됐습니다.

* Task 348: AOT direct/conditional back edge에 타이머 safe-point `INT3` 추가.
* Task 349: 고정 55ms 게시를 제거하고 원본 PIT 설정인 240Hz IRQ0 cadence 복원.

따라서 재측정은 Task 346 직후가 아니라 **Task 350까지 포함한 현재 HEAD**를 기준으로
하며, safe-point trap을 breakpoint census의 부분집합으로 별도 보고합니다.

설계: [20260729-347-release-axis-reattribution.md](../design/20260729-347-release-axis-reattribution.md)

### 범위

**포함**

1. Release 60초 3회로 다음을 다시 잰다.
   * 상위 bucket(`guest-run` / `veh` / `glide-gate` / `unaccounted`)
   * 예외 census(종류별)와 single-step 구간 길이 분포
   * 커널 전이 총비용(전이 가격 × 현재 예외 수)
   * 복귀 funnel과 남은 거절 사유(`span-unsafe`)
   * Task 348 타이머 safe-point trap/injected/deferred
2. 그 값으로 **다음 최적화 후보를 순위화**한다.
3. `docs/analysis/current-execution-frontier.md`의 낡은 수치에 구성·시점 표기를 달거나
   갱신한다.
4. 같은 seed의 격리 EEPROM, 고정 환경변수, 로그 parser를 사용하는 재현 가능한
   `scripts/task347_release_axis_reattribution.ps1`을 남긴다.

**제외**

* 새 최적화 구현. 이 작업은 **대상 선정까지**다.
* `SUPERBLOCK`(emitter 계약 선행), `span-unsafe` 수정.
* Task 348/349의 실행 의미 변경. 이번 작업은 현재 구현을 관찰만 한다.

### 방법 규칙 (이번 연속 작업에서 확정된 것)

* **성능 판정은 프레임(`grBufferSwap`) 중앙값 3회.** `progress`는 emulate된 이벤트
  수이므로 처리량 지표가 아니다(Task 342).
* **동등성 계약:** malformed 0, fatal 0, Glide 공백 0, 60초 정상 timeout에 더해
  `grBufferSwap`·Glide gate 진입·LINEXE get-proc이 baseline 범위 안(Task 338).
* **고정 비용(커널 전이·syscall·대역폭)의 비중은 다른 곳을 최적화할 때마다 다시
  계산한다**(Task 336).
* 실행 간 편차가 18%에 이르므로 단일 표본으로 판정하지 않는다(Task 335).

### 사전 등록 gate

| gate | 조건 | 성립 시 다음 작업 |
|---|---|---|
| G1 | 커널 전이 >= 30% | 예외 자체를 줄이는 설계(emitter 계약 포함) |
| G2 | `unaccounted`(실제 guest 실행) >= 60% | guest 코드가 지배 — 번역 품질이 대상 |
| G3 | Glide gate >= 20% | 렌더 경로 재분해 |
| G4 | VEH-exclusive >= 20% | VEH 내부 재분해 |
| G5 | 어느 항목도 20% 미만 | 분해 경계가 틀렸으므로 재설계 |

### 검증 절차

1. Release 60초 3회, 위 동등성 계약 통과.
2. census 합계와 execution-time profile의 전체 VEH scope count 차이가 0 또는 1
   (timeout 순간 열린 scope 한 건 허용).
   `exception_dispatch_entry_count`는 AOT early handler 뒤의 late-dispatch 계수이므로
   전체 VEH 진입 대조값으로 사용하지 않는다.
3. 유도한 커널 전이 비중이 `unaccounted`를 넘지 않을 것.
4. 세 실행 모두 timer safe-point와 PIT 240Hz가 활성인 현재 기본 경로일 것.

---

## English

### Background

After moving the performance baseline to Release in Task 331, six changes landed — the host poll
loop's sleep (333), the cache-to-guest index (334), the per-gate pump rendezvous (335), quarantine
on repeated writes (342) and per address (344), and resuming after a segment write (346) — and each
moved the axis. Measurement-only tasks changed direction repeatedly too: re-pricing exception
transition (336), the exclusive exception census (337), tracing the return blocker (339-341),
naming privileged-instruction traps (343), and re-judging `SUPERBLOCK` (345). Cumulatively the
60-second Release frame count went from 275 at Task 331 to a median of 3,456, about 12.6x.

### Why re-attribute now

Task 346 raised the post-HLE return success share from 55.6% to 95.1%, which cut single-steps and
raised `INT3` boundary traps, shifting the exception mix substantially. Task 336's "27.7-30.4%
kernel transition" and Task 337's run-length distribution are therefore stale. Task 338 already
picked a target from a stale axis once and picked the wrong one; the axis is re-measured before the
next target is chosen.

Two later changes move the axis again: Task 348 adds timer-safe-point `INT3`
guards at AOT back edges, and Task 349 replaces the fixed 55ms publication with
the original PIT-programmed 240 Hz IRQ0 cadence. The measurement therefore uses
the current HEAD through Task 350 and reports safe-point traps as a subset of
the breakpoint census.

### Scope

Re-measure over three 60-second Release runs: the top-level buckets, the exception census and
single-step run lengths, the derived kernel transition cost at the current exception count, and the
return funnel with its remaining `span-unsafe` rejections, plus the timer-safe-point
trap/injected/deferred counts; then rank the next optimization
candidates and re-label or update the stale figures in the frontier. Out of scope: implementing any
optimization, `SUPERBLOCK`, which needs the emitter contract first, fixing
`span-unsafe`, or changing the Task 348/349 execution semantics. Leave a
reproducible `scripts/task347_release_axis_reattribution.ps1` harness using a
fixed environment, isolated EEPROM copies, and parsed run artifacts.

### Method rules fixed during this run of tasks

Performance is judged by the median `grBufferSwap` count over three runs, since `progress` counts
emulated events rather than throughput (Task 342). Equivalence requires zero malformed dispatch, no
fatal halt, no Glide gap, a normal 60-second timeout, and buffer swaps, gate entries, and resolved
procs staying in the baseline's range (Task 338). The share of any fixed cost is recomputed after
every optimization elsewhere (Task 336), and no judgement rests on a single sample, since run-to-run
spread reaches 18% (Task 335).

### Pre-registered gates

G1: kernel transition at or above 30%, selecting a design that reduces exceptions themselves. G2:
`unaccounted` real guest execution at or above 60%, making translation quality the target. G3: the
Glide gate at or above 20%, selecting a render-path decomposition. G4: VEH-exclusive at or above
20%, selecting a decomposition inside the VEH. G5: nothing reaching 20%, which would mean the
boundaries are wrong.

### Verification

Three 60-second Release runs passing the equivalence contract, a census total
within zero or one of the execution-time profile's whole-VEH scope count (the
one-count allowance is an open scope at the timeout snapshot), no use of the
late `exception_dispatch_entry_count` as the whole-VEH reference, a derived
kernel-transition share that fits inside `unaccounted`,
and confirmation that every run uses the current timer-safe-point and 240 Hz PIT path.
