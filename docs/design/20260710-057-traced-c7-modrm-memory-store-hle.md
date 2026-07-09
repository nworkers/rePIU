# Traced C7 ModR/M memory store HLE 설계

## 배경

제한된 `89 /r` memory store 처리 뒤 `piu_1st`는 `0x0201DF1A`의 opcode `0xC7`에서 다시 중단된다.

관측된 bytes는 다음과 같다.

```text
01 00 C7 40 18 00 00 00 00 C7 40 14 00 00 80 3F
[C7] 40 28 00 00 80 3F D9 40 28 8B 50 18 89 50 10 D9
```

현재 명령 `C7 40 28 00 00 80 3F`는 `mov dword ptr [eax+0x28], 0x3F800000` 형태이다. 예외 시점의 `EAX=0x025E6FE0`이고 destination `0x025E7008`은 runtime arena end `0x025E7000` 바로 뒤에 있다.

## 정책

* root fallback은 추가하지 않는다.
* `C7 /0 r/m32, imm32` 중 SIB 없는 32-bit ModR/M memory destination만 처리한다.
* 기존 특수 처리였던 `C7 01 imm32`도 같은 ModR/M 처리 경로로 통합한다.
* destination이 runtime arena 안이면 실제 dword write를 수행한다.
* destination이 runtime arena 밖이면 마지막 DOS open 실패 경로에서만 metadata/value store로 기록하고 skip한다.
* register destination, SIB addressing, displacement-only addressing은 이번 범위에서 처리하지 않는다.

## 기대 결과

`0x0201DF1A`의 `C7 /0` store를 통과시키고, 이후 다음 미처리 opcode 또는 접근 위반 지점을 새 관측 지점으로 삼는다.

# Traced C7 ModR/M Memory Store HLE Design

## Background

After the constrained `89 /r` memory-store handling, `piu_1st` stops again at opcode `0xC7` at `0x0201DF1A`.

The observed bytes are:

```text
01 00 C7 40 18 00 00 00 00 C7 40 14 00 00 80 3F
[C7] 40 28 00 00 80 3F D9 40 28 8B 50 18 89 50 10 D9
```

The current instruction `C7 40 28 00 00 80 3F` is `mov dword ptr [eax+0x28], 0x3F800000`. At the exception point, `EAX=0x025E6FE0`, so destination `0x025E7008` is just beyond runtime arena end `0x025E7000`.

## Policy

* Do not add root fallback.
* Handle only `C7 /0 r/m32, imm32` memory destinations using 32-bit ModR/M without SIB.
* Fold the previous special-case `C7 01 imm32` handling into the same ModR/M path.
* Perform the actual dword write when the destination is inside the runtime arena.
* When the destination is outside the runtime arena, record and skip it only on the last DOS open failure path.
* Register destinations, SIB addressing, and displacement-only addressing are out of scope for this task.

## Expected Result

Advance past the `C7 /0` store at `0x0201DF1A`, then use the next unhandled opcode or access violation as the new observation point.
