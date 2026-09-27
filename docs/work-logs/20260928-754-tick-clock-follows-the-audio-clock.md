# Task 754 작업 로그 — tick의 시계를 오디오가 따르는 시계로

설계: [20260928-754](../design/20260928-754-tick-clock-follows-the-audio-clock.md)
작업 지시: [20260928-754](../work-orders/20260928-754-tick-clock-follows-the-audio-clock.md)

## 요약

Task 753 빌드에서도 같은 증상(음악이 느려졌다 돌아오고, 노트가 뒤로 돌아감)이 나왔습니다. 즉 753이 고친 입력
결함은 이 증상의 원인이 아니었습니다. 원인은 **엔진이 두 시계를 쓰고 있었고, 이 머신의 WSL에서 그 둘의
속도가 다르다**는 것입니다. tick 스케줄은 SDL의 시계(`CLOCK_MONOTONIC_RAW`)를, 오디오와 그 밖의 전부는
`CLOCK_MONOTONIC`을 따르는데, WSL의 시간 동기화가 `CLOCK_MONOTONIC`을 실제 시간보다 1~7%(초 단위로는 9%까지)
느리게 끌고 있었습니다. 게스트의 시간이 음악보다 빨리 가서 노트가 앞서 나갔다가 게임이 MP3 위치에 다시 맞출
때 뒤로 돌아왔습니다. tick 스케줄을 `steady_clock`으로 옮기자, 두 시계가 평균 3.2%(최악의 1초 9.1%) 어긋난
실행에서도 tick이 오디오의 시계로 초당 239.5~240.7개로 유지됐습니다.

음악 자체가 실제 시간으로 느려졌다 돌아오는 것은 WSL의 시계 보정에서 오며 **이 작업으로 고쳐지지 않습니다.**

## 과정

1. 사용자 로그의 최종 보고로 Task 753이 동작했음을 확인했습니다(읽힌 SERVICE 누름 5, 프레임 9,497개 모두
   복귀로 끝남, pruned 44 / edges 44).
2. census를 0.5초 단위 속도로 바꿨습니다. MP3 공급은 끝까지 초당 15.7~16.2 KB였고, tick은 30~40초에 초당
   241~246, 증상이 난 40~44.9초에 257~264였습니다. 선택 화면(8~12초)에도 260~263인 구간이 있었습니다.
3. `[repiu-pit]` 줄은 시작의 둘뿐이라 주파수는 240.048 Hz로 고정이었고, tick 예정 수는 시계에서 나오므로
   시계를 의심했습니다. WSL에서 `CLOCK_MONOTONIC_RAW`가 `CLOCK_MONOTONIC`보다 초에 따라 0~4.2% 빨랐습니다.
4. 어느 쪽이 실제 시간인지 Windows의 성능 카운터와 대조했습니다(Windows 쪽 프로세스가 WSL이 찍는 두 시계
   값에 도착 시각을 붙임, 46초). RAW는 모든 5초 창에서 ±0.001%, MONOTONIC은 −0.8~−7.1%, 전체 −3.1%.
5. `adjtimex`(읽기만)의 `tick`이 10000·9597·9788 사이를 오갔고, `systemd-timesyncd`가 돌고 있었으며, WSL의
   시각은 Windows보다 0.7~0.85초 앞서 있었습니다.
6. `EventClockNanoseconds`를 `steady_clock`으로 바꾸고, 키 이벤트의 timestamp를 그 시계로 옮겨 타임라인에
   기록하게 했습니다. 두 시계를 1초 창으로 비교해 경고와 최종 보고 줄을 남깁니다.

## 검증

| 검증 | 결과 |
|---|---|
| probe `event_clock`(core probe) | Linux x64 Release·Win32 x86 Debug 모두 `event_clock_probe=true`(`windows=10, over_limit=2, worst_ppm=41666, total_ppm=8333, ticks_fast=261, ticks_audio=240`), `core_probe_all=true` |
| probe `--jamma-input-timeline`(Win32) | `jamma_input_timeline_probe=true` |
| Win32 x86 Debug 빌드 | 성공. Win32 게임 실행은 하지 않았습니다 |
| pumpit8 입력 스크립트 1회, wayland, 80초 | 아래 표 |

| pumpit8 80초(수정 후) | 값 |
|---|---|
| 같은 실행의 시계 어긋남(RAW 대 steady) | 전체 +3.2%, 최악의 1초 +9.1%, 76초 중 49초가 0.5% 초과 |
| tick/초(2초 단위, 8초 이후) | 239.5~240.7 |
| 플레이 구간 MP3 공급 | 초당 15,912~16,046 byte, 큐 최소 229 ms, pcm-empty 0 |
| tick 예정/전달/폐기 | 17,741 / 17,621 / 120 |
| 읽힌 누름 | 스크립트와 같음(SERVICE 5) |

