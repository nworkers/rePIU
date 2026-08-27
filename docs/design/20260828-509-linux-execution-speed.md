# Task 509 — Linux 실행 속도: 재는 것부터

작업 지시: [20260828-509](../work-orders/20260828-509-linux-execution-speed.md) ·
작업 로그: [20260828-509](../work-logs/20260828-509-linux-execution-speed.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260827-506](20260827-506-linux-aot-code-cache.md) ·
[20260828-508](20260828-508-refused-recovery-teardown.md)

## 배경 — "느리다"는 아직 고칠 수 있는 문장이 아닙니다

Task 506이 화면을 열었고 사용자가 **게임 화면이 나오는 것을 직접 확인했습니다**(2026-08-28).
같은 관측이 남긴 것이 **"속도가 아주 느리다"**입니다.

이 저장소는 Windows 쪽에서 같은 종류의 문장을 여러 번 숫자로 바꿔 봤고, 그때마다 순서가
같았습니다 — **먼저 재고, 그다음 귀속하고, 마지막에 고칩니다.** 재기 전에 원인을 고르면
Task 418이 겪은 일이 반복됩니다: 그 세션은 재기준선을 잡자마자 **축이 두 번 바뀌었습니다.**

그래서 509는 "무엇이 느린가"를 묻지 않습니다. **얼마나 느린가**를 묻습니다.

## 확인됨 — 지금 Linux 실행은 자기 속도를 말하지 않습니다

재려고 했더니 잴 것이 없었습니다. 세 가지를 확인했습니다.

**하나. 렌더까지 간 Linux 실행은 요약을 출력하지 않습니다.** 프레임 수가 들어 있는 로더
요약은 종료 블록을 끝까지 지나야 나오는데, 60초 예산 실행 6회가 **6회 모두** 회수를
거절당하는 갈래로 갑니다(Task 508). 그 갈래는 `_Exit`으로 끝나므로 요약이 없습니다. 실제로
그 로그에서 프레임을 찾으면 이것이 전부입니다.

```
$ grep -aiE "frame|swap" build/task508-after-run6.err | tail -4
[repiu-live-debug] grLfbLock seed #1 read=1 encode=1 framebuffer non-black=0
...
```

**이것은 508의 부작용이 아니라 508이 드러낸 것입니다.** 508 이전에도 같은 실행은 요약을
내지 못했습니다 — 그때는 코어 덤프로 죽었기 때문입니다.

**둘. Windows는 같은 조건에서 요약이 나옵니다.** 회수는 똑같이 40회 모두 거절당하지만
`TerminateThread`가 게스트 스레드를 세워 `stopped=1`이 되기 때문입니다. 그래서 Windows
60초 Debug 실행에서는 이 줄이 나옵니다.

```
Win32 Glide call trace: ordinal=85 name=_GRBUFFERSWAP@4 count=2831
```

**대조군만 숫자를 낼 수 있는 상태**이고, 이대로는 비교가 성립하지 않습니다.

**셋. FPS는 이미 계산되고 있는데 창 제목으로만 갑니다.** `RecordPresentedFrame()`이 초당
한 번 FPS를 내어 `SDL_SetWindowTitle`로 보냅니다. 밖에서 읽으려면 X11 도구가 필요한데
이 WSL에는 **하나도 없습니다.**

```
xdotool: MISSING   wmctrl: MISSING   xprop: MISSING   xwininfo: MISSING
```

패키지를 더 넣어 창 제목을 긁는 방법은 있지만, **반복 절차가 창 관리자에 의존하게 됩니다.**
헤드리스에서도, 로그만 남은 실행에서도 못 씁니다.

## 결정 1 — 실행이 자기 프레임 수를 종료 줄에 싣습니다

507이 넣은 `[repiu-shutdown]` 한 줄은 **두 갈래 모두에서, 버퍼링 없이** 나옵니다. 프레임
수를 실을 자리로 이보다 나은 곳이 없습니다.

```
[repiu-shutdown] reason=timeout … frames=1234 span_ms=14567
```

* `frames` — 표시된 프레임 총계(단조 증가).
* `span_ms` — **첫 프레임부터** 종료까지의 벽시계 시간. 예산 전체가 아닙니다. `pumpit1`은
  기동 자산 디코드에 약 45초를 쓰므로, 예산으로 나누면 실제보다 몇 배 낮게 나옵니다.

