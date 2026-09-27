# Task 750: swap 대기 중 타이머 tick 전달 작업 지시

설계: [20260928-750](../design/20260928-750-timer-ticks-during-the-swap-wait.md)

## 한국어

1. 백엔드에 `WaitForPendingSwaps`를 넣고 게시된 swap이 끝나면 대기자를 깨운다.
2. `grBufferSwap` 게이트를 게시 + 대기로 바꾸고, 대기 중 밀린 tick을 호출부로 돌아오는 프레임으로
   주입한다(`ContinueGlideSwapWait`, `FindGlideGateCallSite`). direct dispatch resolver 둘이 주입 결과를
   받아들이게 한다.
3. tick 무장 블록을 `ThreadContext::timer_tick_arm` callable로 빼 게스트 대기 루프도 부르게 한다.
4. 스위치 `REPIU_GLIDE_SWAP_WAIT_TICKS`(Linux x64 기본 켜짐), 보고 줄, 진단 `REPIU_GLIDE_SWAP_WAIT_LOG`.
5. 위치 트레이스로 x11 페이싱·wayland·자유 실행을 재고, 스위치 off 대조·pumpit2a·pumpit1·core
   probe(Linux·Win32)·Win32 smoke를 확인한 뒤 README·analysis·작업 로그를 갱신하고 커밋한다.

## English

1. Add `WaitForPendingSwaps` to the backend and wake waiters when a posted swap finishes.
2. Turn the `grBufferSwap` gate into post plus wait, injecting owed ticks during the wait with a frame
   that returns to the call site (`ContinueGlideSwapWait`, `FindGlideGateCallSite`), and make both direct
   dispatch resolvers accept the injected outcome.
3. Move the tick arming block into the `ThreadContext::timer_tick_arm` callable so the guest's wait loop
   runs it too.
4. The switch `REPIU_GLIDE_SWAP_WAIT_TICKS` (on by default on Linux x64), the report line, and the
   diagnostic `REPIU_GLIDE_SWAP_WAIT_LOG`.
5. Remeasure x11 paced, wayland and free-running plays with the position trace, check the switch-off
   control, pumpit2a, pumpit1, the core probes (Linux, Win32) and a Win32 smoke run, update README, the
   analysis and the work log, and commit.
