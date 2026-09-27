# Task 753: 입력 replay 프레임을 핸들러의 복귀로 끝내고, 남은 것은 나이로 회수한다

## 한국어

### 배경

사용자 보고: Task 752 빌드(wayland, D3D12, 진짜 vsync)로 pumpit8의 곡 720을 플레이했는데 "문제가 다시
나왔다". 그 실행의 stderr(`repiu_log.txt`, 43초)에서 프레임·MP3·tick 수치는 정상이었다.

| 항목 | 값 |
|---|---|
| 렌더러 / swap interval | `D3D12 (NVIDIA GeForce RTX 4090)` / 1 적용, 페이싱 비활성 |
| 플레이 구간(26~48초) MP3 공급 | 초당 15.5~17.0 KB, 토글 38~39회, 큐 최소 229 ms, starvation 증가 없음, pcm-empty 0 |
| 플레이 구간 25 ms 초과 프레임 | 2개(present 23 ms, 27 ms) |
| tick | 10,821 예정 / 10,701 전달 / 120 폐기, 최대 backlog 5 |

정상이 아닌 것은 입력이었다. `[repiu-input] SERVICE PRESSED`가 **1,897회**(초당 약 60회) 찍혔고, 같은
실행의 실제 키 edge는 76개뿐이었다. 로그의 순서는 늘 `SERVICE PRESSED` + `P2-Center released`와
`SERVICE released` + `P2-Center PRESSED`의 교대였다. 즉 게스트의 읽기가 두 상태를 번갈아 받았다: 지금의
키(사용자가 누르고 있는 P2 키)와, **SERVICE(F2)를 누르고 있던 옛 시점의 키**.

입력 타임라인(`JammaInputTimeline`)은 tick을 주입할 때 그 tick의 예정 시각을 replay 프레임으로 쌓고,
게스트의 입력 포트 읽기에 그 시각의 키 상태를 돌려준다(핸들러가 늦게 돌아도 키 edge를 제 순서로 보게
하려는 것). 프레임의 회수는 **스택 비교 하나**뿐이었다: 읽기나 다음 주입의 ESP가 프레임의 ESP보다 높을
때. 게스트가 다시 올라오지 않는 스택 높이에서 주입된 tick의 프레임은 영영 회수되지 않고 바닥에 남는다.
그 뒤로 핸들러 밖의 모든 읽기는 그 프레임의 시각으로 답을 받고, 핸들러 안의 읽기는 새 프레임으로 답을
받는다. 최종 보고의 `history-pruned 19 / edges 76`(바닥 프레임의 시각 이후의 edge는 버리지 못함)과
`active-depth 1`이 그 흔적이다. 같은 흔적은 제 입력 스크립트 실행들에도 있었다(pumpit8·pumpitp2·
pumpipx2p 등에서 pruned < edges, 한 키의 PRESSED가 수백~수천 회).

### 설계

1. **핸들러의 복귀로 프레임을 끝낸다**(`EndTimerInterrupt`). 엔진이 `iret`을 직접 실행하는 곳
   (`HandleIretdInstruction`)에서, 그 `iret`의 ESP와 같은 ESP로 쌓인 프레임을 위에서부터 찾아 그것과 그
   위의 프레임을 회수한다. 주입이 쌓지 않은 프레임을 꺼내는 `iret`(체인된 옛 핸들러, 소프트웨어
   인터럽트)은 맞는 프레임이 없어 아무것도 끝내지 않는다. `REPIU_JAMMA_REPLAY_FRAME_END=0`은 이 경로를
   끈다.
2. **남은 것은 나이로 회수한다**. Win32는 게스트의 `iret`을 네이티브로 실행하므로 복귀를 볼 수 없다.
   프레임이 쌓인 뒤 64회(`kReplayFrameStaleBegins`, tick backlog의 상한) 넘게 새 주입이 시작됐으면 그
   프레임은 아직 도는 핸들러가 아니라 회수되지 못한 것이다. 시계가 아니라 주입 횟수로 세는 까닭은 replay
   시각이 backlog 때문에 정당하게 수백 ms 과거일 수 있기 때문이다.
3. **스크립트 키보드의 핸들러 밖 읽기**. 핸들러 밖의 읽기는 SDL의 키보드 상태 배열을 읽는데, 밀어 넣은
   이벤트(`SDL_PushEvent`)는 그 배열을 바꾸지 않는다. 지금까지는 회수되지 않은 프레임이 그 읽기에 답하고
   있었으므로, 프레임을 제때 끝내면 스크립트 실행의 핸들러 밖 읽기는 늘 "뗌"이 된다. 입력 스크립트가 돌
   때에 한해 타임라인의 최신 상태로 답한다(`ServeLiveReadsFromLatestState`). 실제 키보드의 경로는 그대로다.
4. **관측**. 최종 보고에 `JAMMA timeline frames ended-by-return/retired-stale/latest-state-reads`를 더하고,
   `REPIU_JAMMA_TIMELINE_TRACE=1`은 나이로 회수되는 프레임마다 그 ESP와 주입 당시의 게스트 EIP를 찍는다.

### 검증 전략

