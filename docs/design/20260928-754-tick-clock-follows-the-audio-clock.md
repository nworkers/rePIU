# Task 754: 타이머 tick의 시계를 오디오가 따르는 시계로 맞춘다

## 한국어

### 배경

사용자 보고(Task 753 빌드, pumpit8 곡 720, wayland): "음악 재생이 느려지는 듯하다가 정상으로 돌아오기도
하고, 그다음 노트가 원래 위치보다 뒤로 다시 돌아갔다." 같은 증상이 나와 바로 종료한 실행의 로그다.

Task 753의 입력 수정은 그 실행에서 동작하고 있었다(읽힌 SERVICE 누름 5회, 프레임 9,497개 모두 복귀로
끝남, history-pruned 44 / edges 44). 즉 증상의 원인은 입력이 아니었다.

로그의 census(MP3 공급·tick 누계)를 0.5초 단위 속도로 바꾸니 **tick 속도에 계단**이 있었다.

| 구간(census 시계) | tick/초 | MP3 byte/초 |
|---|---|---|
| 30.0~40.0초(플레이) | 241.4~245.7 | 15,697~16,213 |
| 40.0~44.9초(증상, 종료 직전) | 257.0~263.6 | 15,738~16,155 |
| 8~12초(선택 화면) | 260~263 | — |

PIT 설정은 바뀌지 않았다(`[repiu-pit]` 줄은 시작의 두 개뿐, 240.048 Hz). tick 예정 수는 호스트 루프가
시계에서 계산하므로, 같은 주파수에서 초당 tick 수가 달라졌다면 **두 시계의 속도가 다르다**는 뜻이다.

엔진의 시계는 둘이었다.

* tick 스케줄과 입력 타임라인: SDL의 시계(`SDL_GetTicksNS`). Linux에서 `CLOCK_MONOTONIC_RAW`.
* 그 밖의 전부 — MP3 재생 시계, swap 페이서, census, 그리고 오디오를 소비하는 사운드 서버: `steady_clock`,
  즉 `CLOCK_MONOTONIC`.

이 머신의 WSL에서 두 시계를 Windows의 성능 카운터와 대조했다(46초, 5초 창).

| 시계 | Windows 카운터 대비 |
|---|---|
| `CLOCK_MONOTONIC_RAW` | 모든 창에서 ±0.001% |
| `CLOCK_MONOTONIC` | 창마다 −0.8% ~ −7.1%, 전체 −3.1% |

`adjtimex`의 `tick`이 10000과 9597(−4.0%)·9788(−2.1%) 사이를 오가고 있었다: 무언가가 시계를 맞추려고
`CLOCK_MONOTONIC`을 몇 초씩 크게 늦추고 있다(그때 WSL의 시계는 Windows보다 0.7~0.85초 앞서 있었다). 누가
`tick`을 바꾸는지는 확인하지 못했다: 배포판 안에서 도는 `systemd-timesyncd`는 부팅 때의 동기화 한 번만
기록했고 `tick`을 쓰는 방식이 아니며, 배포판 안에서는 다른 동기화 프로세스가 보이지 않았다. 보통의 시계
보정은 0.05% 이내인데 이것은 4%다. 오디오는 그 시계를 따라 소비되므로(census의 MP3 공급이 두 구간 모두 그 시계로 초당 16 KB)
음악은 실제 시간으로 느려졌다 돌아오고, tick은 RAW를 따라 실제 속도로 가므로 **게스트의 시간이 음악보다 최대
9% 빨리 간다**. 게임은 노트를 tick으로 굴리다가 MP3 위치에 다시 맞추므로 노트가 앞서 갔다가 뒤로 돌아온다.
사용자의 설명과 순서까지 같다.

Task 752에서 wayland의 fps가 60.4~64.4로 찍힌 것도 같은 현상이다(진짜 vsync는 실제 60 Hz이고 fps를 재는
시계가 느렸다).

### 설계

1. **tick 스케줄과 입력 타임라인의 시계를 `steady_clock`으로 바꾼다.** `EventClockNanoseconds`가
   `steady_clock`을 돌려준다. tick은 음악과 박자를 맞추기 위한 것이므로 음악이 따르는 시계를 따라야 한다.
   시계가 가만히 있는 머신에서는 두 시계의 속도가 같아 달라지는 것이 없다. Win32에서는 둘 다 성능
   카운터다. `REPIU_EVENT_CLOCK=sdl`은 이전처럼 SDL의 시계를 쓴다.
2. **SDL 이벤트의 시각을 옮긴다.** 키 이벤트의 timestamp는 SDL 시계의 값이다. 이벤트의 나이(SDL 시계의
   지금 − timestamp)를 구해 `steady_clock`의 지금에서 뺀다. 오차는 두 시계의 속도 차 × 나이(몇 ms)다.
3. **시계가 어긋나면 말한다.** 두 시계를 1초 창으로 비교해 0.5%를 넘는 첫 창에서 stderr에
   `[repiu-clock] …`을 한 번 찍고, 최종 보고에 `host clock raw-against-steady tick-clock/total-ppm/
   worst-second-ppm/seconds/seconds-over-0.5%`를 남긴다. 평균이 아니라 창으로 보는 까닭은 slew가 몇 초씩
   몰려 오기 때문이다.

### 이 작업이 고치지 못하는 것

