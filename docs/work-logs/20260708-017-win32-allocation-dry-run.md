# Win32 주소 범위 Dry-Run 작업 로그

## 작업 결과

Win32 runtime memory policy에 실제 할당 전 주소 범위 검사 단계를 추가했다.

`Win32AddressRangeProbe` 구조와 `ProbeWin32RuntimeAddressRange` 함수를 추가했으며, analyzer가 `VirtualQuery` 기반 dry-run 결과를 출력하도록 갱신했다.

이번 단계는 실제 메모리를 예약하지 않는다. 검사 결과는 이후 `VirtualAlloc` 기반 executable memory allocation 정책을 설계하기 위한 관찰 자료로 사용한다.

## 변경 파일

* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-017-win32-allocation-dry-run.md`
* `docs/work-orders/20260708-017-win32-allocation-dry-run.md`

## 검증

* `cmake --build build\vs2022_debug --config Debug`: 성공
* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: 성공
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공

## 확인된 출력

x64 analyzer:

* `Win32 host pointer bits: 64`
* `Win32 direct x86 execution: unsupported`
* `Win32 allocation dry run: valid`
* `Win32 target range available: true`

Win32 x86 analyzer:

* `Win32 host pointer bits: 32`
* `Win32 direct x86 execution: supported`
* `Win32 allocation dry run: valid`
* `Win32 target range available: false`
* `Win32 first blocking block base: 0x00180000`
* `Win32 first blocking block size: 0x00001000`
* `Win32 first blocking block state: MEM_COMMIT`

## 회고

실제 목표인 Win32 x86 프로세스에서는 DOS/4GW 이미지가 요구하는 `[0x00010000, 0x005E7000)` 범위 전체가 비어 있지 않았다.

다음 단계에서 단순히 현재 analyzer 프로세스 안에 고정 주소로 예약하려고 하면 실패할 가능성이 높다. 실제 실행 host를 별도 32-bit executable로 구성하고 초기화 순서를 최소화하거나, 가능한 경우 이미지 베이스 충돌을 피할 수 있는 예약 전략을 먼저 설계해야 한다.

## Work Log

## Result

Added a pre-allocation address range probe to the Win32 runtime memory policy.

Added the `Win32AddressRangeProbe` structure and `ProbeWin32RuntimeAddressRange` function, and updated the analyzer to print `VirtualQuery`-based dry-run results.

This step does not reserve memory. The probe result is used as observation data for a later `VirtualAlloc`-based executable memory allocation policy.

## Changed Files

* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-017-win32-allocation-dry-run.md`
* `docs/work-orders/20260708-017-win32-allocation-dry-run.md`

## Verification

* `cmake --build build\vs2022_debug --config Debug`: passed
* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: passed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed

## Observed Output

x64 analyzer:

* `Win32 host pointer bits: 64`
* `Win32 direct x86 execution: unsupported`
* `Win32 allocation dry run: valid`
* `Win32 target range available: true`

Win32 x86 analyzer:

* `Win32 host pointer bits: 32`
* `Win32 direct x86 execution: supported`
* `Win32 allocation dry run: valid`
* `Win32 target range available: false`
* `Win32 first blocking block base: 0x00180000`
* `Win32 first blocking block size: 0x00001000`
* `Win32 first blocking block state: MEM_COMMIT`

## Retrospective

In the actual target Win32 x86 process, the full DOS/4GW image range `[0x00010000, 0x005E7000)` is not free.

Trying to reserve the fixed range directly inside the current analyzer process is likely to fail. The next step should first design a dedicated 32-bit execution host with a minimal startup footprint, or another reservation strategy that can avoid image base conflicts when possible.
