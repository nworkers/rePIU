# Traced D9 FPU memory HLE 설계

## 배경

`C7 /0` ModR/M store 처리 뒤 `piu_1st`는 `0x0201DF21`의 `D9 40 28`에서 중단된다.

`D9 40 28`은 `fld dword ptr [eax+0x28]`로 해석된다. 직전 작업에서 `C7 40 28 00 00 80 3F`를 처리했으므로, source 주소 `EAX+0x28`은 바로 직전에 `0x3F800000`이 기록된 out-of-arena metadata/value 주소다.

## 정책

* 실제 x87 FPU 전체 상태를 구현하지 않는다.
* `D9 /0` `FLD m32fp`는 source가 readable이면 값을 읽고, 직전 skipped memory store와 같은 주소이면 그 shadow 값을 마지막 traced FPU 값으로 보관한다.
* `D9 /2` 또는 `D9 /3` `FST/FSTP m32fp`는 마지막 traced FPU 값을 destination에 저장하거나, out-of-arena 실패 경로라면 memory store로 기록하고 skip한다.
* SIB 없는 32-bit ModR/M memory operand만 지원한다.
* 이 처리는 `spr.res` open 실패 뒤 진행되는 관측 경로에 한정한다.

## 기대 결과

`D9 40 28`과 이어지는 같은 계열 FPU memory store를 통과하고, 다음 미처리 지점을 새 관측 지점으로 삼는다.

# Traced D9 FPU Memory HLE Design

## Background

After `C7 /0` ModR/M store handling, `piu_1st` stops at `D9 40 28` at `0x0201DF21`.

`D9 40 28` decodes as `fld dword ptr [eax+0x28]`. The previous task handled `C7 40 28 00 00 80 3F`, so source address `EAX+0x28` is the same out-of-arena metadata/value address where `0x3F800000` was just recorded.

## Policy

* Do not implement full x87 FPU state.
* For `D9 /0` `FLD m32fp`, read the value when the source is readable, or use the previous skipped memory-store shadow value when it targets the same address.
* For `D9 /2` or `D9 /3` `FST/FSTP m32fp`, store the last traced FPU value to the destination, or record and skip it as a memory store on the out-of-arena failure path.
* Support only 32-bit ModR/M memory operands without SIB.
* This handling is limited to the observed path after the failed `spr.res` open.

## Expected Result

Advance past `D9 40 28` and the following FPU memory store of the same family, then use the next unhandled point as the new observation point.
