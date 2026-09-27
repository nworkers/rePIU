# Task 755 작업 로그 — WSL에서 음악이 전반적으로 느린 원인

설계: [20260928-755](../design/20260928-755-wsl-audio-rate-and-clock-tug-of-war.md)
작업 지시: [20260928-755](../work-orders/20260928-755-wsl-audio-rate-and-clock-tug-of-war.md)

## 요약

음악이 느린 것은 **엔진이 아니라 WSL의 사운드 서버가 실제 시간보다 느리게 출력하기 때문**이고, v0.0.194의
변경과 무관합니다. 사운드 서버는 `CLOCK_MONOTONIC`으로 초당 44,100프레임을 정확히 내지만, 그 시계가 실제
시간보다 1~8% 느리게 가고 있어 실제 시간으로는 초당 40,390~43,553프레임입니다(게임 없이 측정).

그 시계가 계속 끌려가는 까닭으로 **유력한 것**은 두 시간 동기화의 줄다리기입니다. Windows의 시계가 NTP
시각보다 약 1.0초 늦고(Windows Time 서비스가 꺼져 있음), WSL 안의 `systemd-timesyncd`는 NTP 시각으로, WSL의
호스트 동기화는 Windows 시각으로 맞추려 합니다. 이것은 확인된 사실들에 맞는 **가설**이며, 조치로 확인한 것은
아닙니다(시스템의 시각과 서비스를 제가 바꾸지 않았습니다).

## 측정

**1. 엔진의 변경이 원인인가 — 아니다.** pumpitea, 입력 스크립트, wayland, 75초씩.

| tick 시계 | tick/초 | MP3 디코드(프레임/초, 곡 구간) | MP3 공급(byte/초) | 시계 어긋남(전체 / 최악의 1초) |
|---|---|---|---|---|
| `steady`(v0.0.194 기본) | 239.6~240.3 | 38.0~38.7 | 15,935~16,028 | +5.4% / +8.8% |
| `sdl`(v0.0.193의 동작) | 241.5~267.2 | 37.9~38.8 | 15,880~16,000 | +5.5% / +11.1% |

MP3의 명목 속도는 초당 38.28프레임(44.1 kHz, 1,152샘플)입니다. 두 실행 모두 `CLOCK_MONOTONIC`의 1초마다 그
속도로 디코드했고, pcm-empty는 0, starvation은 4~5였습니다. 두 실행 모두 71초 가운데 71초가 0.5% 넘게
어긋났습니다(Task 754의 측정 때는 76초 중 49초, 전체 +3.2%).

**2. 사운드 서버의 실제 출력 속도.** sink의 monitor를 40초 녹음, 게임 없음.

| 구간 | 실제 시간(RAW) 기준 프레임/초 | `CLOCK_MONOTONIC` 기준 프레임/초 | 그 구간의 시계 |
|---|---|---|---|
| 6~13초 | 40,390 | 44,099 | −8.41% |
| 13~20초 | 40,791 | 44,099 | −7.50% |
| 20~26초 | 41,407 | 44,099 | −6.10% |
| 26~33초 | 42,051 | 44,103 | −4.65% |
| 33~39초 | 43,553 | 44,097 | −1.23% |
| 전체 | 41,296 | 43,539 | −5.15% |

(첫 6초 창은 녹음 시작의 영향이 있어 표에서 뺐습니다: 39,506 / 40,573.)

**3. 시계가 끌려가는 까닭.**

| 읽은 것 | 값 |
|---|---|
| Windows 시계 대 `time.windows.com` | +1.012초(서버가 앞섬), 두 번 |
| Windows 시계 대 `ntp.ubuntu.com` | +1.033~+1.045초, 세 번 |
| Windows Time 서비스 | 시작되지 않음(`w32tm /query /status` 0x80070426) |
| WSL `systemd-timesyncd` | `ntp.ubuntu.com`, 폴 간격 32초(최솟값), offset +1.90초, 패킷 111개, jitter 66 ms |
| `adjtimex`의 `tick` | 9492(−5.1%) ~ 10137(+1.4%) 사이를 움직임 |
| WSL 배포판 | Ubuntu-24.04 하나뿐 |
| WSL 시스템 배포판(CBL-Mariner) | `chrony.conf`(`server time.windows.com`)는 있으나 `chronyd` 프로세스는 보이지 않음 |

`systemd-timesyncd`는 어긋남이 0.4초를 넘으면 시각을 한 번에 옮기고(step), `tick`을 바꾸지 않습니다. 폴 간격이
최솟값에 붙어 있다는 것은 맞춘 시각이 매번 다시 어긋난다는 뜻입니다. `tick`을 바꾸는 쪽은 여전히 확인하지
못했습니다. 관측된 최대 slew(−8.4%)는 chrony의 기본 최대 slew 속도(8.33%)와 같은 크기입니다.

