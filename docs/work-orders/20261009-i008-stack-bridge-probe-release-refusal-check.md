# issue #8 (Task 765): stack bridge probe의 거절 검사 수정 작업 지시

> **경위(2026-10-09, issue #8):** 이 작업은 2026-10-01 Task 765로 끝났지만, 그 커밋(`059a08b`)이 있던
> 브랜치 `docs/763-close-resolved-frontier-items`가 머지되지 않은 채 사라져 수정이 main에 들어가지
> 않았습니다. 같은 실패가 2026-10-05 issue #8로 다시 보고됐고, 2026-10-09 같은 수정을 main 위에 다시
> 적용했습니다. 본문은 Task 765 당시의 기록입니다.


설계: [20261009-i008](../design/20261009-i008-stack-bridge-probe-release-refusal-check.md)

## 한국어

1. `src/tools/aot_probe/stack_bridge_probe.cpp`에 `RepiuStackBridgeProbeRefusedCall`(MSVC naked)을 두고,
   `ProbeBridgeContract`의 거절 검사를 `refused == kRefusedEntryMarker`(`0x0BADF00D`)로 바꾼다.
2. `src/tools/aot_probe/stack_bridge_probe.S`에 같은 심볼을 둔다.
3. Win32 Release·Debug core probe와 Linux i386 core probe를 돌려 확인하고, 작업 로그를 남긴 뒤 커밋한다.

## English

> **History (2026-10-09, issue #8):** this was finished on 2026-10-01 as Task 765, but the branch
> holding its commit (`059a08b`), `docs/763-close-resolved-frontier-items`, disappeared unmerged, so
> the fix never reached main. The same failure was reported again on 2026-10-05 as issue #8, and the
> same fix was reapplied on main on 2026-10-09. The body is the Task 765 record.

1. Add `RepiuStackBridgeProbeRefusedCall` (an MSVC naked function) to
   `src/tools/aot_probe/stack_bridge_probe.cpp` and change the refusal check in `ProbeBridgeContract`
   to `refused == kRefusedEntryMarker` (`0x0BADF00D`).
2. Add the same symbol to `src/tools/aot_probe/stack_bridge_probe.S`.
3. Run the Win32 Release and Debug core probes and the Linux i386 core probe, write the work log and
   commit.