비교 대상인 수정 전의 값은 사용자의 로그입니다(tick 초당 241~264). 같은 조건의 수정 전 실행을 따로 돌리지는
않았습니다: 시계의 어긋남이 실행마다 달라 나란히 놓을 수 없고, 수정 후 실행의 보고가 어긋남과 tick 속도를
함께 보여 줍니다. 전 롬셋 조사는 하지 않았습니다.

로그: `build/task754-steady.err.log`. 시계 대조 스크립트: `build/clock753*.py`(저장소 밖).

## 정정 (커밋 5167040 이후)

과정 5에서 `systemd-timesyncd`를 시계를 늦추는 주체처럼 적었지만 확인된 것이 아니었습니다. 그 서비스의
기록은 부팅 때의 동기화 한 번뿐이고, 그 서비스는 `tick`을 바꾸는 방식으로 일하지 않습니다. 한 시간 뒤에도
`tick`은 9595~9600(−4%)이었고, 그때 WSL의 시각은 Windows보다 0.1~0.2초 **뒤**에 있었는데도 계속 늦추고
있었습니다. 누가 `tick`을 바꾸는지는 **미확인**입니다(배포판 안에서는 보이지 않음, WSL 2.4.12.0, 커널
5.15.167.4). 보통의 시계 보정은 0.05% 이내이므로 4%는 정상 범위가 아닙니다.

## 사용자 확인 (2026-09-28)

* **노트가 튀는(뒤로 돌아가는) 증상은 사라졌습니다.**
* **음악이 느려졌다 돌아오는 증상은 남아 있고, WSL을 다시 시작해도 사라지지 않았습니다.** 아래 "남은 것"에
  적었던 "`wsl --shutdown` 뒤에는 보정이 멈출 것"이라는 예상은 틀렸습니다.
* 사용자 결정: 이것은 WSL의 문제로 남겨 두고, **실기(실제 Linux 머신)에서 실행해 확인하는 것을 후속 작업**으로
  둡니다. 실기에서는 최종 보고의 `host clock raw-against-steady …` 줄(마지막 값이 0인지)과 음악의 속도를
  함께 봅니다.

## 남은 것

* **음악의 속도 자체**: WSL이 `CLOCK_MONOTONIC`을 늦추는 동안 오디오는 실제 시간으로 느리게 소비됩니다.
  엔진 밖의 일입니다. `wsl --shutdown` 뒤 다시 시작하면 시계 차이가 없어져 보정이 멈출 것으로 예상하지만
  확인하지 않았습니다(사용자의 WSL을 제가 재시작하지 않았습니다).
* 화면은 실제 60 Hz로 나가고 게스트의 시간과 음악은 느린 시계를 따르므로, 보정이 도는 동안 프레임당 게스트
  시간이 1~9% 짧습니다. 노트와 음악은 서로 맞지만 스크롤 속도는 그만큼 느려 보일 수 있습니다.
* Task 745~753의 fps·tick 속도 수치는 `CLOCK_MONOTONIC`으로 잰 것입니다. 시계가 늦춰진 동안 잰 값은
  실제보다 1~9% 높게 나왔을 수 있습니다(Task 752의 wayland 60.4~64.4 fps가 그 예).
* Task 753의 입력 결함은 실재했고 고쳐졌지만, 사용자가 본 증상의 원인은 아니었습니다.
* 사용자 확인이 필요합니다.

---

# English

# Task 754 work log — the tick clock follows the clock the audio follows

Design: [20260928-754](../design/20260928-754-tick-clock-follows-the-audio-clock.md)
Work order: [20260928-754](../work-orders/20260928-754-tick-clock-follows-the-audio-clock.md)

## Summary

The same symptom (the music slowing and coming back, the arrows going back) appeared on the Task 753
build, so the input defect 753 fixed was not its cause. The cause is that **the engine used two clocks,
and on this machine's WSL the two run at different rates**. The tick schedule followed SDL's clock
(`CLOCK_MONOTONIC_RAW`) and the audio and everything else `CLOCK_MONOTONIC`, which WSL's time
synchronisation was dragging 1–7% (up to 9% within a second) slower than real time. The guest's time ran
ahead of the music, so the arrows ran ahead and came back when the game realigned them to the MP3
position. With the tick schedule on `steady_clock`, a run in which the two clocks parted by 3.2% on
average (9.1% in the worst second) kept its ticks at 239.5–240.7 a second of the audio's clock.

The music itself slowing in real time and coming back comes from WSL's clock correction and **is not
fixed by this task**.

## Steps

