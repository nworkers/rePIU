# Traced C7 memory store HLE 설계

## 배경

`stage.cfg` 누락 경로 resize guard 이후 `piu_1st`는 `FAIL: res_load( spr.res )` 출력까지 진행한다.

현재 중단 지점은 `0x020F7340`의 `C7 01 FF FF FF FF`이다. 이 명령은 `mov dword ptr [ecx], 0xFFFFFFFF` 형태의 일반 memory store이다. 이 store를 처리하면 다음 관측 지점은 같은 out-of-arena metadata 주소를 확인하는 `F7 07 01 00 00 00` test 계열 명령이다.

## 관측

예외 시점의 `ECX`는 현재 runtime arena end를 넘어선 주소를 가리킨다. 이 흐름은 `spr.res` open 실패 이후 원본 런타임이 실패 경로의 heap/file metadata sentinel을 기록하려는 것으로 관측된다.

`SPR.RES`는 target root에는 존재하지만 현재 current directory `\DATAS\BGA` 아래에는 없다. 이번 작업에서는 root fallback을 도입하지 않고, 사용자가 요청한 대로 opcode 처리만 먼저 진행한다.

## 정책

* `C7 01 imm32`를 traced memory store HLE 대상으로 처리한다.
* destination이 runtime arena 안이면 실제 dword write를 수행하고 `EIP`를 6바이트 진행한다.
* destination이 runtime arena 밖이면 마지막 DOS open이 실패한 상태일 때만 out-of-arena metadata store로 기록하고 `EIP`를 6바이트 진행한다.
* 이 처리는 일반 C7 전체 해석기가 아니라 현재 관측된 ModR/M `0x01` 형태만 지원한다.
* loader 로그에는 처리 횟수, 마지막 store 주소, 값, 실제 적용 여부를 출력한다.
* 직전 out-of-arena store와 같은 주소를 읽는 `F7 07 imm32`는 마지막 store 값을 임시 shadow value로 사용해 `TEST` flags를 갱신한다.

# Traced C7 Memory Store HLE Design

## Background

After the `stage.cfg` missing-path resize guard, `piu_1st` advances to the `FAIL: res_load( spr.res )` output.

The current stop is `C7 01 FF FF FF FF` at `0x020F7340`. This instruction is a normal memory store: `mov dword ptr [ecx], 0xFFFFFFFF`. After that store is handled, the next observed point is the `F7 07 01 00 00 00` test instruction against the same out-of-arena metadata address.

## Observation

At the exception point, `ECX` points beyond the current runtime arena end. This appears to be the original runtime writing a heap/file metadata sentinel on the failure path after the failed `spr.res` open.

`SPR.RES` exists at the target root but not under current directory `\DATAS\BGA`. This task does not introduce root fallback; it only handles the opcode first, as requested.

## Policy

* Handle `C7 01 imm32` as a traced memory store HLE case.
* If the destination is inside the runtime arena, perform the actual dword write and advance `EIP` by 6 bytes.
* If the destination is outside the runtime arena, record it as an out-of-arena metadata store and advance `EIP` by 6 bytes only when the last DOS open failed.
* This is not a general C7 decoder; it supports only the currently observed ModR/M `0x01` form.
* Loader logs print the handled count, last store address, value, and whether the write was applied.
* `F7 07 imm32` reading the same address as the previous out-of-arena store uses the last store value as a temporary shadow value and updates `TEST` flags.