## 해석

* 확인된 것: 음악의 속도는 사운드 서버의 출력 속도이고, 그것은 `CLOCK_MONOTONIC`을 따르며, 그 시계가 실제
  시간보다 느립니다. 엔진은 그 시계의 1초마다 정확한 양을 공급합니다.
* v0.0.194 이후 게임의 시간(tick)도 같은 시계를 따르므로 **게임 전체가 음악과 함께 느려집니다**. v0.0.193에서는
  음악만 느리고 노트는 실제 속도로 가서 둘이 어긋났습니다.
* 가설: NTP 시각과 Windows 시각이 1초 다르고, WSL 안에서 둘을 각각 따르는 동기화가 서로의 결과를 되돌립니다.
  그래서 보정이 끝나지 않습니다.

## 사용자가 해 볼 수 있는 조치 (확인되지 않음)

1. **Windows의 시계를 맞춥니다.** 설정 → 시간 및 언어 → 날짜 및 시간 → "지금 동기화". 또는 관리자
   PowerShell에서 `Start-Service w32time; w32tm /resync`. Windows와 NTP가 같아지면 두 동기화가 다툴 이유가
   없어집니다.
2. 또는 WSL 안의 NTP 동기화를 끕니다: `sudo systemctl disable --now systemd-timesyncd`. 호스트 동기화만
   남습니다.

조치 뒤에는 게임의 최종 보고 `host clock raw-against-steady …` 줄의 마지막 값(`seconds-over-0.5%`)이 0인지로
판정합니다.

## 사용자 확인과 보류 (2026-09-28)

* 사용자가 Windows의 시계를 "지금 동기화"했지만 **MP3 재생은 그대로 느렸습니다.**
* 그 실행의 로그는 받지 않았으므로, 동기화 뒤에 시계의 slew가 멈췄는지는 **알 수 없습니다**. 따라서 둘 중
  어느 쪽인지 가리지 못했습니다: (가) 줄다리기 가설이 틀려 slew가 계속됐다, (나) slew는 멈췄는데 음악이 느린
  원인이 따로 있다.
* 사용자 결정: **보류**. 실기(실제 Linux 머신)에서의 비교가 있어야 판단할 수 있습니다. WSL 쪽은 더 조사하지
  않습니다.
* 실기에서 볼 것: 최종 보고의 `host clock raw-against-steady …` 줄과 음악의 속도. 실기에서도 느리면 시계가
  아니라 엔진 쪽을 다시 봐야 합니다.

## 검증

코드를 바꾸지 않았으므로 빌드와 probe는 돌리지 않았습니다. 실행은 pumpitea 2회(75초)와 사운드 서버 측정
1회(40초)입니다.

로그: `build/task755-steady.err.log`, `build/task755-sdl.err.log`. 측정 도구(저장소 밖):
세션 scratchpad의 `parate.c`, `build/clock753*.py`.

## 남은 것

* `tick`을 바꾸는 주체는 미확인입니다.
* 위 조치가 slew를 멈추는지는 미확인입니다.
* 실기(실제 Linux 머신)에서의 확인은 그대로 후속 작업입니다.

---

# English

# Task 755 work log — why the music is slower overall on WSL

Design: [20260928-755](../design/20260928-755-wsl-audio-rate-and-clock-tug-of-war.md)
Work order: [20260928-755](../work-orders/20260928-755-wsl-audio-rate-and-clock-tug-of-war.md)

## Summary

The music is slow **because WSL's sound server puts audio out slower than real time, not because of the
engine**, and v0.0.194's change has nothing to do with it. The sound server produces exactly 44,100
frames a second of `CLOCK_MONOTONIC`, and that clock is running 1–8% slower than real time, so in real
time it produces 40,390–43,553 frames a second (measured with no game running).

