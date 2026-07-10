# Port I/O HLE 라우터 작업 지시

## 목표

`piu_1st`가 `INT 33h` 이후 도달한 `OUT DX,EAX` Port I/O를 관측 가능한 HLE 라우터로 처리한다.

## 범위

* `include/repiu/platform/win32/execution_trampoline.h`
  * Port I/O 진단 구조체를 추가한다.
* `src/platform/win32/execution_trampoline.cpp`
  * `66 EF` (`OUT DX,EAX`) 해석을 추가한다.
  * `port=0x02AC`, `value=0x00000010`, `width=4` 조합만 allow-list no-op으로 처리한다.
  * 처리된 Port I/O를 진단 구조체에 기록한다.
* `src/host/win32/main.cpp`
  * Port I/O 진단 로그를 출력한다.
* `scripts/test_all.ps1`
  * 새 Port I/O 로그와 다음 관측 지점을 검증한다.

## 제외

* VGA/사운드/타이머 같은 실제 하드웨어 장치 구현
* 모든 Port I/O의 무조건 no-op 처리
* 원본 실행 파일 코드 수정

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Port I/O HLE Router Work Order

## Goal

Handle the `OUT DX,EAX` Port I/O reached by `piu_1st` after `INT 33h` through an observable HLE router.

## Scope

* `include/repiu/platform/win32/execution_trampoline.h`
  * Add a Port I/O diagnostics structure.
* `src/platform/win32/execution_trampoline.cpp`
  * Decode `66 EF` (`OUT DX,EAX`).
  * Handle only the allow-listed no-op combination `port=0x02AC`, `value=0x00000010`, `width=4`.
  * Record handled Port I/O in diagnostics.
* `src/host/win32/main.cpp`
  * Print Port I/O diagnostics.
* `scripts/test_all.ps1`
  * Verify the new Port I/O log and the next observation point.

## Excluded

* Implementing real hardware devices such as VGA, sound, or timers
* Treating every Port I/O as an unconditional no-op
* Modifying original executable code

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
