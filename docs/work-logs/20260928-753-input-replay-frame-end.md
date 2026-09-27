# Task 753 작업 로그 — 입력 replay 프레임의 끝

설계: [20260928-753](../design/20260928-753-input-replay-frame-end.md)
작업 지시: [20260928-753](../work-orders/20260928-753-input-replay-frame-end.md)

## 요약

사용자의 pumpit8 실행 로그에서 프레임·MP3·tick은 정상이었고, 정상이 아닌 것은 입력이었습니다. 게스트가
SERVICE 누름을 1,897회(초당 약 60회) 읽었는데 실제 키 edge는 76개였습니다. 원인은 입력 타임라인의 replay
프레임이 **스택 비교로만 회수**되던 것입니다: 게스트가 다시 올라오지 않는 스택 높이에서 주입된 tick의
프레임이 바닥에 남아, 핸들러 밖의 모든 읽기가 그 tick 시각의 키(SERVICE를 누르고 있던 때)로 답을 받았습니다.
프레임을 핸들러의 `iret`에서 끝내고, 복귀를 볼 수 없는 곳은 나이(이후 64회 넘는 주입)로 회수하게 했습니다.
수정 후 pumpit8 입력 스크립트 실행에서 게스트가 읽은 누름 수가 스크립트와 정확히 같아졌습니다.

이 결함이 사용자가 본 증상의 원인인지는 **확인되지 않았습니다**. 로그만으로는 어느 증상이 다시 나왔는지
알 수 없고, 제 실행은 소리와 화면을 듣고 보지 못합니다. 사용자 확인이 필요합니다.

## 과정

1. 사용자 로그(`repiu_log.txt`, 43초, wayland + D3D12 + 진짜 vsync)의 수치를 초 단위로 냈습니다. 플레이
   구간(26~48초)의 MP3 공급은 초당 15.5~17.0 KB, 큐 최소 229 ms, pcm-empty 0, 25 ms 초과 프레임 2개였고
   Task 752의 제 정상 실행과 같은 범위였습니다.
2. `[repiu-input]` 줄은 4,358개였습니다. SERVICE PRESSED 1,897회, P2 다섯 키의 PRESSED 합 281회(실제 edge는
   SERVICE 포함 76개). 줄의 순서는 `SERVICE PRESSED`/`P2-Center released`와 `SERVICE released`/`P2-Center
   PRESSED`의 교대였습니다. 첫 SERVICE 누름 네 번(7.3~8.5초)은 사람의 간격이었고, 1.5초짜리 로드가 끝난
   8.9초부터 매 프레임 교대가 시작됐습니다. 그 직후 180 ms 안에 코인 음 keyon이 7회 났습니다: 게스트가 그
   읽기를 실제 누름으로 받아들였다는 흔적입니다.
3. 최종 보고의 `JAMMA timeline`은 edges 76 / history-pruned 19 / active-depth 1이었습니다. 바닥에 남은
   프레임의 시각 이후 edge를 버리지 못한 것입니다. Task 751·752의 제 입력 스크립트 로그에도 같은 흔적이
   있었습니다(pruned < edges, 한 키의 PRESSED 수백~수천 회).
4. 나이 규칙과 추적을 넣고 복귀 경로를 끈 채(`REPIU_JAMMA_REPLAY_FRAME_END=0 REPIU_JAMMA_TIMELINE_TRACE=1`)
   pumpit8을 돌려 회수되지 못하는 프레임을 확인했습니다: 120초에 51개, 가장 많은 것은 게스트 EIP
   `0x0101198C`에서 주입된 ESP `0x021CEB3C`의 프레임(15회)이었습니다.
5. `EndTimerInterrupt`를 `HandleIretdInstruction`에 연결했습니다. 프레임을 제때 끝내면 스크립트 실행의 핸들러
   밖 읽기가 SDL 키보드 상태(밀어 넣은 이벤트가 바꾸지 않음)로 가므로, 스크립트가 돌 때에 한해 타임라인의
   최신 상태로 답하게 했습니다.

## 검증

| pumpit8, 입력 스크립트(SERVICE 5, P2-Center 34, 나머지 P2 각 25), 120초 | 읽힌 누름 SERVICE / Center / UL / UR / DL / DR | pruned / edges | 복귀로 끝남 / 나이로 회수 |
|---|---|---|---|
| 수정 전(같은 스크립트를 쓴 Task 752의 로그 6개) | 합계 1,389~2,406(스크립트는 139) | 177~272 / 276~278 | — |
| 스택 비교 + 나이(복귀 끔), x11 | 7 / 34 / 29 / 48 / 48 / 24 | 274 / 274 | 0 / 51 |
| 수정 후, x11 | 5 / 34 / 25 / 25 / 25 / 25 | 278 / 278 | 28,342 / 0 |
| 수정 후, wayland | 5 / 34 / 25 / 25 / 25 / 25 | 278 / 278 | 28,471 / 0 |

