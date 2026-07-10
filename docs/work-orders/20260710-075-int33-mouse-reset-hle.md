# INT 33h AX=0000h mouse reset HLE 작업 지시

## 목표

`piu_1st`가 `INT 31h AX=0400h` 이후 도달한 `INT 33h AX=0000h/0002h`를 최소 mouse HLE로 처리한다.

## 범위

* `src/platform/win32/execution_trampoline.cpp`
  * `INT 33h` 일반 HLE handler를 추가한다.
  * trace pre-handler에 `INT 33h` 감지 함수를 추가한다.
  * `AX=0000h`에 대해 `AX=0000h`, `BX=0000h`로 응답한다.
  * `AX=0002h` hide cursor는 no-op으로 처리한다.
  * 마지막 interrupt vector/AX 진단 정보를 기록한다.
* `scripts/test_all.ps1`
  * `piu_1st`의 현재 기대 관측 지점을 새 결과에 맞춰 갱신한다.

## 제외

* 실제 mouse cursor/position/button 상태 HLE
* Win32 입력 장치 연결
* 원본 실행 파일 코드 수정

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# INT 33h AX=0000h Mouse Reset HLE Work Order

## Goal

Handle `INT 33h AX=0000h/0002h`, reached by `piu_1st` after `INT 31h AX=0400h`, with minimal mouse HLE.

## Scope

* `src/platform/win32/execution_trampoline.cpp`
  * Add a general HLE handler for `INT 33h`.
  * Add an `INT 33h` detector to the trace pre-handler.
  * Respond to `AX=0000h` with `AX=0000h`, `BX=0000h`.
  * Treat `AX=0002h` hide cursor as a no-op.
  * Record the last interrupt vector/AX diagnostics.
* `scripts/test_all.ps1`
  * Update the current expected `piu_1st` observation point to the new result.

## Excluded

* Real mouse cursor/position/button-state HLE
* Win32 input device integration
* Modifying original executable code

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
