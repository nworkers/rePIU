# Traced 66 C7 memory store HLE 작업 지시

## 목표

`0x0201DF01`의 `66 C7 /0 r/m16, imm16` 중단 지점을 관측 기반 HLE로 처리하고 다음 진행 지점을 확인한다.

## 범위

* Win32 trampoline의 traced memory store 처리기에 `66 C7 /0` word store를 추가한다.
* SIB 없는 32-bit ModR/M memory destination만 지원한다.
* arena 내부 destination은 실제 word write로 처리한다.
* arena 외부 destination은 마지막 DOS open 실패 경로에서만 기록 후 skip한다.
* 테스트 기대 관측 지점, 작업 로그, TODO를 갱신한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Traced 66 C7 Memory Store HLE Work Order

## Goal

Handle the `66 C7 /0 r/m16, imm16` stop at `0x0201DF01` through observation-driven HLE and identify the next reachable point.

## Scope

* Add `66 C7 /0` word-store handling to the Win32 trampoline traced memory-store handler.
* Support only 32-bit ModR/M memory destinations without SIB.
* Perform actual word writes for destinations inside the arena.
* Record and skip out-of-arena destinations only on the last DOS open failure path.
* Update the expected test observation point, work log, and TODO.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
