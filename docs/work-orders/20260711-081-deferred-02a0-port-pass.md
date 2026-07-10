# 0x02A0 Port I/O 보류 통과 작업 지시

## 목표

`0x02A0` 계열 Port I/O 의미 분석은 TODO로 보류한 채, `piu_1st`를 trace-limit 너머의 다음 blocker까지 진행시킨다.

## 범위

* `0x02A0..0x02AF` 범위 4-byte `OUT DX,EAX`를 `deferred-ignored`로 처리한다.
* trace buffer 용량 제한은 기록 제한으로만 사용하고, 실행 중단 조건에서 제외한다.
* 큰 안전 상한을 초과하면 `deferred-limit`으로 중단한다.
* `66 ED` (`IN EAX,DX`)를 기록 가능한 중단점으로 추가한다.
* `piu_1st` 실행 결과와 테스트 기준을 새 blocker에 맞춰 갱신한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Deferred 0x02A0 Port I/O Pass Work Order

## Goal

Keep the `0x02A0` Port I/O meaning deferred in TODO while advancing `piu_1st` beyond the artificial trace-limit to the next blocker.

## Scope

* Treat 4-byte `OUT DX,EAX` in `0x02A0..0x02AF` as `deferred-ignored`.
* Use trace buffer capacity only as a recording limit, not as an execution stop condition.
* Stop with `deferred-limit` if a large safety cap is exceeded.
* Add `66 ED` (`IN EAX,DX`) as a recordable stopping point.
* Update the `piu_1st` run result and test expectations to the new blocker.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
