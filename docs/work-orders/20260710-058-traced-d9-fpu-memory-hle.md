# Traced D9 FPU memory HLE 작업 지시

## 목표

`0x0201DF21`의 `D9 40 28` FPU memory load 중단 지점을 shadow 값 기반 HLE로 처리하고, 더 진행 가능한 다음 지점을 확인한다.

## 범위

* Win32 trampoline에 제한된 `D9` FPU memory 처리기를 추가한다.
* `D9 /0` `FLD m32fp`는 readable source 또는 직전 skipped store shadow 값을 마지막 FPU 값으로 보관한다.
* `D9 /2`, `D9 /3` `FST/FSTP m32fp`는 마지막 FPU 값을 실제 write 또는 out-of-arena 기록으로 처리한다.
* SIB 없는 32-bit ModR/M memory operand만 지원한다.
* 테스트 기대 관측 지점, 작업 로그, TODO를 갱신한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Traced D9 FPU Memory HLE Work Order

## Goal

Handle the `D9 40 28` FPU memory-load stop at `0x0201DF21` through shadow-value HLE and identify the next reachable point.

## Scope

* Add a constrained `D9` FPU memory handler to the Win32 trampoline.
* For `D9 /0` `FLD m32fp`, keep the readable source value or the previous skipped-store shadow value as the last FPU value.
* For `D9 /2` and `D9 /3` `FST/FSTP m32fp`, write the last FPU value or record it as an out-of-arena store.
* Support only 32-bit ModR/M memory operands without SIB.
* Update the expected test observation point, work log, and TODO.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
