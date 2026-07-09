# Traced C7 memory store HLE 작업 로그

## 변경 내용

`piu_1st`가 `spr.res` open 실패 뒤 도달하는 `0x020F7340`의 `C7 01 FF FF FF FF` memory store를 관측 기반 HLE 대상으로 추가했다.

이번 작업에서는 root fallback을 추가하지 않았다. `spr.res`는 current directory `\DATAS\BGA` 기준으로 없는 파일로 유지하며, 사용자의 지시에 따라 opcode 처리만 먼저 진행했다.

## 구현 요약

* Win32 trampoline에 `C7 01 imm32` 처리기를 추가했다.
* destination이 runtime arena 안이면 실제 dword write를 수행한다.
* destination이 runtime arena 밖이면 마지막 DOS open 실패 경로에서만 out-of-arena metadata store로 기록하고 다음 명령으로 진행한다.
* 같은 out-of-arena metadata 주소를 검사하는 `F7 07 imm32`는 직전 store 값을 shadow 값으로 사용해 `TEST` flags를 갱신한다.
* loader 로그와 `Win32MinimalExecutionAttempt`에 memory store 관측 필드를 추가했다.
* `scripts/test_all.ps1`의 `piu_1st` 기대 관측 지점을 새 중단 지점으로 갱신했다.

## 관측 결과

`C7` store와 이어지는 `F7` test를 통과한 뒤 `piu_1st`는 다음 지점까지 진행한다.

* 마지막 출력: `FAIL: res_load( spr.res )`
* 처리된 memory store 수: `1`
* 마지막 memory store 주소: `0x020F7340`
* 마지막 memory store destination: `0x02670E04`
* 마지막 memory store 값: `0xFFFFFFFF`
* 실제 write 적용 여부: `false`
* 현재 다음 중단 지점: `0x020F6708`
* 현재 opcode: `0x89`

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 일반 권한 실행은 기존 build directory의 `spdlog` stamp 파일 접근 거부로 CMake configure 단계에서 실패했다.
  * 권한 상승 실행에서 Win32 x86 build, `dos4gw_hello`, `piu_1st` 관측 테스트가 모두 통과했다.

## 다음 작업

`0x020F6708`에서 관측된 opcode `0x89`를 relocated bytes와 레지스터 상태 기준으로 좁혀, `89 /r` memory store 처리 범위를 설계한다.

# Traced C7 Memory Store HLE Work Log

## Changes

Added an observation-driven HLE case for the `C7 01 FF FF FF FF` memory store at `0x020F7340`, reached by `piu_1st` after the failed `spr.res` open.

This task did not add root fallback. `spr.res` remains a missing file relative to current directory `\DATAS\BGA`, and only opcode handling was advanced as requested.

## Implementation Summary

* Added a `C7 01 imm32` handler to the Win32 trampoline.
* Performs the actual dword write when the destination is inside the runtime arena.
* Records and advances an out-of-arena metadata store only on the last DOS open failure path.
* Handles `F7 07 imm32` tests against the same out-of-arena metadata address by using the previous store value as a shadow value and updating `TEST` flags.
* Added memory-store observation fields to loader logs and `Win32MinimalExecutionAttempt`.
* Updated the expected `piu_1st` observation point in `scripts/test_all.ps1`.

## Observation

After the `C7` store and following `F7` test are handled, `piu_1st` advances to the next point.

* Last output: `FAIL: res_load( spr.res )`
* Handled memory store count: `1`
* Last memory store address: `0x020F7340`
* Last memory store destination: `0x02670E04`
* Last memory store value: `0xFFFFFFFF`
* Write applied: `false`
* Current next stop: `0x020F6708`
* Current opcode: `0x89`

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * The normal-permission run failed during CMake configure because access to the existing build directory's `spdlog` stamp file was denied.
  * The elevated run passed the Win32 x86 build, `dos4gw_hello`, and `piu_1st` observation tests.

## Next Work

Narrow the observed opcode `0x89` at `0x020F6708` from relocated bytes and register state, then design the required `89 /r` memory-store handling scope.