| 그 밖의 검증 | 결과 |
|---|---|
| 수정 후 pumpit8 플레이 구간 | 59.7~60.3 fps(x11), 59.0~60.9 fps(wayland); MP3 pcm-empty 0, multi 0; tick 폐기 110 / 28,452, 98 / 28,569 |
| probe `--jamma-input-timeline`(Win32 x86 Debug) | `jamma_input_timeline_probe=true`, `frame_return=true`, `frame_nested=true`, `frame_stale=true`, `stale_retired=1`, `latest_state=true` |
| core probe | Linux x64 Release `core_probe_all=true`, Win32 x86 Debug `core_probe_all=true` |
| Win32 x86 Debug 빌드 | 성공. Win32 게임 실행은 하지 않았습니다 |
| Linux x64 16개 롬셋 재조사(60초씩) | 16개 모두 읽힌 누름 수가 스크립트와 같음(29, pumpit8 28, pumpipx3 24), pruned = edges, 나이 회수 0. pumpit2a 1회가 예산 종료 시점(60초)에 segfault(종료 139) — 재실행 5회(수정 켬 3, 끔 2)에서는 없음. pumpito 1회가 40~52초에 22~33 fps — 재실행 3회에서는 없음 |
| pumpito 60초, 복귀 끔 대 켬 | P1-Center 58 → 6, SERVICE 29 → 5 |

timeline probe는 `repiu_aot_probe`에만 있어(입력 타임라인 소스가 SDL에 의존) Linux에서는 돌리지 못했습니다.

로그: `build/task753-*.err.log`, `build/survey/linux-x64-753/`.

## 남은 것

* **사용자가 본 증상과의 인과는 미확인**입니다. 수정본에서 같은 증상이 남으면 그 실행의 stderr가 필요하고,
  무엇이(소리, 노트 위치, 판정) 어떻게 이상했는지가 있으면 범위를 좁힐 수 있습니다.
* 플레이 중 present가 23~43 ms인 프레임이 있습니다(게스트 몫은 1.5~3.3 ms): 45초 이후 x11 6개, wayland 13개.
  사용자 로그에도 2개 있었습니다. 호스트의 표시 쪽이며 이 작업이 건드린 경로가 아닙니다. 조사하지
  않았습니다.
* Win32는 복귀를 볼 수 없어 나이 규칙에 기댑니다. 그 사이(이후 64회 주입, 240 Hz에서 약 0.27초)에는 옛
  시각의 읽기가 남을 수 있습니다: 복귀를 끈 Linux 실행에서 SERVICE 7회(스크립트 5회)로 나타난 정도입니다.
* pumpit2a의 예산 종료 시점 segfault는 기존의 teardown 결함으로 보이지만 이 작업과 무관하다고 확정하지는
  못했습니다(수정 켠 실행 4회 중 1회).
* 전 롬셋 재조사는 이 작업에서 한 번 돌렸습니다. 사용자 지시에 따라 앞으로 일반적인 수정은 probe 사례로만
  확인하고, 오래 걸리는 회귀 실행은 필요할 때만 범위를 좁혀서 합니다.

---

# English

# Task 753 work log — the end of an input replay frame

Design: [20260928-753](../design/20260928-753-input-replay-frame-end.md)
Work order: [20260928-753](../work-orders/20260928-753-input-replay-frame-end.md)

## Summary

In the log of the user's pumpit8 run the frames, the MP3 feed and the ticks were normal; the input was
not. The guest read a SERVICE press 1,897 times (about 60 a second) where there were 76 real key edges.
The cause is that the input timeline's replay frames were retired **by the stack test alone**: the frame
of a tick injected at a stack level the guest does not come back to stayed at the bottom, and every read
outside a handler was answered with the keys at that tick's time (when SERVICE was being held). A frame
now ends at its handler's `iret`, and where the return cannot be seen it is retired by age (more than 64
later injections). With the fix, the presses the guest read in pumpit8's scripted run are exactly the
script's.

Whether this defect caused what the user saw is **not confirmed**. The log does not say which symptom
came back, and my runs neither hear nor watch. The user's confirmation is needed.

## Steps

1. The user's log (`repiu_log.txt`, 43 s, wayland + D3D12 + real vsync) by the second. In play (26–48 s)
   the MP3 feed was 15.5–17.0 KB a second, the queue never under 229 ms, pcm-empty 0, two frames over
   25 ms: the same range as my healthy runs of Task 752.
