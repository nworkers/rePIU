# Task 744 작업 로그 — `xor edx,edx; mov dl,bl`로 정규화하는 jump table

설계: [20260927-744](../design/20260927-744-jump-table-low-byte-move-guard.md)
작업 지시: [20260927-744](../work-orders/20260927-744-jump-table-low-byte-move-guard.md)

## 요약

Task 741 census의 5위 지점 `0x010F659E`(`jmp dword ptr cs:[edx*4+table]`, 초당 1,100~1,200회 trap)가
jump table로 번역됩니다. planner는 `cmp bl,imm; ja` low-byte guard를 알았지만 정규화로
`and r32,0xFF`만 받았고, 이 Watcom `switch`는 `xor edx,edx; mov dl,bl`로 guard의 low byte를 다른
레지스터에 옮깁니다. `PropagateLowByteJumpTableGuard`가 `movzx r32,r8`, 자기 자신 `xor`/`sub`(guard에
`zeroed_register`를 실어 다음 명령으로 전달), `mov r8,r8'`(zeroed 레지스터 ← guard 레지스터)를 받습니다.
attract 30초에서 그 지점이 census에서 사라졌습니다(0회).

## 과정

1. 디스어셈블: `cmp bl,0xd; ja exit; xor edx,edx; mov dl,bl; jmp cs:[edx*4+0xE651C]`. `MatchJumpTableBranch`
   는 이미 `CS:`를 받고 long-mode 방출기(`LowerLongModeJumpTableTargetLoad`)는 `2E` 접두어를 벗기므로,
   막힌 곳은 guard 전달뿐이었습니다. guard 전달이 안 되면 `jmp`는 `ZYDIS_ATTRIB_HAS_SEGMENT`로 HLE
   boundary가 되고, 매번 trap해 runtime의 간접 transfer 처리(접두어를 벗김)로 갑니다.
2. `JumpTableGuard::zeroed_register`와 네 idiom. 첫 pass의 인라인 전파와 sweep이 같은 함수를 쓰므로
   두 명령짜리 idiom도 한 명령씩 전달됩니다. `xor`의 대상이 guard 레지스터 자신이면 받지 않습니다
   (그 경우 guard 값이 사라짐).
3. probe `jump_table_guard`: `xor edx,edx; mov dl,bl` 정규화, `movzx edx,bl` 정규화, `mov dl,cl`(guard가
   아닌 레지스터에서) 거부.

## 검증

| 검증 | 결과 |
|---|---|
| Linux x64 Release core probe | `core_probe_all=true` (새 case 3개 포함) |
| pumpitea attract 30초 | 폴트 0, `0x010F659E` 0회(742 뒤 36,659회), breakpoint 968,425(보정 spin 변동 범위 안), VEH 7.30%, 3,117 frame |
| pumpitea 플레이 60초(합성 키) | 아래 English 절과 같음 |
| pumpit2a 25초 | 폴트 0, 3,302 frame, breakpoint 108,557 |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

로그: `build/task744-bp1.err.log`, `build/task744-play2.err.log`, `build/task744-pumpit2a-reg.err.log`.

## 남은 것

* census 상위는 이제 전부 게임 자신의 `delay()`(INT 21h AH=2Ch)·`lseek`·ISR의 batch된 `in`/EOI/`iret`
  입니다. 엔진 쪽 trap 후보는 소진되었습니다.

---

# English

# Task 744 work log — jump tables normalized by `xor edx,edx; mov dl,bl`

Design: [20260927-744](../design/20260927-744-jump-table-low-byte-move-guard.md)
Work order: [20260927-744](../work-orders/20260927-744-jump-table-low-byte-move-guard.md)

## Summary

The fifth-busiest site of Task 741's census, `0x010F659E` (`jmp dword ptr cs:[edx*4+table]`,
1,100–1,200 traps a second), is translated as a jump table. The planner knew the `cmp bl,imm; ja`
low-byte guard but accepted only `and r32,0xFF` as its normalization, while this Watcom `switch` moves
the guarded low byte into another register with `xor edx,edx; mov dl,bl`.
`PropagateLowByteJumpTableGuard` now accepts `movzx r32,r8`, a self-`xor`/`sub` (carrying
`zeroed_register` in the guard to the next instruction) and `mov r8,r8'` (zeroed register ← guard
register). The site is gone from the census over a 30 s attract (zero).

## Steps

1. Disassembly: `cmp bl,0xd; ja exit; xor edx,edx; mov dl,bl; jmp cs:[edx*4+0xE651C]`.
   `MatchJumpTableBranch` already accepts `CS:` and the long-mode emitter
   (`LowerLongModeJumpTableTargetLoad`) strips the `2E` prefix, so only the guard propagation was in
   the way; without it the `jmp` becomes an HLE boundary through `ZYDIS_ATTRIB_HAS_SEGMENT` and traps
   into the runtime's indirect transfer handling (which strips the prefix) every time.
2. `JumpTableGuard::zeroed_register` and the four idioms. The first pass's inline propagation and the
   sweep share the function, so the two-instruction idiom is carried one instruction at a time. A
   `xor` of the guard register itself is not accepted (it would destroy the guarded value).
3. Probe `jump_table_guard`: the `xor edx,edx; mov dl,bl` normalization, the `movzx edx,bl`
   normalization, and a rejected `mov dl,cl` (from a register that is not the guard).

## Verification

| Check | Result |
|---|---|
| Linux x64 Release core probe | `core_probe_all=true` (three new cases included) |
| pumpitea 30 s attract | no faults, `0x010F659E` zero (36,659 after 742), 968,425 breakpoints (within the calibration spin's variance), VEH 7.30%, 3,117 frames |
| pumpitea 60 s play (synthetic keys) | reaches the song, no faults |
| pumpit2a 25 s | no faults, 3,302 frames, 108,557 breakpoints |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

Logs: `build/task744-bp1.err.log`, `build/task744-play2.err.log`, `build/task744-pumpit2a-reg.err.log`.

## What remains

* The census's top sites are now all the game's own: `delay()` (INT 21h AH=2Ch), `lseek` and the
  ISR's batched `in`, EOI and `iret`. The engine-side trap candidates are exhausted.
