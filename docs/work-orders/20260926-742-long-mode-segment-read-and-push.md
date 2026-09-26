# Task 742: long-mode `mov r,sreg`·`push sreg` 슬롯 작업 지시

설계: [20260926-742](../design/20260926-742-long-mode-segment-read-and-push.md)

## 한국어

1. planner에 `kGuardedSegmentPush`(`06`/`1E`/`0F A0`/`0F A8`, 32비트 operand)를 더한다.
2. long-mode 방출기 `EmitLongModeGuardedSegmentRead`·`EmitLongModeGuardedSegmentPush`를 넣고
   dispatch 사슬에 건다. i386 이미지의 push는 원본 복사, long-mode 이미지의 i386 read 슬롯은 방출하지
   않는다.
3. `ValidateAotCodeCacheHleCoverage`와 instruction census에 두 kind를 반영한다.
4. core probe `long_mode_emission`에 세 case를 더한다.
5. Linux x64 Release로 pumpitea attract 27초를 재측정해 두 지점이 사라졌는지 보고, core probe(Linux·
   Win32), pumpitea 플레이, pumpit2a 회귀를 확인한다.
6. 설계·작업 로그·analysis를 갱신하고 커밋한다.

## English

1. Add `kGuardedSegmentPush` (`06`/`1E`/`0F A0`/`0F A8`, 32-bit operand) to the planner.
2. Add `EmitLongModeGuardedSegmentRead` and `EmitLongModeGuardedSegmentPush` and hook them into the
   dispatch chain; copy the push verbatim on i386 images and never emit the i386 read slot into a
   long-mode image.
3. Teach `ValidateAotCodeCacheHleCoverage` and the instruction census both kinds.
4. Add three core-probe cases to `long_mode_emission`.
5. Remeasure pumpitea's 27 s attract on Linux x64 Release for the two sites, and run the core probes
   (Linux and Win32), a pumpitea play and a pumpit2a regression.
6. Update the design, work log and analysis, then commit.
