# Task 744: `xor r32,r32; mov r8,r8` jump-table guard 작업 지시

설계: [20260927-744](../design/20260927-744-jump-table-low-byte-move-guard.md)

## 한국어

1. `JumpTableGuard`에 `zeroed_register`를 두고 `PropagateLowByteJumpTableGuard`가 `movzx`,
   `xor`/`sub` 자기 자신, `mov r8,r8'` idiom을 받게 한다.
2. probe `jump_table_guard`에 정규화 두 case와 거부 한 case를 더한다.
3. Linux x64 Release attract 30초로 `0x010F659E`가 사라졌는지 재고, core probe(Linux·Win32)와
   pumpitea 플레이·pumpit2a를 확인한다.
4. 설계·작업 로그·analysis를 갱신하고 커밋한다.

## English

1. Give `JumpTableGuard` a `zeroed_register` and have `PropagateLowByteJumpTableGuard` accept the
   `movzx`, self-`xor`/`sub` and `mov r8,r8'` idioms.
2. Add two normalization cases and one rejection case to the `jump_table_guard` probe.
3. Remeasure a 30 s Linux x64 Release attract for `0x010F659E`, and check the core probes (Linux,
   Win32), a pumpitea play and pumpit2a.
4. Update the design, work log and analysis, then commit.
