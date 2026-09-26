# Task 744: `xor edx,edx; mov dl,bl`로 정규화하는 jump table 인식

## 한국어

### 배경

Task 741의 breakpoint 지점 census에서 `0x010F659E`가 attract 27초에 27,594~36,659회(초당 1,100~
1,200회, 상위 5위) trap을 냈고, exit site는 `aot-reentry-boundary`+`aot-indirect-transfer`였다.
코드는 Watcom의 `switch` 분기다.

```
010F6591  80 FB 0D              cmp bl,0xd
010F6594  0F 87 92 03 00 00     ja  exit
010F659A  31 D2                 xor edx,edx
010F659C  88 DA                 mov dl,bl
010F659E  2E FF 24 95 1C 65 0E 00  jmp dword ptr cs:[edx*4+table]
```

planner의 jump-table 인식은 `cmp r8,imm; ja` 형식의 low-byte guard를 알고, 그 뒤 정규화로
`and r32,0xFF`(guard 레지스터 자신)만 받는다. 이 코드는 guard 레지스터(`bl`→`ebx`)의 low byte를
**다른 레지스터**(`edx`)에 `xor`로 0을 만든 뒤 `mov`로 옮긴다. 정규화가 안 맞으니 guard가 `jmp`까지
전달되지 않고, `jmp`는 `CS:` 접두어 때문에 HLE boundary가 되어 매번 trap한다(runtime의 간접 transfer
처리는 접두어를 벗기고 평범한 간접 점프로 다룬다).

### 설계

`JumpTableGuard`에 `zeroed_register`를 두고 `PropagateLowByteJumpTableGuard`가 네 idiom을 받는다.

| 명령 | 조건 | 결과 |
|---|---|---|
| `and r32,0xFF` | r32 == guard 레지스터 | 정규화 완료 (기존) |
| `movzx r32,r8` | parent(r8) == guard 레지스터 | 정규화 완료, index = r32 |
| `xor r32,r32` / `sub r32,r32` | 같은 레지스터 | guard 유지, `zeroed_register = r32` |
| `mov r8,r8'` | parent(r8) == zeroed_register, parent(r8') == guard 레지스터 | 정규화 완료, index = zeroed_register |

첫 pass의 인라인 전파와 sweep의 `TryPropagateLowByteJumpTableGuard`가 같은 함수를 쓰므로 둘 다
따라온다. `jmp`의 매처는 이미 `CS:`를 받고, long-mode 방출기는 `2E` 접두어를 벗긴다.

probe `jump_table_guard`에 세 case를 더한다: `xor edx,edx; mov dl,bl` 정규화, `movzx edx,bl`
정규화, 그리고 `mov dl,cl`(guard가 아닌 레지스터에서 옮김) 거부.

### 검증 전략

attract 27~30초에서 `0x010F659E`가 census에서 사라지고 breakpoint가 약 3만 줄어드는지, core probe
(Linux·Win32)와 pumpitea 플레이·pumpit2a가 그대로인지 본다.

## English

### Background

In Task 741's breakpoint site census, `0x010F659E` trapped 27,594–36,659 times per 27 s attract
(1,100–1,200 a second, fifth busiest), exiting as `aot-reentry-boundary`+`aot-indirect-transfer`. It
is a Watcom `switch` dispatch: `cmp bl,0xd; ja exit; xor edx,edx; mov dl,bl; jmp dword ptr
cs:[edx*4+table]`. The planner's jump-table recognition knows the `cmp r8,imm; ja` low-byte guard
but accepts only `and r32,0xFF` on the guard register itself as the normalization. This code zeroes
**another register** (`edx`) and moves the guarded low byte into it, so the guard never reaches the
`jmp`, which the `CS:` prefix then makes an HLE boundary that traps every time (the runtime's indirect
transfer handling strips the prefix and treats it as a plain indirect jump).

### Design

`JumpTableGuard` gains `zeroed_register`, and `PropagateLowByteJumpTableGuard` accepts four idioms:
`and r32,0xFF` on the guard register (existing); `movzx r32,r8` with `parent(r8)` the guard register
(normalized, index = r32); `xor r32,r32` or `sub r32,r32` (guard carried with `zeroed_register =
r32`); and `mov r8,r8'` with `parent(r8)` the zeroed register and `parent(r8')` the guard register
(normalized, index = the zeroed register). The first pass's inline propagation and the sweep's
`TryPropagateLowByteJumpTableGuard` share the function. The `jmp` matcher already accepts `CS:` and
the long-mode emitter strips the `2E` prefix.

The `jump_table_guard` probe gains three cases: the `xor edx,edx; mov dl,bl` normalization, the
`movzx edx,bl` normalization, and a rejected `mov dl,cl` (moved from a register that is not the
guard).

### Verification strategy

`0x010F659E` gone from the census over a 27–30 s attract with about 30,000 fewer breakpoints; core
probes (Linux, Win32), pumpitea play and pumpit2a unchanged.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

설계대로 넣었습니다. attract 30초에서 `0x010F659E`가 census에서 사라졌고(0회), core probe(Linux·
Win32)와 pumpitea 플레이·pumpit2a는 그대로입니다. 설계와 달라진 점은 없습니다. 이로써 Task 741
census의 엔진 쪽 trap 후보는 소진되었고, 남은 상위 지점은 모두 게임 자신의 `delay()`·`lseek`·ISR
명령입니다.

### English

Implemented as designed. Over a 30 s attract `0x010F659E` is gone from the census (zero), and the core
probes (Linux, Win32), pumpitea play and pumpit2a are unchanged. No departures from the design. This
exhausts the engine-side trap candidates of Task 741's census; the remaining top sites are all the
game's own `delay()`, `lseek` and ISR instructions.
