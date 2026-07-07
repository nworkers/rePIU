# Win32 Host Image Base 정책 작업 로그

## 작업 결과

Win32 x86 host executable이 원본 DOS/4GW 이미지의 고정 주소 범위와 직접 충돌하지 않도록 CMake 링크 정책을 추가했다.

`repiu_configure_win32_execution_host` 함수를 추가했으며, MSVC 32-bit Win32 executable target에 `/BASE:0x01000000`과 `/DYNAMICBASE:NO`를 적용하도록 했다.

현재 dedicated execution host가 없으므로 `repiu_exe_analyzer`에 먼저 적용했다. analyzer 출력에는 `Win32 host image base policy: 0x01000000`을 추가했다.

## 변경 파일

* `CMakeLists.txt`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-018-win32-host-image-base.md`
* `docs/work-orders/20260708-018-win32-host-image-base.md`

## 검증

* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공
* `cmake --build build\vs2022_debug --config Debug`: 성공
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: 성공

## 확인된 출력

Win32 x86 analyzer:

* `Win32 host image base policy: 0x01000000`
* `Win32 host pointer bits: 32`
* `Win32 direct x86 execution: supported`
* `Win32 target range available: false`
* `Win32 first blocking block base: 0x00010000`
* `Win32 first blocking block size: 0x00003000`
* `Win32 first blocking block state: MEM_COMMIT`

x64 analyzer:

* `Win32 host pointer bits: 64`
* `Win32 direct x86 execution: unsupported`
* `Win32 target range available: true`

## 회고

Win32 x86 analyzer에 host image base 정책은 적용되었지만, 목표 주소 범위는 여전히 완전히 비어 있지 않다.

이 결과는 host image base 이동만으로는 충분하지 않다는 뜻이다. 다음 단계에서는 dedicated Win32 x86 execution host를 분리하고, C/C++ runtime 초기화 이후가 아니라 가능한 가장 이른 시점에 목표 범위를 `VirtualAlloc`으로 선점할 수 있는 구조를 설계해야 한다.

## Work Log

## Result

Added a CMake link policy so the Win32 x86 host executable does not directly collide with the fixed address range required by the original DOS/4GW image.

Added the `repiu_configure_win32_execution_host` function. For MSVC 32-bit Win32 executable targets, it applies `/BASE:0x01000000` and `/DYNAMICBASE:NO`.

Because there is no dedicated execution host yet, the policy is first applied to `repiu_exe_analyzer`. The analyzer now prints `Win32 host image base policy: 0x01000000`.

## Changed Files

* `CMakeLists.txt`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-018-win32-host-image-base.md`
* `docs/work-orders/20260708-018-win32-host-image-base.md`

## Verification

* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed
* `cmake --build build\vs2022_debug --config Debug`: passed
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: passed

## Observed Output

Win32 x86 analyzer:

* `Win32 host image base policy: 0x01000000`
* `Win32 host pointer bits: 32`
* `Win32 direct x86 execution: supported`
* `Win32 target range available: false`
* `Win32 first blocking block base: 0x00010000`
* `Win32 first blocking block size: 0x00003000`
* `Win32 first blocking block state: MEM_COMMIT`

x64 analyzer:

* `Win32 host pointer bits: 64`
* `Win32 direct x86 execution: unsupported`
* `Win32 target range available: true`

## Retrospective

The host image base policy is applied to the Win32 x86 analyzer, but the target address range is still not fully free.

This means moving the host image base alone is not enough. The next step should design a dedicated Win32 x86 execution host that can reserve the target range with `VirtualAlloc` as early as possible, before normal C/C++ runtime activity occupies the low address range.
