# Win32 Execution Host 초기 예약 작업 로그

## 작업 결과

별도 `repiu_win32_execution_host` executable target을 추가했다.

`TargetProfile`에 `TargetRuntimeReservationHint`를 추가했고, `piu_1st`에 이전 runtime memory dry-run에서 확인한 `base=0x00010000`, `size=0x005D7000` 범위를 기록했다.

Win32 runtime memory policy에는 fixed range 기반 policy 생성과 `VirtualAlloc(MEM_RESERVE)` 예약 시도 API를 추가했다.

execution host는 시작 직후 target profile의 예약 힌트로 policy를 만들고, `VirtualQuery` probe와 `VirtualAlloc` 예약 시도 결과를 출력한다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/target/target_profile.h`
* `src/target/target_profile.cpp`
* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/win32_execution_host/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-019-win32-execution-host-reserve.md`
* `docs/work-orders/20260708-019-win32-execution-host-reserve.md`

## 검증

* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`: 성공
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공
* `cmake --build build\vs2022_debug --config Debug`: 성공
* `build\vs2022_debug\Debug\repiu_win32_execution_host.exe`: 성공

## 확인된 출력

Win32 x86 execution host:

* `Win32 host image base policy: 0x01000000`
* `Win32 host pointer bits: 32`
* `Win32 direct x86 execution: supported`
* `Win32 host range available: false`
* `Win32 host first blocking block base: 0x00010000`
* `Win32 host first blocking block size: 0x00003000`
* `Win32 host first blocking block state: MEM_COMMIT`
* `Win32 early reservation result: not reserved`
* `Win32 reservation error: 487`

x64 execution host:

* `Win32 host pointer bits: 64`
* `Win32 direct x86 execution: unsupported`
* `Win32 host range available: true`
* `Win32 early reservation result: reserved`
* `Win32 reserved base: 0x00010000`
* `Win32 reserved size: 0x005D7000`

## 회고

전용 execution host에서도 32-bit 프로세스의 `0x00010000`부터 `0x3000` 크기 블록이 이미 `MEM_COMMIT` 상태다.

따라서 일반 C++ `main` 이후에 목표 범위를 예약하는 방식은 충분히 이르지 않다.

다음 단계에서는 MSVC `/ENTRY` 기반 custom entry point 또는 아주 작은 bootstrap executable을 설계해 CRT 초기화 전에 `VirtualAlloc` 예약을 시도해야 한다. 이 경로는 C++ runtime 사용 제약이 생기므로 별도 설계가 필요하다.

## Work Log

## Result

Added a dedicated `repiu_win32_execution_host` executable target.

Added `TargetRuntimeReservationHint` to `TargetProfile`, and recorded the previously observed runtime range `base=0x00010000`, `size=0x005D7000` for `piu_1st`.

Added fixed-range policy creation and `VirtualAlloc(MEM_RESERVE)` reservation attempt APIs to the Win32 runtime memory policy module.

The execution host now builds a policy from the target profile reservation hint immediately after startup and prints both `VirtualQuery` probe and `VirtualAlloc` reservation attempt results.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/target/target_profile.h`
* `src/target/target_profile.cpp`
* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/win32_execution_host/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-019-win32-execution-host-reserve.md`
* `docs/work-orders/20260708-019-win32-execution-host-reserve.md`

## Verification

* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`: passed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed
* `cmake --build build\vs2022_debug --config Debug`: passed
* `build\vs2022_debug\Debug\repiu_win32_execution_host.exe`: passed

## Observed Output

Win32 x86 execution host:

* `Win32 host image base policy: 0x01000000`
* `Win32 host pointer bits: 32`
* `Win32 direct x86 execution: supported`
* `Win32 host range available: false`
* `Win32 host first blocking block base: 0x00010000`
* `Win32 host first blocking block size: 0x00003000`
* `Win32 host first blocking block state: MEM_COMMIT`
* `Win32 early reservation result: not reserved`
* `Win32 reservation error: 487`

x64 execution host:

* `Win32 host pointer bits: 64`
* `Win32 direct x86 execution: unsupported`
* `Win32 host range available: true`
* `Win32 early reservation result: reserved`
* `Win32 reserved base: 0x00010000`
* `Win32 reserved size: 0x005D7000`

## Retrospective

Even in the dedicated execution host, the 32-bit process already has a `MEM_COMMIT` block at `0x00010000` with size `0x3000`.

This means reserving the target range after normal C++ `main` is still not early enough.

The next step should design an MSVC `/ENTRY` custom entry point or a very small bootstrap executable that attempts `VirtualAlloc` before CRT initialization. That path requires a separate design because it limits C++ runtime usage.

## 추가 결정

사용자와 논의한 결과, 원래 주소에 그대로 할당하는 방법은 어렵다고 보고 relocation 기반 로드를 우선 추진하기로 결정했다.

고정 주소 예약을 더 깊게 파기보다, 다음 작업은 `Relocatable Runtime Image` 설계와 dry-run으로 진행한다.

## Additional Decision

After discussion with the user, fixed-address allocation is considered difficult enough that relocation-based loading should be prioritized.

Instead of digging deeper into fixed-address reservation first, the next work will proceed with `Relocatable Runtime Image` design and dry-run.
