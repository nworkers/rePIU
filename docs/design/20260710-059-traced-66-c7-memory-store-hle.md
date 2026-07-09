# Traced 66 C7 memory store HLE 설계

## 배경

`D9` FPU memory HLE 처리 뒤 `piu_1st`는 `0x0201DF01`의 `66 C7 00 00 00`에서 중단된다.

`66 C7 /0`는 operand-size override가 붙은 `mov r/m16, imm16`이다. 현재 흐름에서는 `EAX=0x025E700C`이고, destination이 runtime arena 밖에 있으므로 앞선 metadata/value 초기화 흐름의 16-bit store로 관측된다.

## 정책

* `66 C7 /0 r/m16, imm16` 중 SIB 없는 32-bit ModR/M memory destination만 처리한다.
* arena 내부 destination은 실제 word write로 처리한다.
* arena 외부 destination은 마지막 DOS open 실패 경로에서만 기록 후 skip한다.
* 기존 memory store 로그에는 16-bit 값을 zero-extend해서 기록한다.
* 16-bit addressing, SIB addressing, register destination은 이번 범위에서 처리하지 않는다.

# Traced 66 C7 Memory Store HLE Design

## Background

After `D9` FPU memory HLE handling, `piu_1st` stops at `66 C7 00 00 00` at `0x0201DF01`.

`66 C7 /0` is `mov r/m16, imm16` with an operand-size override. In the current flow, `EAX=0x025E700C`, and the destination is outside the runtime arena, so this is observed as a 16-bit store in the same metadata/value initialization flow.

## Policy

* Handle only `66 C7 /0 r/m16, imm16` memory destinations using 32-bit ModR/M without SIB.
* Perform actual word writes for destinations inside the arena.
* Record and skip out-of-arena destinations only on the last DOS open failure path.
* Record the 16-bit value zero-extended in the existing memory-store log.
* 16-bit addressing, SIB addressing, and register destinations are out of scope.
