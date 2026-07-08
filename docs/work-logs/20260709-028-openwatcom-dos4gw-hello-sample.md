# OpenWatcom DOS/4GW Hello 샘플 작업 로그

## 결과

OpenWatcom v2 설치 스크립트와 DOS/4GW Hello 샘플 빌드 스크립트를 추가했다.
OpenWatcom 설치물과 다운로드 cache는 각각 `tools/openwatcom/`, `tools/downloads/` 아래에 두고 Git 추적에서 제외했다.

샘플은 `samples/dos4gw_hello/hello.c`의 표준 C `main`/`puts` 프로그램이다.
빌드 스크립트는 `wcl386 -bt=dos -l=dos4g`를 사용해 `samples/dos4gw_hello/build/hello.exe`를 생성한다.
이 executable은 실제 DOS/4G runtime에서 요구하는 LE stack object를 포함한다.

이전 실험 산출물 `hello_c.exe`는 C object를 사용했지만 stack object가 없어 실제 실행 환경에서 `DOS/4G fatal error (1317): program has no stack` 오류가 발생할 수 있었다.
최종 기준에서는 custom `_start`, `int 3` trap, stack object 없는 executable을 샘플로 사용하지 않는다.
혼동을 막기 위해 로컬 build 산출물의 `hello_c.exe`는 제거했다.

`dos4gw_hello` target profile은 `samples/dos4gw_hello/build/hello.exe`를 가리킨다.
analyzer와 loader는 명령행 첫 인자로 target id를 선택할 수 있다.
기존 `piu_1st` target은 기본값으로 유지했다.

loader에는 `dos4gw_hello` target 전용 DOS console HLE 실행 경로를 추가했다.
OpenWatcom C runtime startup과 console 출력에 필요한 최소 DOS/DPMI interrupt를 처리하고, 기존 `piu_1st` 실행 경로의 privileged instruction 관찰은 유지했다.

## 검증

* `cmd /c scripts\build_dos4gw_hello.bat`: 성공, `hello.exe` 생성
* OpenWatcom `wdump -q -e samples\dos4gw_hello\build\hello.exe`: `object # for initial ESP = 00000002H`, `size of stack = 00010000H` 확인
* `build\vs2022_win32_debug_c_sample\Debug\repiu_exe_analyzer.exe dos4gw_hello`: 성공, `LE stack object: 2`, `LE stack offset: 0x00010380` 확인
* `build\vs2022_win32_debug_c_sample\Debug\repiu_loader_win32.exe dos4gw_hello`: 성공, `Win32 HLE console output:` 아래 `Hello, world!` 출력 확인
* `build\vs2022_win32_debug_c_sample\Debug\repiu_loader_win32.exe piu_1st`: 성공, 기존 `0x020F3890` privileged instruction 예외 관찰 유지
* x64 Debug build: 성공
* `build\vs2022_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: 성공, 64-bit host에서는 직접 x86 실행을 시도하지 않고 unsupported로 보고

## 제한 사항

현재 작업 환경에는 DOSBox/DOSBox-X 같은 실제 DOS 실행기가 없어 `hello.exe`를 실제 DOS 화면에서 직접 실행하지는 못했다.
대신 OpenWatcom `wdump`와 analyzer로 DOS/4G fatal error 1317의 원인인 missing stack object가 제거되었음을 확인했다.
사용자 환경의 실제 DOS/4G 실행에서도 같은 `hello.exe`를 실행해 최종 확인한다.

# OpenWatcom DOS/4GW Hello Sample Work Log

## Result

Added the OpenWatcom v2 install script and the DOS/4GW Hello sample build script.
The OpenWatcom installation and download cache live under `tools/openwatcom/` and `tools/downloads/`, and both are excluded from Git tracking.

The sample is the standard C `main`/`puts` program at `samples/dos4gw_hello/hello.c`.
The build script uses `wcl386 -bt=dos -l=dos4g` to generate `samples/dos4gw_hello/build/hello.exe`.
This executable includes the LE stack object required by the real DOS/4G runtime.

The previous experimental `hello_c.exe` used a C object but did not have a stack object, so it could fail in a real execution environment with `DOS/4G fatal error (1317): program has no stack`.
The final baseline does not use custom `_start`, `int 3` traps, or executables without stack objects as the sample.
The local build output `hello_c.exe` was removed to avoid confusion.

The `dos4gw_hello` target profile points to `samples/dos4gw_hello/build/hello.exe`.
Analyzer and loader can select a target id from the first command-line argument.
The existing `piu_1st` target remains the default.

The loader now has a DOS console HLE execution path enabled only for the `dos4gw_hello` target.
It handles the minimal DOS/DPMI interrupts required by OpenWatcom C runtime startup and console output while preserving the existing privileged-instruction observation for `piu_1st`.

## Verification

* `cmd /c scripts\build_dos4gw_hello.bat`: passed, generated `hello.exe`
* OpenWatcom `wdump -q -e samples\dos4gw_hello\build\hello.exe`: confirmed `object # for initial ESP = 00000002H` and `size of stack = 00010000H`
* `build\vs2022_win32_debug_c_sample\Debug\repiu_exe_analyzer.exe dos4gw_hello`: passed, confirmed `LE stack object: 2` and `LE stack offset: 0x00010380`
* `build\vs2022_win32_debug_c_sample\Debug\repiu_loader_win32.exe dos4gw_hello`: passed, confirmed `Hello, world!` under `Win32 HLE console output:`
* `build\vs2022_win32_debug_c_sample\Debug\repiu_loader_win32.exe piu_1st`: passed, preserved the existing privileged-instruction observation at `0x020F3890`
* x64 Debug build: passed
* `build\vs2022_debug\Debug\repiu_loader_win32.exe dos4gw_hello`: passed, reported unsupported direct x86 execution on a 64-bit host without attempting execution

## Limitations

This workspace does not have a real DOS runner such as DOSBox/DOSBox-X, so `hello.exe` was not executed directly in an actual DOS screen here.
Instead, OpenWatcom `wdump` and the analyzer confirmed that the missing stack object, the cause of DOS/4G fatal error 1317, has been removed.
The same `hello.exe` should be run in the user's real DOS/4G environment for final confirmation.