**경합이 없습니다.** 이 두 값은 호스트 스레드만 씁니다 — 스왑을 실행하는 스레드가 곧 종료
블록을 도는 스레드입니다. 508이 "도는 게스트 스레드와 경합하는 일을 하지 말라"고 정한 것과
어긋나지 않습니다.

## 결정 2 — 초당 FPS를 선택적으로 로그에도 냅니다

`REPIU_GLIDE_FRAME_RATE_LOG=1`이면 창 제목에 쓰는 것과 **같은 값**을 오류 스트림에도
한 줄 냅니다.

평균 하나로는 부족하기 때문입니다. 이 저장소는 **"관측 창보다 긴 주기는 보이지 않는다"**에
이번 이식에서만 두 번 걸렸습니다(frontier 3.5절) — 30초는 "느리다", 240초는 "갇혔다",
1,200초에서야 실상이었습니다. 시간별 값이 있으면 "일정하게 느린가, 어디서 무너지는가"를
같은 실행 하나로 구분할 수 있습니다.

계산은 이미 있으므로 새로 도는 것은 없습니다. 꺼져 있으면 문자열도 만들지 않습니다.

## 결정 3 — 측정 조건을 먼저 못 박습니다

| 항목 | 값 | 왜 |
|---|---|---|
| 빌드 | **Release, 양쪽 호스트** | Debug는 이 저장소에서 11.34배 계수와 **단계 순위 역전**이 확인된 적이 있습니다(Task 330) |
| vsync | **OFF** (`REPIU_GLIDE_SWAP_INTERVAL=0`) | 켜져 있으면 CPU가 아니라 디스플레이 대기를 재게 됩니다(Task 440에서 32.8% 대 2.9%) |
| 감시견 | `REPIU_STALL_TIMEOUT_MS=0` | 건강한 실행을 정지로 오판한 전력 |
| 장면 | `pumpit1`, 같은 예산 | 장면이 다르면 비교가 성립하지 않습니다 |
| 반복 | **호스트당 3회** | 이 저장소는 1회 실행을 판정으로 쓰지 않습니다 |

**Release가 Linux에서 빌드된 적이 없습니다.** 이것이 이 작업의 첫 위험이고, 안 되면 Debug
대 Debug로 내려가되 **그 사실을 숫자 옆에 적습니다.**

## 이 작업이 답하지 않는 것

**"어디가 느린가"는 509의 범위가 아닙니다.** 귀속 노브(`REPIU_GLIDE_ORDINAL_TIME_PROFILE`,
`REPIU_AOT_RETURN_STAGE_PROFILE`)는 이미 있지만, 배율을 모르는 채 그것을 켜면 읽을 기준이
없습니다. 그리고 **Windows의 순위를 Linux에 옮겨 읽으면 안 됩니다** — 예외 전달, 시그널,
GL 드라이버가 전부 다른 호스트입니다. 이것은 510의 일입니다.

**WSLg와 실제 데스크톱의 차이**도 나누지 않습니다. WSLg는 X11을 한 겹 더 지나므로 present
비용이 다를 수 있고, 그 분리는 측정값이 하나 생긴 뒤에야 의미가 있습니다.

**렌더 정확성**(Windows와 프레임 단위 대조)은 별도 축입니다.

---

# Task 509 — Linux execution speed: measuring it first