1. The final report of the user's log showed Task 753 at work (SERVICE read 5 times, all 9,497 frames
   ended by return, pruned 44 of edges 44).
2. The census as rates per half second. The MP3 feed stayed at 15.7–16.2 KB a second to the end; ticks
   were 241–246 a second at 30–40 s and 257–264 at 40–44.9 s, where the symptom was. The selection
   screens (8–12 s) had a stretch at 260–263 too.
3. The only `[repiu-pit]` lines are the two at the start, so the frequency was fixed at 240.048 Hz, and
   the ticks due come from a clock, which made the clock the suspect. On WSL `CLOCK_MONOTONIC_RAW` ran
   0–4.2% faster than `CLOCK_MONOTONIC`, second by second.
4. Which of them is real time was settled against the Windows performance counter (a Windows process
   stamping the two readings WSL prints, 46 s): RAW within ±0.001% in every 5 s window, MONOTONIC −0.8%
   to −7.1%, −3.1% overall.
5. `adjtimex` (read only) showed `tick` moving between 10000, 9597 and 9788, `systemd-timesyncd` was
   running, and WSL's time was 0.7–0.85 s ahead of Windows.
6. `EventClockNanoseconds` returns `steady_clock`, and key events' timestamps are translated to it before
   the timeline records them. The two clocks are compared in windows of a second, for a warning and a
   line in the final report.

## Verification

| Check | Result |
|---|---|
| Probe `event_clock` (core probe) | `event_clock_probe=true` on Linux x64 Release and Win32 x86 Debug (`windows=10, over_limit=2, worst_ppm=41666, total_ppm=8333, ticks_fast=261, ticks_audio=240`), `core_probe_all=true` |
| Probe `--jamma-input-timeline` (Win32) | `jamma_input_timeline_probe=true` |
| Win32 x86 Debug build | succeeded. No Win32 game run was made |
| One scripted pumpit8 run, wayland, 80 s | the table below |

| pumpit8 for 80 s (after) | Value |
|---|---|
| The clocks' divergence in that run (RAW against steady) | +3.2% overall, +9.1% in the worst second, 49 of 76 seconds over 0.5% |
| Ticks a second (per 2 s, from 8 s on) | 239.5–240.7 |
| MP3 feed in play | 15,912–16,046 bytes a second, queue never under 229 ms, pcm-empty 0 |
| Ticks due / delivered / dropped | 17,741 / 17,621 / 120 |
| Presses read | the script's (SERVICE 5) |

The figures before the fix are the user's log (241–264 ticks a second). No separate run under the old
clock was made: the divergence differs from run to run so two runs cannot be set side by side, and the
run after the fix reports the divergence and the tick rate together. No survey of every ROM set was made.

Log: `build/task754-steady.err.log`. The clock comparison scripts: `build/clock753*.py` (outside the
repository).

## Correction (after commit 5167040)

Step 5 named `systemd-timesyncd` as if it were what slows the clock, which was not established. That
service logged one synchronisation at boot and does not work by changing `tick`. An hour later `tick`
was still 9595–9600 (−4%), and the clock was still being slowed although WSL's time was by then 0.1–0.2 s
**behind** Windows. Who changes `tick` is **not established** (nothing visible from inside the
distribution; WSL 2.4.12.0, kernel 5.15.167.4). Ordinary clock discipline stays within 0.05%, so 4% is
not in the normal range.

## The user's confirmation (2026-09-28)

* **The arrows no longer jump (go back).**
* **The music slowing and coming back remains, and restarting WSL did not remove it.** The expectation
  written under "What remains" below, that the correction would stop after `wsl --shutdown`, was wrong.
* The user's decision: this is left as WSL's problem, and **running on real hardware (an actual Linux
  machine) is the follow-up**. There, the final report's `host clock raw-against-steady …` line (whether
  its last value is 0) is to be read together with the music's speed.

## What remains

* **The music's own speed**: while WSL slows `CLOCK_MONOTONIC`, audio is consumed slowly in real time.
  That is outside the engine. Restarting WSL (`wsl --shutdown`) is expected to remove the clock offset
  and stop the correction, which was not checked (I did not restart the user's WSL).
* The screen goes out at a real 60 Hz while the guest's time and the music follow the slow clock, so
  while the correction runs a frame holds 1–9% less guest time. Arrows and music agree, but the scroll
  may look that much slower.
* The fps and tick rates of Tasks 745–753 were measured with `CLOCK_MONOTONIC`. Figures taken while the
  clock was being slowed may read 1–9% high (Task 752's 60.4–64.4 fps on wayland is an example).
* Task 753's input defect was real and is fixed, but it was not the cause of what the user saw.
* The user's confirmation is needed.
