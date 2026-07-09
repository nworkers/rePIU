# Traced C7 ModR/M memory store HLE 작업 지시

## 목표

`piu_1st`의 새 중단 지점인 `0x0201DF1A`의 `C7 /0 r/m32, imm32` store를 관측 기반 HLE로 처리하고, 더 진행 가능한 다음 지점을 확인한다.

## 범위

* 기존 `C7 01 imm32` 특수 처리를 `C7 /0` ModR/M memory store 처리로 확장한다.
* SIB 없는 32-bit ModR/M memory destination만 지원한다.
* arena 내부 destination은 실제 dword write로 처리한다.
* arena 외부 destination은 마지막 DOS open 실패 경로에서만 기록 후 skip한다.
* 테스트 기대 관측 지점을 새 결과에 맞게 갱신한다.
* 작업 로그와 TODO를 갱신한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Traced C7 ModR/M Memory Store HLE Work Order

## Goal

Handle the new `piu_1st` stop at `C7 /0 r/m32, imm32` at `0x0201DF1A` through observation-driven HLE and identify the next reachable point.

## Scope

* Expand the previous `C7 01 imm32` special case into `C7 /0` ModR/M memory-store handling.
* Support only 32-bit ModR/M memory destinations without SIB.
* Perform actual dword writes for destinations inside the arena.
* Record and skip out-of-arena destinations only on the last DOS open failure path.
* Update the expected test observation point.
* Update the work log and TODO.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
