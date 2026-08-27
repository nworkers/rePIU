# Task 511 작업 로그 — 실행 중에 읽는 귀속 보고

설계: [20260828-511](../design/20260828-511-live-execution-profile-report.md) ·
작업 지시: [20260828-511](../work-orders/20260828-511-live-execution-profile-report.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
측정 절차: [execution-frame-rate-measurement](../guides/execution-frame-rate-measurement.md)

## 결과 — 축은 폴트 전달입니다

벽을 치웠고, **치우자마자 첫 실행이 답을 냈습니다.**

두 호스트가 같은 기계이므로 TSC cycle이 직접 비교됩니다. 프레임당, 100만 cycle 단위입니다.

| 버킷 | Windows | Linux | 배율 | **격차 기여** |
|---|---:|---:|---:|---:|
| **veh (폴트 핸들러)** | 2.065 | 88.072 | **42.6x** | **68.3%** |
| glide | 1.618 | 26.480 | 16.4x | 19.7% |
| unaccounted | 2.159 | 17.214 | 8.0x | 12.0% |
| dos | 0.006 | 2.438 | 433.9x | 1.9% |
| port-io | 0.006 | 0.105 | 18.6x | 0.1% |
| **합계** | **5.843** | **131.805** | **22.6x** | 126.0M |

**Linux 격차의 68%가 폴트 핸들러입니다.** 그리고 Linux에서 그것은 **시그널 전달**입니다 —
SIGSEGV·SIGTRAP이 sigaltstack 위로 배달되고 `sigreturn`으로 돌아오는 경로입니다.

조건: `pumpit1`, Release, vsync OFF, 90초 예산, `REPIU_EXECUTION_TIME_PROFILE=1`,
`REPIU_LIVE_PROFILE_INTERVAL_MS=10000`. 프로파일이 켜져 있으므로 두 호스트 모두 자기
기준선보다 약 10% 느립니다 — **몫으로 읽어야 하는 이유이고, 그래서 cycle로 비교했습니다.**

재현: `veh` 몫이 3회에서 **66.82% · 66.71% · 64.55%**입니다. Windows 대조군은 35.35%입니다.

## 510과 어긋나지 않습니다

glide 버킷이 격차의 19.7%인데 510은 Glide 노브가 아무것도 안 바꾼다고 했습니다. **모순이
아닙니다.** 510이 시험한 것은 호스트 왕복 **횟수**(`SETTER_ELIDE`)와 크로싱 **횟수**
(`DRAW_BATCH`)였습니다. 여기 26.5M cycle의 대부분은 횟수가 아니라 **회당 비용과 대기**이고,
510에서 async present가 6.29 ms를 회수한 것이 정확히 그 자리입니다.

## 시간별 값이 값을 했습니다

누적 몫만 냈으면 놓쳤을 것이 두 개 있습니다.

```
#1 frames=567  window_frames=567 cycles_per_frame=83,327,058  veh=57.20% dos=9.86%
#5 frames=1706 window_frames=275 cycles_per_frame=133,327,684 veh=59.82% dos=2.90%
#8 frames=2217 window_frames=135 cycles_per_frame=271,766,990 veh=66.82% dos=1.85%
```

* **프레임당 비용이 실행 중에 세 배가 됩니다** (83M → 272M). "일정하게 26.8배 느리다"가
  아니라 **장면이 진행될수록 나빠집니다.**
* `dos`가 9.86% → 1.85%로 빠집니다 — 기동 자산 적재가 끝나는 것이 보입니다.

이 저장소가 이번 이식에서 **"관측 창보다 긴 주기는 보이지 않는다"**에 두 번 걸렸고
(frontier 3.5절), 이번에는 걸리지 않았습니다.

## 구현

* **파생값을 공유 함수로 뺐습니다.** `Win32ExecutionTimeShares`와
  `ComputeExecutionTimeShares`를 `execution_time_profile`에 두고 `main.cpp`가 그것을 부릅니다.
  `veh_exclusive`·`unaccounted` 식이 `main.cpp` 안에만 있었고, 두 번째 독자가 생기는 순간
  복사본 두 개는 언젠가 서로 다른 수를 말합니다. **`main.cpp`의 출력은 그대로입니다.**
* **`live_execution_profile_report.{h,cpp}`** — 옵트인 `REPIU_LIVE_PROFILE_INTERVAL_MS`.
  누적 몫과 창 값을 함께 내고, 고정 버퍼 `snprintf` + `WriteHostErrorStream`입니다. 첫 호출은
  시계만 시작합니다.
* **훅은 `grBufferSwap` 한 자리**입니다. 게이트 진입마다 시계를 읽으면 계측이 스스로 비용이
  됩니다(Task 353). 그 자리에 이미 같은 모양의 옵트인 훅(`Win32GlideAdvanceFrameDump`)이
  있어 형태를 그대로 따랐습니다.

**락이 없는 이유는 스레드가 하나이기 때문입니다.** 카운터를 쓰는 것도, 읽는 것도 게스트
스레드입니다. 다른 스레드에서 읽었다면 i386에서 64비트 카운터가 찢어질 수 있었고, 그것이
"종료 갈래에서 덤프" 후보를 접은 이유입니다.

## 검증

| 항목 | 결과 |
|---|---|
| Linux i386 Release 빌드 | 성공, 오류 0 |
| Linux live 보고 | 실행당 7~8줄, 세 실행 모두 |
| 재현 | `veh` 66.82% · 66.71% · 64.55% |
| Windows 대조군 (요약 경로) | `veh` 35.35%, glide 27.70%, unaccounted 36.95% |
| 계측 부담 | Linux 26.7 fps 대 무계측 27.21 — 약 2% |
| **Windows Release·Debug 빌드 (511 포함)** | 둘 다 성공 |
| **Windows Debug probe** | 15 / 15, 실패 0 |
| **`main.cpp` 요약 (추출 검증)** | 추출 후 `veh 35.53% / glide 27.55% / port 0.08% / dos 0.09% / unacc 36.93%`, 추출 전 `35.35 / 27.70 / 0.10 / 0.10 / 36.95` — 실행 간 변동 범위 안, 형식 동일 |
| **Windows live 보고** | 같은 코드로 8줄 출력 |

### 한 가지 더 — 프레임당 비용이 두 호스트에서 반대로 움직입니다

| | 첫 창 | 마지막 창 |
|---|---:|---:|
| Windows cycles/frame | 5,838,995 | **3,212,893** (내려감) |
| Linux cycles/frame | 83,327,058 | **271,766,990** (세 배) |

**같은 장면인데 Windows에서는 싸지고 Linux에서는 비싸집니다.** 다만 이것을 그대로 읽으면 안
됩니다 — 90초 동안 Windows는 60,761 프레임, Linux는 2,328 프레임을 그렸으므로 **두 호스트가
장면의 같은 지점에 있지 않습니다.** 장면 진행이 시간 기반이면 같은 지점이고, 프레임 기반이면
아닙니다. 확인되지 않았으므로 **관측으로만 남깁니다.**

## 남은 경계

* **왜 시그널 전달이 그렇게 비싼지는 아직 모릅니다.** 배달 횟수가 많은 것인지 회당 단가가
  큰 것인지 가르지 않았습니다. `veh` 버킷의 `counts`가 그 답을 갖고 있고, 다음 단위입니다.
* **`dos`가 프레임당 434배**입니다. 격차 기여는 1.9%로 작지만 배율 자체는 이 표에서 가장
  큽니다. 작다고 넘길 것이 아니라 **왜 그런지 따로 봐야 합니다.**
* 성능을 고치지 않았습니다. 511은 읽을 수 있게 만드는 데까지입니다.

---

# Task 511 work log — attribution read while the run is going

Design: [20260828-511](../design/20260828-511-live-execution-profile-report.md) ·
Work order: [20260828-511](../work-orders/20260828-511-live-execution-profile-report.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Procedure: [execution-frame-rate-measurement](../guides/execution-frame-rate-measurement.md)

## Result — the axis is fault delivery

The wall came down, and **the first run past it gave the answer.**

Both hosts are the same machine, so TSC cycles compare directly. Per frame, in millions of cycles:

| Bucket | Windows | Linux | Factor | **Share of the gap** |
|---|---:|---:|---:|---:|
| **veh (the fault handler)** | 2.065 | 88.072 | **42.6x** | **68.3%** |
| glide | 1.618 | 26.480 | 16.4x | 19.7% |
| unaccounted | 2.159 | 17.214 | 8.0x | 12.0% |
| dos | 0.006 | 2.438 | 433.9x | 1.9% |
| port-io | 0.006 | 0.105 | 18.6x | 0.1% |
| **total** | **5.843** | **131.805** | **22.6x** | 126.0M |

**68% of Linux's gap is the fault handler**, and on Linux that is **signal delivery** -- SIGSEGV and
SIGTRAP delivered onto the alternate stack and returned through `sigreturn`.

Conditions: `pumpit1`, Release, vsync off, a 90-second budget, `REPIU_EXECUTION_TIME_PROFILE=1` and
`REPIU_LIVE_PROFILE_INTERVAL_MS=10000`. With the profile on, both hosts run about 10% below their own
baselines -- **which is why this has to be read as shares, and why the comparison is in cycles.**

Repeatability: the `veh` share over three runs is **66.82%, 66.71%, 64.55%**. The Windows control is
35.35%.

## This does not contradict Task 510

The glide bucket is 19.7% of the gap while 510 found the Glide knobs changed nothing. **There is no
contradiction.** What 510 varied was the **number** of host round trips (`SETTER_ELIDE`) and the
**number** of crossings (`DRAW_BATCH`). Most of the 26.5M cycles here is not count but **cost per
crossing and waiting** -- the same place async present recovered 6.29 ms in 510.

## The value over time earned its place

Two things a cumulative share alone would have hidden:

```
#1 frames=567  window_frames=567 cycles_per_frame=83,327,058  veh=57.20% dos=9.86%
#5 frames=1706 window_frames=275 cycles_per_frame=133,327,684 veh=59.82% dos=2.90%
#8 frames=2217 window_frames=135 cycles_per_frame=271,766,990 veh=66.82% dos=1.85%
```

* **The cost per frame triples during the run** (83M to 272M). It is not "uniformly 26.8x slower";
  **it gets worse as the scene proceeds.**
* `dos` falls from 9.86% to 1.85% -- start-up asset loading finishing, visible as it happens.

This port has twice been caught by **a period longer than the observation window** (frontier section
3.5). This time it was not.

## Implementation

* **The derived values moved into a shared function.** `Win32ExecutionTimeShares` and
  `ComputeExecutionTimeShares` now live in `execution_time_profile`, and `main.cpp` calls them. The
  `veh_exclusive` and `unaccounted` formulas were inline in `main.cpp`, and the moment a second
  reader exists, two copies is how two reports come to disagree. **`main.cpp`'s output is
  unchanged.**
* **`live_execution_profile_report.{h,cpp}`** -- opt-in through
  `REPIU_LIVE_PROFILE_INTERVAL_MS`. It prints the cumulative share alongside a window value, through
  a fixed-buffer `snprintf` and `WriteHostErrorStream`. The first call only starts the clock.
* **One hook, at `grBufferSwap`.** A clock read on every gate entry would make the instrument part of
  what it measures (Task 353). That place already had an opt-in hook of the same shape
  (`Win32GlideAdvanceFrameDump`), and the new one follows it.

**There is no lock because there is one thread.** The guest thread both writes the counters and reads
them. Reading from another thread could tear a 64-bit counter on i386, which is why the "dump on the
shutdown arm" candidate was dropped.

## Verification

| Item | Result |
|---|---|
| Linux i386 Release build | succeeded, zero errors |
| The Linux live report | seven to eight lines per run, in all three runs |
| Repeatability | `veh` at 66.82%, 66.71%, 64.55% |
| The Windows control (summary path) | `veh` 35.35%, glide 27.70%, unaccounted 36.95% |
| Instrument cost | Linux 26.7 fps against 27.21 uninstrumented -- about 2% |
| **Windows Release and Debug builds (with 511)** | both succeeded |
| **Windows Debug probe** | 15 of 15, zero failures |
| **`main.cpp`'s summary (the extraction)** | after: `veh 35.53% / glide 27.55% / port 0.08% / dos 0.09% / unacc 36.93%`; before: `35.35 / 27.70 / 0.10 / 0.10 / 36.95` -- within run-to-run variation, identical format |
| **The Windows live report** | eight lines from the same code |

### One more thing — cost per frame moves in opposite directions

| | First window | Last window |
|---|---:|---:|
| Windows cycles/frame | 5,838,995 | **3,212,893** (falling) |
| Linux cycles/frame | 83,327,058 | **271,766,990** (tripled) |

**The same scene gets cheaper on Windows and more expensive on Linux.** This must not be read
straight, though: in 90 seconds Windows drew 60,761 frames and Linux 2,328, so **the two hosts are
not at the same point in the scene.** If the scene advances on time they are; if it advances on
frames they are not. That is unconfirmed, so it stays **an observation only**.

## Remaining boundary

* **Why signal delivery is that expensive is not known yet.** Whether it is the number of deliveries
  or the cost of each has not been separated. The `counts` beside the `veh` bucket holds that answer,
  and it is the next unit.
* **`dos` is 434x per frame.** Its share of the gap is a small 1.9%, but the factor is the largest in
  the table. That is worth looking at on its own rather than dismissing as small.
* No performance was fixed. 511 goes as far as making it readable.
