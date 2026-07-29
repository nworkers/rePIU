# 20260729-347 작업 로그: 현재 Release 실행 축 재귀속 / Work log: Current Release execution-axis re-attribution

* 설계: [20260729-347-release-axis-reattribution.md](../design/20260729-347-release-axis-reattribution.md)
* 작업 지시: [20260728-347-release-axis-reattribution.md](../work-orders/20260728-347-release-axis-reattribution.md)
* 측정 산출물: `build/benchmarks/release-axis/20260729-143257/` (Git 제외)

## 한국어

### 결과

Task 348의 AOT 타이머 safe point와 Task 349의 원본 240Hz PIT cadence를 포함한 현재
Release HEAD를 60초씩 세 번 측정했습니다. 프레임 중앙값은 1,124이고 실행 축은
다음과 같습니다.

| 축 | 중앙값 | 3회 범위 | 판정 |
|---|---:|---:|---|
| 실제 guest 실행 추정 | **60.72%** | 60.61~61.25% | **G2 성립** |
| Glide gate | **21.73%** | 21.71~21.91% | **G3 성립** |
| VEH-exclusive | 9.70% | 9.26~9.79% | G4 불성립 |
| 커널 예외 전이 추정 | 6.83% | 6.74~6.92% | G1 불성립 |

G2와 G3가 동시에 성립했습니다. 큰 축은 guest 실행이고, Glide gate 내부도
queue/wake/work/complete 중앙값이 약 0.51%/31.62%/38.92%/28.96%로 분산됐습니다.
따라서 다음 대상은 **AOT 캐시 내 guest 실행 60.72%의 active work 대
timer/pacing busy-wait 재귀속**입니다. guest 명령은 이미 host CPU에서 직접
실행되므로 측정 없이 번역 품질 수정으로 바로 가지 않습니다.

### 재현 가능한 측정 도구

`scripts/task347_release_axis_reattribution.ps1`을 추가했습니다.

* Release AOT probe를 먼저 실행해 전이 가격과 전체 probe 성공을 확인합니다.
* `aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1`, 고정 timeout을 사용합니다.
* 같은 seed에서 실행별 EEPROM을 복사합니다.
* stdout/stderr와 실행별 JSON, 전체 CSV/summary JSON을 기록합니다.
* timeout, profile, PIT 240Hz, census, 복귀 funnel, 프레임/gate/get-proc,
  malformed/fatal/Glide issue를 자동 검증합니다.

10초 smoke에서 resident export의 실제 이름이 `_GRBUFFERSWAP@4`임을 확인해 parser를
수정했습니다. 이 smoke 자체는 프레임 liveness gate 전이어서 의도대로 실패했지만,
실제 로그에는 191 buffer swap이 있었으므로 원인은 대소문자와 decorated name
불일치였습니다.

### 계측 계약 정정

첫 60초 시도에서 census 393,418과 `exception_dispatch_entry_count` 242,788이
달랐습니다. 코드 순서를 다시 확인한 결과:

1. 배타 census와 `kVehTotal` scope는 `DispatchGuestException` 진입부에서 시작합니다.
2. `ExceptionDispatchScope`는 AOT write completion/fault, 타이머 safe point,
   reentry와 transfer handler **뒤**에서 시작합니다.
3. 따라서 `exception_dispatch_entry_count`는 전체 VEH 진입이 아니라
   late-dispatch 계수입니다.

현재 실행에서는 census의 약 38%가 그 scope 전에 처리됩니다. 대조값을
execution-time profile의 `kVehTotal` count로 바꿨습니다. timeout snapshot 순간에는
census가 증가한 뒤 아직 scope가 닫히지 않은 한 건이 존재할 수 있고, 최종 세 실행
모두 census와 profile count 차이가 정확히 1이었습니다.

이 사실은 runtime 의미를 바꾸지 않고 측정 계약만 정정했습니다.

### 세 실행

| run | frames | VEH | Glide | VEH-exclusive | kernel 전이 | guest 실행 | exceptions |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1,124 | 31.92% | 21.71% | 9.26% | 6.83% | 61.25% | 387,293 |
| 2 | 1,141 | 32.47% | 21.73% | 9.79% | 6.92% | 60.61% | 392,627 |
| 3 | 1,112 | 32.55% | 21.91% | 9.70% | 6.74% | 60.72% | 381,892 |

동등성은 세 실행 모두 다음을 통과했습니다.

* 정상 60초 timeout
* malformed 0, fatal breakpoint 0, fatal halt false
* Glide implementation issue 6개 축 모두 0
* frames 1,112~1,141, gate 94,379~98,088, get-proc 39
* PIT divisor 4,972 / 240Hz
* census와 `kVehTotal` count 차이 1
* 커널 전이 추정 <= unaccounted

### 예외와 복귀

예외 중앙값:

| 종류 | 중앙값 | 대략적 비중 |
|---|---:|---:|
| single-step | 128,378 | 33.15% |
| breakpoint | 195,933 | 50.60% |
| access violation | 22,098 | 5.7% |
| other (`0xC0000096`) | 40,822 | 10.5% |

