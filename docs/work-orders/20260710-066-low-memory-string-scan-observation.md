# Low-memory string scan observation 작업 지시

## 목표

`piu_1st`가 `0x020F4DC1`의 low-memory 문자열 스캔 근처에서 timeout 관측되는 이유를 좁히기 위해, low-memory DOS memory access helper의 처리 상태를 별도로 출력한다.

## 범위

* `HandleDosMemoryAccess`의 low-memory 문자열/환경 접근 처리에 카운터와 마지막 관측 필드를 추가한다.
* 마지막 low-memory access `EIP`, opcode, `ESI`, `EDI`, 읽기/쓰기 값, destination을 `Win32MinimalExecutionAttempt`에 복사한다.
* loader 출력에 low-memory access summary를 추가한다.
* `piu_1st` 테스트 기준에 low-memory access 관측을 추가한다.

## 제외 범위

* low-memory 내용을 실제 DOS PSP/environment layout으로 완성하지 않는다.
* `spr.res` 파일 없음은 정상 분기로 보고 파일시스템 정책을 바꾸지 않는다.
* timeout 정책 자체를 바꾸지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Low-Memory String Scan Observation Work Order

## Goal

Add dedicated output for the low-memory DOS memory access helper so the `piu_1st` timeout observation near `0x020F4DC1` can be narrowed down.

## Scope

* Add counters and last-observation fields to low-memory string/environment access handling in `HandleDosMemoryAccess`.
* Copy the last low-memory access `EIP`, opcode, `ESI`, `EDI`, read/write value, and destination into `Win32MinimalExecutionAttempt`.
* Add low-memory access summary output to the loader.
* Add low-memory access observation expectations to the `piu_1st` test.

## Out Of Scope

* Do not fully implement DOS PSP/environment memory layout.
* Treat missing `spr.res` as a normal branch and do not change filesystem policy.
* Do not change the timeout policy itself.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
