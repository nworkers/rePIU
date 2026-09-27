# Task 755: WSL에서 음악이 전반적으로 느린 원인 조사

## 한국어

### 배경

사용자 보고(v0.0.194, WSLg): "pumpitea 같은 다른 게임을 실행해 보니 MP3 재생이 전반적으로 느려진 것 같다."
v0.0.194가 tick의 시계를 바꿨으므로(Task 754), 그 변경이 음악의 속도를 바꿨는지부터 가려야 한다.

### 조사 설계

수정 없이 측정만 한다. 대상은 보고된 게임 하나(pumpitea)다.

1. **엔진의 변경이 원인인가**: pumpitea를 같은 입력 스크립트로 tick 시계 둘(`steady` 기본, `REPIU_EVENT_CLOCK=sdl`)
   에서 75초씩 돌려 MP3 디코드 속도(초당 프레임)를 비교한다. 같으면 Task 754는 음악의 속도를 바꾸지 않았다.
2. **음악은 실제 시간으로 얼마나 느린가**: 게임 없이 사운드 서버의 출력만 잰다. sink의 monitor를 녹음해 초당
   프레임 수를 `CLOCK_MONOTONIC_RAW`(이 호스트에서 Windows 성능 카운터와 ±0.001%)와 `CLOCK_MONOTONIC`으로 센다.
3. **시계는 왜 계속 끌려가는가**: Windows의 시계와 NTP 서버의 차이, WSL 안의 시간 동기화 상태를 읽는다(읽기만).

시스템의 시각·서비스를 바꾸는 일은 하지 않는다(관리자 권한이 필요하고 사용자의 결정이다).

## English

### Background

The user's report (v0.0.194, WSLg): "running another game such as pumpitea, MP3 playback seems slower
overall." v0.0.194 changed the clock of the ticks (Task 754), so whether that change altered the music's
speed has to be settled first.

### Design of the investigation

Measurement only, no change. The subject is the one game reported (pumpitea).

1. **Is the engine's change the cause**: pumpitea under one input script on both tick clocks (`steady`,
   the default, and `REPIU_EVENT_CLOCK=sdl`) for 75 s each, comparing the MP3 decode rate (frames a
   second). If they agree, Task 754 did not alter the music's speed.
2. **How slow is the music in real time**: the sound server's output alone, with no game. The sink's
   monitor is recorded and its frames a second counted against `CLOCK_MONOTONIC_RAW` (within ±0.001% of
   the Windows performance counter on this host) and against `CLOCK_MONOTONIC`.
3. **Why does the clock keep being dragged**: the difference between the Windows clock and NTP servers,
   and the state of time synchronisation inside WSL (read only).

Nothing that changes the system's time or services is done (it needs administrator rights and is the
user's decision).