음악 자체가 실제 시간으로 느려졌다 돌아오는 것은 WSL의 시계 보정과 사운드 서버 사이의 일이고 엔진 밖이다.
이 작업은 게스트의 시간을 음악과 같은 시계에 묶어 **노트와 음악이 서로 어긋나지 않게** 할 뿐이다. 시계
보정 자체는 WSL을 다시 시작하면(`wsl --shutdown`) 시계 차이가 없어져 멈출 것으로 예상하지만 확인하지
않았다.

### 검증 전략

* probe(`event_clock`, core probe에 포함되어 Linux·Win32 모두에서 돈다): 스위치, timestamp 옮기기, 어긋남
  계산, 창 단위 계측(10초 중 2초가 4% 어긋난 경우), 240 Hz tick이 9% 빠른 시계에서 261개가 되는 것.
* pumpit8 입력 스크립트 1회(wayland, 80초): census 시계로 본 tick 속도가 240에 머무는지, 같은 실행의
  시계 어긋남 보고와 함께 본다. 전 롬셋 조사는 하지 않는다.

## English

### Background

The user's report (the Task 753 build, pumpit8's song 720, wayland): "the music seems to slow down and
then comes back to normal, and after that the arrows went back behind where they had been." The log is of
a run ended as soon as the symptom appeared.

Task 753's input fix was at work in that run (SERVICE read 5 times, all 9,497 frames ended by return,
history-pruned 44 of edges 44), so the input was not the cause of the symptom.

Turning the log's census (the MP3 feed and the tick total) into rates per half second showed **a step in
the tick rate**.

| Interval (census clock) | ticks a second | MP3 bytes a second |
|---|---|---|
| 30.0–40.0 s (play) | 241.4–245.7 | 15,697–16,213 |
| 40.0–44.9 s (the symptom, just before the exit) | 257.0–263.6 | 15,738–16,155 |
| 8–12 s (selection screens) | 260–263 | — |

The PIT was not reprogrammed (the only `[repiu-pit]` lines are the two at the start, 240.048 Hz). The
host loop computes the ticks due from a clock, so a different number of ticks a second at one frequency
means **two clocks running at different rates**.

The engine had two clocks.

* The tick schedule and the input timeline: SDL's clock (`SDL_GetTicksNS`), which on Linux reads
  `CLOCK_MONOTONIC_RAW`.
* Everything else — the MP3 playback clock, the swap pacer, the census, and the sound server that consumes
  the audio: `steady_clock`, that is `CLOCK_MONOTONIC`.

On this machine's WSL the two were compared against the Windows performance counter (46 s, windows of
5 s).

| Clock | Against the Windows counter |
|---|---|
| `CLOCK_MONOTONIC_RAW` | within ±0.001% in every window |
| `CLOCK_MONOTONIC` | −0.8% to −7.1% by window, −3.1% overall |

`adjtimex`'s `tick` was moving between 10000 and 9597 (−4.0%) or 9788 (−2.1%): something is slowing
`CLOCK_MONOTONIC` hard for seconds at a time to set the clock right (WSL's clock was 0.7–0.85 s ahead of
Windows then). Who changes `tick` was not established: `systemd-timesyncd`, which runs in the
distribution, logged one synchronisation at boot and does not work through `tick`, and no other
synchronisation process was visible from inside the distribution. Ordinary clock discipline stays within
0.05%; this is 4%. Audio is consumed by that clock (the census's MP3 feed is 16 KB a second of that clock
in both intervals), so the music slows in real time and comes back, while ticks follow RAW at the real
rate, so **the guest's time runs up to 9% ahead of the music**. The game scrolls its arrows by ticks and
then realigns them to the MP3 position, so the arrows run ahead and come back. It matches the user's
description down to the order.

Task 752's wayland figures of 60.4–64.4 fps were the same thing (real vsync is a real 60 Hz and the clock
measuring fps was slow).

### Design

1. **The tick schedule and the input timeline move to `steady_clock`.** `EventClockNanoseconds` returns
   `steady_clock`. Ticks exist to keep time with the music, so they must follow the clock the music
   follows. On a machine whose clock is left alone the two run at one rate and nothing changes; on Win32
   both are the performance counter. `REPIU_EVENT_CLOCK=sdl` uses SDL's clock as before.
2. **SDL event times are translated.** A key event's timestamp is of SDL's clock. The event's age (SDL's
   present minus the timestamp) is taken off `steady_clock`'s present; the error is the clocks' rate
   difference over that age, a few ms.
3. **A divergence is reported.** The two clocks are compared in windows of one second; the first window
   over 0.5% prints `[repiu-clock] …` to stderr once, and the final report gains `host clock
   raw-against-steady tick-clock/total-ppm/worst-second-ppm/seconds/seconds-over-0.5%`. Windows rather
   than an average, because the slew comes in bursts of seconds.

### What this task does not fix

The music itself slowing in real time and coming back is between WSL's clock correction and the sound
server, outside the engine. This task ties the guest's time to the clock the music follows so that **the
arrows and the music do not part**. The correction itself is expected to stop when WSL is restarted
(`wsl --shutdown`), which removes the clock offset; that was not checked.

### Verification strategy

* The probe (`event_clock`, part of the core probe and so run on Linux and Win32): the switch, the
  translation of timestamps, the divergence arithmetic, the metering by window (2 seconds of 10 off by
  4%), and 240 Hz ticks being 261 on a clock 9% fast.
* One scripted pumpit8 run (wayland, 80 s): whether the tick rate on the census clock stays at 240, read
  beside the same run's report of the clocks' divergence. No survey of every ROM set.
