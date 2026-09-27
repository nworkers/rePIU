# Task 753: 입력 replay 프레임의 끝 작업 지시

설계: [20260928-753](../design/20260928-753-input-replay-frame-end.md)

## 한국어

1. `JammaInputTimeline`에 `EndTimerInterrupt`(복귀로 끝냄), 나이 규칙(`kReplayFrameStaleBegins`),
   `ServeLiveReadsFromLatestState`/`TryLatestPressedMask`, 세 카운터와 `REPIU_JAMMA_TIMELINE_TRACE`를 더한다.
2. `HandleIretdInstruction`에서 `EndTimerInterrupt`를 부른다(`REPIU_JAMMA_REPLAY_FRAME_END=0`으로 끔).
   주입 시 게스트 EIP를 프레임에 남긴다.
3. 입력 스크립트가 돌 때 핸들러 밖의 읽기를 타임라인의 최신 상태로 답한다.
4. probe에 복귀·체인 `iret`·중첩·나이·최신 상태 사례를 더한다.
5. pumpit8 입력 스크립트 실행을 수정 전 규칙과 수정 후로 비교하고, 16개 롬셋 재조사와 core
   probe(Linux·Win32), Win32 실행을 확인한 뒤 README·analysis·작업 로그를 갱신하고 커밋한다.

## English

1. Give `JammaInputTimeline` `EndTimerInterrupt` (ending by return), the age rule
   (`kReplayFrameStaleBegins`), `ServeLiveReadsFromLatestState`/`TryLatestPressedMask`, three counters and
   `REPIU_JAMMA_TIMELINE_TRACE`.
2. Call `EndTimerInterrupt` from `HandleIretdInstruction` (off with `REPIU_JAMMA_REPLAY_FRAME_END=0`), and
   keep the interrupted guest EIP in the frame.
3. While an input script runs, answer reads outside a handler from the timeline's latest state.
4. Add the probe's cases: return, a chained `iret`, nesting, age, latest state.
5. Compare pumpit8's scripted run under the old rule and the new, check the 16 ROM sets, the core probes
   (Linux, Win32) and a Win32 run, update README, the analysis and the work log, and commit.
