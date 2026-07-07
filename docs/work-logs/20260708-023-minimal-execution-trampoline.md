# Minimal Execution Trampoline 작업 로그

## 작업 결과

Win32 minimal execution trampoline을 추가했다.

execution host는 relocated image를 Win32 process memory에 배치한 뒤 별도 thread에서 relocated entry를 한 번 호출한다.

thread proc은 `__try/__except`로 감싸져 있어 원본 코드에서 예외가 발생해도 host process가 바로 종료되지 않고, 예외 코드와 주소를 기록한다.

이 실행 시도는 guest stack 전환이나 HLE dispatcher 없이 수행되는 첫 관찰용 경로다.

## 변경 파일

* `include/repiu/platform/win32/execution_trampoline.h`
* `src/platform/win32/execution_trampoline.cpp`
* `src/tools/win32_execution_host/main.cpp`
* `CMakeLists.txt`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/TODO.md`
* `docs/design/20260708-023-minimal-execution-trampoline.md`
* `docs/work-orders/20260708-023-minimal-execution-trampoline.md`

## 검증

* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`: 성공
* `cmake --build build\vs2022_debug --config Debug`: 성공
* `build\vs2022_debug\Debug\repiu_win32_execution_host.exe`: 성공

## 확인된 출력

Win32 x86 execution host:

* `Win32 selected relocated image base: 0x02000000`
* `Win32 relocated image placement result: placed`
* `Win32 minimal execution entry: 0x020F3818`
* `Win32 minimal execution returned: false`
* `Win32 minimal execution exception caught: true`
* `Win32 minimal execution exception code: 0xC0000096`
* `Win32 minimal execution exception address: 0x020F3890`
* `Win32 minimal execution timed out: false`

x64 execution host:

* `Win32 minimal execution supported: false`
* `Win32 minimal execution attempted: false`

## 회고

원본 entry로 실제 진입하는 데 성공했고, host process를 죽이지 않고 첫 예외를 관찰했다.

예외 `0xC0000096`은 privileged instruction 계열이다. 다음 단계에서는 relocated exception address `0x020F3890`을 원본 주소로 역산하고, 해당 위치 주변 명령을 확인해야 한다.

현재 attempt는 host thread stack을 사용하므로, guest stack 전환 trampoline은 별도 작업으로 남긴다.

## Work Log

## Result

Added the Win32 minimal execution trampoline.

The execution host now places the relocated image in Win32 process memory and then calls the relocated entry once from a separate thread.

The thread proc is wrapped in `__try/__except`, so exceptions from original code are recorded instead of immediately terminating the host process.

This execution attempt is the first observation-only path and does not switch to the guest stack or provide an HLE dispatcher.

## Changed Files

* `include/repiu/platform/win32/execution_trampoline.h`
* `src/platform/win32/execution_trampoline.cpp`
* `src/tools/win32_execution_host/main.cpp`
* `CMakeLists.txt`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/TODO.md`
* `docs/design/20260708-023-minimal-execution-trampoline.md`
* `docs/work-orders/20260708-023-minimal-execution-trampoline.md`

## Verification

* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`: passed
* `cmake --build build\vs2022_debug --config Debug`: passed
* `build\vs2022_debug\Debug\repiu_win32_execution_host.exe`: passed

## Observed Output

Win32 x86 execution host:

* `Win32 selected relocated image base: 0x02000000`
* `Win32 relocated image placement result: placed`
* `Win32 minimal execution entry: 0x020F3818`
* `Win32 minimal execution returned: false`
* `Win32 minimal execution exception caught: true`
* `Win32 minimal execution exception code: 0xC0000096`
* `Win32 minimal execution exception address: 0x020F3890`
* `Win32 minimal execution timed out: false`

x64 execution host:

* `Win32 minimal execution supported: false`
* `Win32 minimal execution attempted: false`

## Retrospective

The host successfully entered the original relocated entry and observed the first exception without killing the host process.

Exception `0xC0000096` is a privileged-instruction class exception. The next step should translate relocated exception address `0x020F3890` back to the original address and inspect nearby instructions.

The current attempt uses a host thread stack, so guest stack switching remains a separate task.