Work order: [20260828-509](../work-orders/20260828-509-linux-execution-speed.md) ·
Work log: [20260828-509](../work-logs/20260828-509-linux-execution-speed.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessors: [20260827-506](20260827-506-linux-aot-code-cache.md) ·
[20260828-508](20260828-508-refused-recovery-teardown.md)

## Background — "slow" is not yet a sentence anything can be done about

Task 506 opened the screen, and the user confirmed by looking that **an actual game screen appears**
(2026-08-28). What the same observation left is **"it is very slow"**.

This repository has turned that kind of sentence into numbers several times on the Windows side, and
the order was always the same: **measure, then attribute, then fix.** Choosing a cause before
measuring repeats what Task 418 ran into -- that session re-baselined and **the axis moved twice**
immediately.

So 509 does not ask what is slow. It asks **how slow**.

## Confirmed — a Linux run does not currently report its own speed

The attempt to measure found nothing to measure. Three things were established.

**One: a Linux run that reaches rendering prints no summary.** The loader summary that carries the
frame count only appears after the shutdown block runs to the end, and six of six 60-second-budget
runs take the refused arm (Task 508), which ends at `_Exit`. Searching those logs for a frame count
returns this and nothing else:

```
$ grep -aiE "frame|swap" build/task508-after-run6.err | tail -4
[repiu-live-debug] grLfbLock seed #1 read=1 encode=1 framebuffer non-black=0
...
```

**This is not a side effect of 508 but something 508 exposed** -- before it, the same runs did not
produce the summary either, because they died with a core dump.

**Two: Windows does print it under the same conditions**, because all forty attempts are refused
there too but `TerminateThread` stops the guest thread, making `stopped=1`. So a 60-second Windows
Debug run gives:

```
Win32 Glide call trace: ordinal=85 name=_GRBUFFERSWAP@4 count=2831
```

**Only the control group can produce a number**, and no comparison stands on that.

**Three: the FPS is already computed and goes only to the window title.** `RecordPresentedFrame()`
produces one FPS value a second and hands it to `SDL_SetWindowTitle`. Reading it from outside needs
an X11 tool, and this WSL has **none of them**:

```
xdotool: MISSING   wmctrl: MISSING   xprop: MISSING   xwininfo: MISSING
```

Installing more packages to scrape a window title is possible, but it makes the repeatable procedure
depend on a window manager -- useless headless, and useless on a run that left only a log.

## Decision 1 — the run carries its own frame count on the shutdown line

The one `[repiu-shutdown]` line 507 added is printed **on both arms, unbuffered**. There is no better
place to put a frame count.

```
[repiu-shutdown] reason=timeout … frames=1234 span_ms=14567
```

* `frames` -- total presented frames, monotonic.
* `span_ms` -- wall time **from the first frame** to shutdown, not the whole budget. `pumpit1` spends
  about 45 seconds decoding start-up assets, so dividing by the budget understates the rate several
  times over.

**There is no race.** Both values are written only by the host thread -- the thread that performs the
swap is the thread that runs the shutdown block. This does not contradict 508's rule against doing
work that races the running guest thread.

## Decision 2 — the per-second FPS can also go to the log

With `REPIU_GLIDE_FRAME_RATE_LOG=1`, the **same value** that goes to the window title is also written
as one line on the error stream.

One average is not enough. This repository was caught twice in this port alone by **not seeing a
period longer than the observation window** (frontier section 3.5): thirty seconds said "slow", 240
said "trapped", and only 1,200 showed what was happening. A value over time separates "uniformly
slow" from "collapses somewhere" within a single run.

The computation already runs, so nothing new is added to the frame path. With the switch off, not
even the string is built.

## Decision 3 — the measurement conditions are fixed first

| Item | Value | Why |
|---|---|---|
| Build | **Release, both hosts** | Debug has a measured 11.34x factor in this repository and **inverts the stage ranking** (Task 330) |
| vsync | **off** (`REPIU_GLIDE_SWAP_INTERVAL=0`) | left on, what gets measured is display wait rather than CPU (32.8% against 2.9% in Task 440) |
| Watchdog | `REPIU_STALL_TIMEOUT_MS=0` | it has misjudged a healthy run as stalled before |
| Scene | `pumpit1`, the same budget | a different scene is not a comparison |
| Repeats | **three per host** | this repository does not treat a single run as a verdict |

**Release has never been built on Linux.** That is this task's first risk; if it does not build, the
fallback is Debug against Debug, **with that fact written next to the number.**

## What this task does not answer

**"Where is it slow" is out of 509's scope.** The attribution knobs
(`REPIU_GLIDE_ORDINAL_TIME_PROFILE`, `REPIU_AOT_RETURN_STAGE_PROFILE`) already exist, but turning
them on without knowing the factor leaves nothing to read them against. And **Windows' ranking must
not be carried over and read as Linux's** -- different host, different exception delivery, signals
and GL driver. That is 510's work.

**WSLg against a real desktop** is not separated either. WSLg goes through one more layer of X11, so
present cost may differ, and that separation only means something once a number exists.

**Rendering accuracy** (a frame-by-frame comparison against Windows) is a separate axis.