* probe(`jamma_input_timeline_probe`): 복귀로 끝남, 체인 핸들러의 `iret`은 무시, 중첩 프레임의 동반 회수,
  나이 규칙(64회까지는 답하고 그 뒤에는 회수), 최신 상태 읽기의 스위치.
* pumpit8 입력 스크립트 120초(x11, wayland): 수정 전 규칙(`REPIU_JAMMA_REPLAY_FRAME_END=0`)과 수정 후의
  키별 PRESSED 수, `history-pruned`/`edges`, 프레임·MP3·tick 수치.
* 16개 롬셋 재조사(진행 단계가 달라지지 않는지), Linux·Win32 core probe, Win32 실행.

### 한계

이 결함이 사용자가 말한 증상(무엇이 "다시 나왔는지"는 로그만으로 특정되지 않는다)의 원인인지는 사용자의
실행으로만 확인된다. 로그에서 정상이 아닌 것은 입력뿐이었다는 것까지가 확인된 범위다.

## English

### Background

The user's report: pumpit8's song 720 played on the Task 752 build (wayland, D3D12, real vsync), and "the
problem came back". In that run's stderr (`repiu_log.txt`, 43 s) the frame, MP3 and tick figures were
normal.

| Item | Value |
|---|---|
| Renderer / swap interval | `D3D12 (NVIDIA GeForce RTX 4090)` / 1 applied, pacing inactive |
| MP3 feed in play (26–48 s) | 15.5–17.0 KB a second, 38–39 toggles, queue never under 229 ms, starvation not rising, pcm-empty 0 |
| Frames over 25 ms in play | 2 (presents of 23 ms and 27 ms) |
| Ticks | 10,821 due / 10,701 delivered / 120 dropped, backlog 5 at most |

What was not normal was the input. `[repiu-input] SERVICE PRESSED` was printed **1,897 times** (about 60 a
second) in a run with only 76 real key edges. The log's order was always `SERVICE PRESSED` with
`P2-Center released`, then `SERVICE released` with `P2-Center PRESSED`: the guest's reads were answered
with two states in turn, the keys as they are (the P2 keys the user was holding) and **the keys as they
were when SERVICE (F2) was being held**.

The input timeline (`JammaInputTimeline`) pushes a replay frame with a tick's due time when the tick is
injected, and answers the guest's input port reads with the keys at that time (so that a handler running
late still sees key edges in order). A frame was retired by **one stack test** only: a read's or the next
injection's ESP being above the frame's. The frame of a tick injected at a stack level the guest does not
come back to is never retired and stays at the bottom. From then on every read outside a handler is
answered with that frame's time while reads inside a handler are answered with a fresh one. The final
report's `history-pruned 19` of `edges 76` (no edge after the bottom frame's time can be dropped) and
`active-depth 1` are its traces. My own scripted runs carried the same traces (pruned < edges and one
key's PRESSED in the hundreds or thousands for pumpit8, pumpitp2, pumpipx2p and others).

### Design

1. **A handler's return ends its frame** (`EndTimerInterrupt`). Where the engine executes `iret` itself
   (`HandleIretdInstruction`), the frame pushed at that `iret`'s ESP is searched for from the top and
   retired with whatever is above it. An `iret` popping a frame no injection pushed (a chained old
   handler, a software interrupt) matches nothing and ends nothing. `REPIU_JAMMA_REPLAY_FRAME_END=0` turns
   the path off.
2. **What remains is retired by age**. Win32 runs the guest's `iret` natively and cannot see the return. A
   frame after which more than 64 injections (`kReplayFrameStaleBegins`, the tick backlog's bound) have
   begun is not a handler still running but one that was never retired. It is counted in injections, not
   by a clock, because a replay time may rightly be hundreds of ms in the past under a backlog.
3. **A scripted keyboard's reads outside a handler**. Those reads use SDL's keyboard state array, which
   pushed events (`SDL_PushEvent`) do not change. Until now frames that were never retired answered them,
   so with frames ended on time a scripted run would read "released" outside every handler. While an input
   script runs, and only then, they are answered from the timeline's latest state
   (`ServeLiveReadsFromLatestState`). A real keyboard's path is as it was.
4. **Observation**. The final report gains `JAMMA timeline frames ended-by-return/retired-stale/
   latest-state-reads`, and `REPIU_JAMMA_TIMELINE_TRACE=1` prints each frame retired by age with its ESP
   and the guest EIP its tick interrupted.

### Verification strategy

* The probe (`jamma_input_timeline_probe`): ending by return, a chained handler's `iret` ignored, a nested
  frame going with the outer one, the age rule (answering up to 64 and retired after), the switch of the
  latest-state reads.
* pumpit8 under its input script for 120 s (x11, wayland): presses per key, `history-pruned` against
  `edges`, and the frame, MP3 and tick figures, under the old rule (`REPIU_JAMMA_REPLAY_FRAME_END=0`) and
  the new.
* The 16 ROM sets surveyed again (that their progress does not change), the core probes on Linux and
  Win32, a Win32 run.

### Limits

Whether this defect is the cause of what the user saw (the log alone does not say which symptom "came
back") can only be confirmed by the user's run. What is established is that the input was the one thing
in the log that was not normal.