The **likely** reason the clock keeps being dragged is a tug of war between two time synchronisations.
The Windows clock is about 1.0 s behind NTP time (the Windows Time service is not running), and inside
WSL `systemd-timesyncd` sets the clock to NTP time while WSL's host synchronisation sets it to Windows
time. This is a **hypothesis** that fits what was established; it was not confirmed by acting on it (I
changed neither the system's time nor its services).

## Measurements

**1. Is the engine's change the cause — no.** pumpitea, input script, wayland, 75 s each.

| Tick clock | ticks a second | MP3 decode (frames a second, in the song) | MP3 feed (bytes a second) | Divergence (overall / worst second) |
|---|---|---|---|---|
| `steady` (v0.0.194's default) | 239.6–240.3 | 38.0–38.7 | 15,935–16,028 | +5.4% / +8.8% |
| `sdl` (v0.0.193's behaviour) | 241.5–267.2 | 37.9–38.8 | 15,880–16,000 | +5.5% / +11.1% |

The MP3's nominal rate is 38.28 frames a second (44.1 kHz, 1,152 samples). Both runs decoded at that rate
per second of `CLOCK_MONOTONIC`, with pcm-empty 0 and starvation 4–5. In both runs 71 of 71 seconds were
over 0.5% apart (49 of 76 and +3.2% overall when Task 754 was measured).

**2. The sound server's real output rate.** The sink's monitor recorded for 40 s, no game.

| Interval | frames a second of real time (RAW) | frames a second of `CLOCK_MONOTONIC` | the clock in that interval |
|---|---|---|---|
| 6–13 s | 40,390 | 44,099 | −8.41% |
| 13–20 s | 40,791 | 44,099 | −7.50% |
| 20–26 s | 41,407 | 44,099 | −6.10% |
| 26–33 s | 42,051 | 44,103 | −4.65% |
| 33–39 s | 43,553 | 44,097 | −1.23% |
| Overall | 41,296 | 43,539 | −5.15% |

(The first window of 6 s carries the start of the recording and is left out of the table: 39,506 /
40,573.)

**3. Why the clock is dragged.**

| What was read | Value |
|---|---|
| The Windows clock against `time.windows.com` | +1.012 s (the server ahead), twice |
| The Windows clock against `ntp.ubuntu.com` | +1.033 to +1.045 s, three times |
| The Windows Time service | not started (`w32tm /query /status`, 0x80070426) |
| WSL's `systemd-timesyncd` | `ntp.ubuntu.com`, poll interval 32 s (the minimum), offset +1.90 s, 111 packets, jitter 66 ms |
| `adjtimex`'s `tick` | moving between 9492 (−5.1%) and 10137 (+1.4%) |
| WSL distributions | Ubuntu-24.04 alone |
| WSL's system distribution (CBL-Mariner) | has a `chrony.conf` (`server time.windows.com`) but no `chronyd` process was visible |

`systemd-timesyncd` steps the time at once when the offset is over 0.4 s and does not change `tick`. A
poll interval pinned at its minimum means the time it sets is off again every time. What changes `tick`
is still not established. The largest slew observed (−8.4%) is the size of chrony's default maximum slew
rate (8.33%).

## Reading

* Established: the music's speed is the sound server's output rate, which follows `CLOCK_MONOTONIC`,
  which runs slower than real time. The engine supplies the right amount per second of that clock.
* Since v0.0.194 the game's time (ticks) follows the same clock, so **the whole game slows with the
  music**. On v0.0.193 the music alone was slow and the arrows ran at the real rate, so the two parted.
* Hypothesis: NTP time and Windows time differ by a second, and inside WSL the synchronisations following
  each undo one another's result, so the correction never ends.

## What the user can try (not confirmed)

1. **Set the Windows clock.** Settings → Time & language → Date & time → "Sync now", or in an
   administrator PowerShell `Start-Service w32time; w32tm /resync`. With Windows and NTP agreeing the two
   synchronisations have nothing to fight over.
2. Or turn NTP synchronisation off inside WSL: `sudo systemctl disable --now systemd-timesyncd`, which
   leaves the host synchronisation alone.

Afterwards the judge is the last value (`seconds-over-0.5%`) of the game's final report line `host clock
raw-against-steady …`: 0 means the clock is left alone.

## The user's check, and the hold (2026-09-28)

* The user synchronised the Windows clock ("Sync now") and **MP3 playback was as slow as before.**
* That run's log was not received, so whether the clock's slew stopped after the synchronisation is
  **not known**. Which of two it is was therefore not settled: (a) the tug-of-war hypothesis is wrong and
  the slew went on, or (b) the slew stopped and the music is slow for another reason.
* The user's decision: **on hold**. A comparison on real hardware (an actual Linux machine) is needed to
  judge. WSL is not investigated further.
* What to read on real hardware: the final report's `host clock raw-against-steady …` line and the
  music's speed. If it is slow there too, the engine has to be looked at again, not the clock.

## Verification

No code was changed, so no build or probe was run. The runs were two of pumpitea (75 s) and one
measurement of the sound server (40 s).

Logs: `build/task755-steady.err.log`, `build/task755-sdl.err.log`. The measuring tools (outside the
repository): `parate.c` in the session's scratchpad, `build/clock753*.py`.

## What remains

* What changes `tick` is not established.
* Whether the steps above stop the slew is not confirmed.
* Checking on real hardware (an actual Linux machine) remains the follow-up.
