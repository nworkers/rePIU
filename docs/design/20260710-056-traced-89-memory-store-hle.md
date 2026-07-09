# Traced 89 memory store HLE 설계

## 배경

`C7 01 FF FF FF FF` store와 이어지는 `F7 07 imm32` test 처리 뒤 `piu_1st`는 `0x020F6708`에서 opcode `0x89`로 중단된다.

관측된 relocated bytes는 다음 흐름을 보여준다.

```text
18 8B 4E 08 83 FA 10 72 1E 89 F7 01 C7 89 7B 0C [89] 17
89 06 8B 5E 04 89 5F 04 89 4F 08 89 7B 08 89 ...
```

`0x89`는 `mov r/m32, r32`이며, 현재 중단 지점의 `89 17`은 `mov dword ptr [edi], edx`로 해석된다. 예외 시점의 `EDI=0x0266FEF4`는 runtime arena 밖이고, 이전 `spr.res` open 실패 경로의 metadata list 조작으로 관측된다.

## 정책

* root fallback은 추가하지 않는다.
* `89 /r` 중 32-bit addressing의 SIB 없는 memory destination만 처리한다.
* destination이 runtime arena 안이면 실제 dword write를 수행한다.
* destination이 runtime arena 밖이면 마지막 DOS open 실패 경로에서만 metadata store로 기록하고 skip한다.
* register destination, SIB addressing, displacement-only addressing은 이번 범위에서 처리하지 않는다.
* 처리 결과는 기존 memory store 로그 필드를 재사용해 누적 수와 마지막 store 위치를 관측한다.

## 기대 결과

`0x020F6708`의 `89 17`과 뒤따르는 같은 계열 store들을 통과시키고, 다음 미처리 opcode 또는 접근 위반 지점을 새 관측 지점으로 삼는다.

# Traced 89 Memory Store HLE Design

## Background

After handling the `C7 01 FF FF FF FF` store and the following `F7 07 imm32` test, `piu_1st` stops at opcode `0x89` at `0x020F6708`.

The observed relocated bytes show this flow.

```text
18 8B 4E 08 83 FA 10 72 1E 89 F7 01 C7 89 7B 0C [89] 17
89 06 8B 5E 04 89 5F 04 89 4F 08 89 7B 08 89 ...
```

Opcode `0x89` is `mov r/m32, r32`, and the current stop `89 17` decodes as `mov dword ptr [edi], edx`. At the exception point, `EDI=0x0266FEF4` is outside the runtime arena and appears to be metadata list manipulation on the previous failed `spr.res` open path.

## Policy

* Do not add root fallback.
* Handle only `89 /r` memory destinations using 32-bit addressing without SIB.
* Perform the actual dword write when the destination is inside the runtime arena.
* When the destination is outside the runtime arena, record and skip it only on the last DOS open failure path.
* Register destinations, SIB addressing, and displacement-only addressing are out of scope for this task.
* Reuse the existing memory-store log fields to observe the cumulative count and last store location.

## Expected Result

Advance past `89 17` at `0x020F6708` and following stores of the same family, then use the next unhandled opcode or access violation as the new observation point.