2. There were 4,358 `[repiu-input]` lines. SERVICE PRESSED 1,897 times, the five P2 keys' PRESSED 281 in
   all (76 real edges, SERVICE included). The order was `SERVICE PRESSED`/`P2-Center released` then
   `SERVICE released`/`P2-Center PRESSED`, over and over. The first four SERVICE presses (7.3–8.5 s) came
   at a person's pace, and the alternation on every frame began at 8.9 s, when a 1.5 s load ended. Seven
   coin keyons followed within 180 ms: the guest took those reads for presses.
3. The final report's `JAMMA timeline` had edges 76, history-pruned 19, active-depth 1: no edge after the
   stranded frame's time could be dropped. The logs of my scripted runs for Tasks 751 and 752 carried the
   same traces (pruned < edges, one key's PRESSED in the hundreds or thousands).
4. With the age rule and the trace in and the return path off (`REPIU_JAMMA_REPLAY_FRAME_END=0
   REPIU_JAMMA_TIMELINE_TRACE=1`) a pumpit8 run showed the frames the stack test cannot reach: 51 in
   120 s, most often the frame at ESP `0x021CEB3C` injected at guest EIP `0x0101198C` (15 times).
5. `EndTimerInterrupt` is called from `HandleIretdInstruction`. With frames ended on time a scripted run's
   reads outside a handler go to SDL's keyboard state, which pushed events do not change, so while a
   script runs, and only then, they are answered from the timeline's latest state.

## Verification

| pumpit8, input script (SERVICE 5, P2-Center 34, the other P2 keys 25 each), 120 s | presses read SERVICE / Center / UL / UR / DL / DR | pruned / edges | ended by return / retired by age |
|---|---|---|---|
| Before (six logs of Task 752 under the same script) | 1,389–2,406 in all (the script holds 139) | 177–272 / 276–278 | — |
| Stack test + age (return off), x11 | 7 / 34 / 29 / 48 / 48 / 24 | 274 / 274 | 0 / 51 |
| After, x11 | 5 / 34 / 25 / 25 / 25 / 25 | 278 / 278 | 28,342 / 0 |
| After, wayland | 5 / 34 / 25 / 25 / 25 / 25 | 278 / 278 | 28,471 / 0 |

| Other checks | Result |
|---|---|
| pumpit8 in play, after | 59.7–60.3 fps (x11), 59.0–60.9 fps (wayland); MP3 pcm-empty 0, multi 0; ticks dropped 110 of 28,452 and 98 of 28,569 |
| Probe `--jamma-input-timeline` (Win32 x86 Debug) | `jamma_input_timeline_probe=true`, `frame_return=true`, `frame_nested=true`, `frame_stale=true`, `stale_retired=1`, `latest_state=true` |
| Core probe | Linux x64 Release `core_probe_all=true`, Win32 x86 Debug `core_probe_all=true` |
| Win32 x86 Debug build | succeeded. No Win32 game run was made |
| The 16 ROM sets on Linux x64 (60 s each) | in all 16 the presses read equal the script's (29; pumpit8 28, pumpipx3 24), pruned = edges, none retired by age. One pumpit2a run segfaulted at the end of its budget (60 s, exit 139), not seen in 5 reruns (3 with the fix on, 2 off). One pumpito run fell to 22–33 fps at 40–52 s, not seen in 3 reruns |
| pumpito 60 s, return off against on | P1-Center 58 → 6, SERVICE 29 → 5 |

The timeline probe lives only in `repiu_aot_probe` (the timeline's source depends on SDL), so it was not
run on Linux.

Logs: `build/task753-*.err.log`, `build/survey/linux-x64-753/`.

## What remains

* **The link to what the user saw is not confirmed.** If the same symptom remains with the fix, that run's
  stderr is needed, and a description of what was wrong (sound, arrow position, judgement) would narrow
  it.
* Some frames in play present in 23–43 ms (the guest's share is 1.5–3.3 ms): 6 on x11 and 13 on wayland
  after 45 s, and 2 in the user's log. They are on the host's display side and not on a path this task
  touched. Not investigated.
* Win32 cannot see the return and leans on the age rule. Until it applies (64 later injections, about
  0.27 s at 240 Hz) reads at an old time can remain: on Linux with the return off that showed as SERVICE
  read 7 times against the script's 5.
* pumpit2a's segfault at the end of its budget looks like the existing teardown defect, but it is not
  established that it has nothing to do with this task (1 of 4 runs with the fix on).
* The survey of every ROM set was run once in this task. By the user's instruction, ordinary changes are
  from now on checked by probe cases alone, and long regression runs are made only when needed and
  narrowed.
