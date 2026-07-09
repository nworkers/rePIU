# Traced 89 memory store HLE 작업 지시

## 목표

`piu_1st`의 현재 `0x020F6708` opcode `0x89` 중단 지점을 관측 기반 memory store HLE로 처리하고, 더 진행 가능한 다음 지점을 확인한다.

## 범위

* Win32 trampoline에 제한된 `89 /r` memory store 처리기를 추가한다.
* SIB 없는 32-bit ModR/M memory destination만 지원한다.
* arena 내부 destination은 실제 dword write로 처리한다.
* arena 외부 destination은 마지막 DOS open 실패 경로에서만 기록 후 skip한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대 관측 지점을 갱신한다.
* 작업 로그와 TODO를 갱신한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Traced 89 Memory Store HLE Work Order

## Goal

Handle the current `piu_1st` opcode `0x89` stop at `0x020F6708` through observation-driven memory-store HLE and identify the next reachable point.

## Scope

* Add a constrained `89 /r` memory-store handler to the Win32 trampoline.
* Support only 32-bit ModR/M memory destinations without SIB.
* Perform actual dword writes for destinations inside the arena.
* Record and skip out-of-arena destinations only on the last DOS open failure path.
* Update the expected `piu_1st` observation point in `scripts/test_all.ps1`.
* Update the work log and TODO.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