새 Release probe의 same-machine 교정값은 `INT3 27,973`,
single-step `30,188 cycle`입니다. 이를 현재 census에 곱한 전이 비중 중앙값이
6.83%입니다. AV와 other에는 별도 교정값이 없어 `INT3` 가격을 적용한 추정입니다.

Task 348 타이머 safe-point trap은 5,535~5,539회, breakpoint의 중앙값 2.83%입니다.
지배 인구가 아닙니다.

복귀 funnel 중앙값은 success 48,765(78.10%), `span-unsafe` 13,702(21.90%)입니다.
그러나 세 실행 모두 single-step run bucket은 길이 1에만 값이 있고 최대값도 1입니다.
Task 337의 5~8 mode와 33+ tail은 사라졌으므로 `span-unsafe` 비율만으로 다음 대상을
정하지 않습니다.

### 빌드와 검증

* PowerShell parser: 성공
* `git diff --check`: 성공
* Win32 x86 Release 전체 빌드: 성공
  * 두 번의 240초 도구 제한 중단 뒤 충분한 제한으로 전체 빌드 완료
  * 기존 C4819와 Zydis import 경고는 비차단
* Release `repiu_aot_probe`: exit 0
  * `exception_transition_calibration_all=true`
  * `pit_timer_probe=true,divisor=4972,frequency_hz=240`
  * `timer_safe_point_probe=true`
* Release direct-loader 60초 3회: 성공

### 남은 작업

1. AOT 캐시 내 guest 실행을 주소·phase별로 표본화합니다.
2. hot loop가 240Hz tick 또는 다른 guest timer를 기다리는 pacing인지 구분합니다.
3. pacing이 의도된 대기라면 2순위인 Glide gate로 돌아갑니다.
4. `SUPERBLOCK`은 far-call emitter 계약이 정리될 때까지 보류합니다.

---

## English

### Result

Measured the current Release HEAD, including Task 348 AOT timer safe points and
Task 349's original 240 Hz PIT cadence, in three 60-second runs. Median frames
were 1,124. Estimated real guest execution was 60.72% (60.61-61.25%), satisfying
G2; the Glide gate was 21.73% (21.71-21.91%), satisfying G3; VEH-exclusive work
was 9.70%, and estimated kernel transitions were 6.83%.

G2 and G3 both hold, but guest execution is the larger axis and the Glide gate
itself is distributed across wake, host work, and completion. The next target
is therefore attributing the 60.72% inside the AOT cache between active work and
timer/pacing busy-wait. Original guest instructions already execute directly on
the host CPU, so no translation-quality change is selected without that
measurement.

### Reproducible harness

Added `scripts/task347_release_axis_reattribution.ps1`. It runs the Release AOT
probe, fixes the backend/profile/timeout environment, copies one EEPROM seed per
run, captures stdout/stderr, emits per-run JSON and aggregate CSV/JSON, and
validates timeout, profile, PIT cadence, census, re-entry funnel,
frame/gate/get-proc liveness, malformed/fatal state, and Glide issues.

A ten-second smoke exposed the decorated resident name
`_GRBUFFERSWAP@4`; the parser was corrected before the three final runs.

### Measurement-contract correction

The first 60-second attempt found census 393,418 against
`exception_dispatch_entry_count` 242,788. Code inspection confirmed that the
census and `kVehTotal` scope begin at `DispatchGuestException` entry, while
`ExceptionDispatchScope` begins only after AOT write, timer, re-entry, and
transfer early handlers. Its counter is therefore late dispatch, not whole VEH.

The harness now validates against the execution-time profile's `kVehTotal`
count. A timeout snapshot may see one census increment whose scope has not yet
closed; all three final runs had exactly this one-count delta. Runtime semantics
were not changed.

### Runs and equivalence

The three frame counts were 1,124, 1,141, and 1,112. Every run reached its
normal timeout, retained zero malformed dispatch, zero fatal breakpoints, no
fatal halt, and zero Glide issues, kept nonzero and overlapping frame/gate/
get-proc values, recorded PIT divisor 4,972 at 240 Hz, kept the census/profile
delta at one, and kept the transition estimate within unaccounted time.

The refreshed same-machine prices are 27,973 cycles for `INT3` and 30,188 for
single-step. Applied to the current census, transitions are 6.83% of wall clock;
AV and other exceptions use the `INT3` price and remain inferred.

Median exceptions were 128,378 single-steps, 195,933 breakpoints, 22,098 access
violations, and 40,822 other exceptions (`0xC0000096`). Timer-safe-point traps
were only 2.83% of breakpoints.

The median re-entry funnel is 48,765 successes (78.10%) and 13,702
`span-unsafe` rejections (21.90%). Every single-step run in every measurement
has length one, so Task 337's five-to-eight mode and 33+ tail are gone.
`span-unsafe` is not selected from rejection share alone.

### Verification and next work

PowerShell parsing and `git diff --check` passed. The full Win32 x86 Release
build completed after two tool-limited partial attempts, the full Release AOT
probe exited zero with transition, PIT, and timer-safe-point probes passing,
and all three direct-loader measurements completed.

Next, sample AOT-cache guest execution by address and phase and distinguish
active work from 240 Hz or other guest timer pacing. If the dominant time is
intentional pacing, return to the second-ranked Glide gate. `SUPERBLOCK`
remains deferred until the far-call emitter contract is understood.
