# 실행 프레임률 측정 절차 / Measuring the execution frame rate

설계: [20260828-509](../design/20260828-509-linux-execution-speed.md) ·
작업 로그: [20260828-509](../work-logs/20260828-509-linux-execution-speed.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md)

이 문서는 **반복 수행하는 절차**만 담습니다. 특정 실행의 측정값은 작업 로그에 있습니다.

## 1. 어디서 숫자가 나오는가

모든 실행이 종료할 때 한 줄을 냅니다. **두 종료 갈래 모두**가 지나므로, 회수를 거절당해
로더 요약을 내지 못한 실행에서도 나옵니다(Task 508).

```
[repiu-shutdown] reason=timeout … gate=0 frames=1832 span_ms=41903
```

* `frames` — 표시된 프레임 총계.
* `span_ms` — **첫 프레임부터** 종료까지.

$$\text{fps} = \frac{\text{frames} \times 1000}{\text{span\_ms}}$$

**예산으로 나누지 마십시오.** `pumpit1`은 기동 자산 디코드에 약 45초를 쓰고 그동안 화면이
없습니다. 90초 예산을 그대로 나누면 실제보다 두 배 이상 낮게 나옵니다.

시간별 값이 필요하면 `REPIU_GLIDE_FRAME_RATE_LOG=1`로 초당 한 줄을 받습니다.

```
[repiu-frame-rate] fps=43.2 frames=1832 span_ms=41903
```

**평균 하나로 판정하지 마십시오.** "일정하게 느린 것"과 "어디서 무너지는 것"은 평균이 같을
수 있습니다.

## 2. 조건 — 어기면 다른 것을 잰 것입니다

| 항목 | 값 | 어기면 |
|---|---|---|
| 빌드 | **Release** | Debug는 11.34배 계수와 단계 순위 역전이 확인된 적이 있습니다(Task 330) |
| vsync | `REPIU_GLIDE_SWAP_INTERVAL=0` | 엔진이 아니라 디스플레이 대기를 잽니다(Task 440) |
| 감시견 | `REPIU_STALL_TIMEOUT_MS=0` | 건강한 실행을 정지로 오판한 전력 |
| 장면·예산 | 두 호스트 같은 값 | 비교가 성립하지 않습니다 |
| 반복 | 호스트당 3회 | 1회는 판정이 아닙니다 |

## 3. Linux

```bash
./scripts/build_linux_i386.sh --config Release
bash scripts/task509_frame_rate_measure.sh 3 90000 task509-linux
```

**빌드 트리가 하나입니다.** `build/linux_i386`은 단일 구성 생성기라 Release로 구성하면
Debug 트리가 대체됩니다. 정확성 작업으로 돌아갈 때 `--config Debug`로 다시 구성해야 하고,
그때 SDL이 다시 빌드됩니다.

## 4. Windows

```powershell
.\scripts\build_win32_x86.ps1 -Configuration Release
$env:REPIU_STALL_TIMEOUT_MS = "0"
$env:REPIU_EXECUTION_TIMEOUT_MS = "90000"
$env:REPIU_GLIDE_SWAP_INTERVAL = "0"
cmd /c "build\win32_x86_debug\Release\repiu.exe pumpit1 2> build\task509-win-run1.err"
Select-String -Path build\task509-win-run1.err -Pattern "repiu-shutdown] reason="
```

`build\win32_x86_debug`는 다중 구성 트리이므로 이름과 달리 `Release\`가 그 안에 있습니다.

## 5. 걸리기 쉬운 것

* **예산이 짧으면 프레임이 0입니다.** `pumpit1`은 첫 스왑까지 약 45초입니다. 90초 예산이면
  렌더 구간이 약 45초 남습니다. 20초 예산으로는 `frames=0`이 나오고, 그것은 느린 것이 아니라
  **아직 그리기 전**입니다.
* **`frames=0 span_ms=0`은 값이 아닙니다.** 스크립트가 `fps=n/a`로 표시합니다.
* **두 호스트의 빌드 구성이 다르면 배율을 쓰지 마십시오.** 다르면 그 사실을 숫자 옆에
  적습니다.
* **WSLg와 실제 데스크톱은 다른 조건입니다.** WSLg는 X11을 한 겹 더 지납니다. 섞어서
  비교하지 마십시오.

---

# Measuring the execution frame rate

Design: [20260828-509](../design/20260828-509-linux-execution-speed.md) ·
Work log: [20260828-509](../work-logs/20260828-509-linux-execution-speed.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md)

This document holds only the **repeatable procedure**; the measurements from a particular run are in
the work log.

## 1. Where the number comes from

Every run prints one line on the way out. **Both shutdown arms pass it**, so it appears even on a run
whose recovery was refused and which therefore prints no loader summary (Task 508).

```
[repiu-shutdown] reason=timeout … gate=0 frames=1832 span_ms=41903
```

* `frames` -- total presented frames.
* `span_ms` -- from the **first** frame to shutdown.

$$\text{fps} = \frac{\text{frames} \times 1000}{\text{span\_ms}}$$

**Do not divide by the budget.** `pumpit1` spends about forty-five seconds decoding start-up assets
with nothing on screen; dividing a 90-second budget understates the rate by more than half.

For a value over time, `REPIU_GLIDE_FRAME_RATE_LOG=1` gives one line a second.

```
[repiu-frame-rate] fps=43.2 frames=1832 span_ms=41903
```

**Do not judge from one average.** "Uniformly slow" and "collapses somewhere" can average the same.

## 2. Conditions -- bend one and you measured something else

| Item | Value | If bent |
|---|---|---|
| Build | **Release** | Debug has a measured 11.34x factor and inverts the stage ranking (Task 330) |
| vsync | `REPIU_GLIDE_SWAP_INTERVAL=0` | measures the display's wait rather than the engine (Task 440) |
| Watchdog | `REPIU_STALL_TIMEOUT_MS=0` | it has misjudged a healthy run as stalled |
| Scene and budget | the same on both hosts | it is not a comparison |
| Repeats | three per host | one run is not a verdict |

## 3. Linux

```bash
./scripts/build_linux_i386.sh --config Release
bash scripts/task509_frame_rate_measure.sh 3 90000 task509-linux
```

**There is only one build tree.** `build/linux_i386` uses a single-config generator, so configuring
Release replaces the Debug tree. Going back to correctness work means `--config Debug` again, and
SDL rebuilds when it happens.

## 4. Windows

```powershell
.\scripts\build_win32_x86.ps1 -Configuration Release
$env:REPIU_STALL_TIMEOUT_MS = "0"
$env:REPIU_EXECUTION_TIMEOUT_MS = "90000"
$env:REPIU_GLIDE_SWAP_INTERVAL = "0"
cmd /c "build\win32_x86_debug\Release\repiu.exe pumpit1 2> build\task509-win-run1.err"
Select-String -Path build\task509-win-run1.err -Pattern "repiu-shutdown] reason="
```

`build\win32_x86_debug` is a multi-config tree, so despite the name `Release\` lives inside it.

## 5. What trips this up

* **A short budget gives zero frames.** `pumpit1` takes about forty-five seconds to its first swap,
  so a 90-second budget leaves roughly forty-five seconds of rendering. A 20-second budget reports
  `frames=0`, which is not slowness but **not having started drawing**.
* **`frames=0 span_ms=0` is not a value.** The script reports `fps=n/a`.
* **Do not use a factor across differing build configurations.** If they differ, write that next to
  the number.
* **WSLg and a real desktop are different conditions.** WSLg goes through one more layer of X11. Do
  not mix them in one comparison.
