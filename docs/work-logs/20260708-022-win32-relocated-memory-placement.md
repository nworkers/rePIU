# Win32 Relocated Memory Placement 작업 로그

## 작업 결과

Relocated image buffer를 Win32 process memory에 배치하는 API와 execution host 연결을 추가했다.

Win32 x86 host image base는 relocated image 후보와 충돌하지 않도록 `0x10000000`으로 변경했다.

execution host는 relocated base 후보를 probe하고, 비어 있는 첫 후보를 선택한 뒤 해당 base로 relocatable plan과 relocated image buffer를 다시 생성한다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/win32_execution_host/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-022-win32-relocated-memory-placement.md`
* `docs/work-orders/20260708-022-win32-relocated-memory-placement.md`

## 검증

* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`: 성공
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공
* `cmake --build build\vs2022_debug --config Debug`: 성공

## 확인된 출력

Win32 x86 execution host:

* `Win32 host image base policy: 0x10000000`
* `Win32 relocated base candidate 0x01000000: occupied`
* `Win32 relocated base candidate 0x02000000: available`
* `Win32 selected relocated image base: 0x02000000`
* `Win32 relocated image placement result: placed`
* `Win32 relocated image placed base: 0x02000000`
* `Win32 relocated image placed size: 0x005D7000`
* `Win32 relocated image copied objects: 4`
* `Win32 relocated image protected objects: 4`

## 회고

`0x01000000`은 실제 Win32 x86 process에서 이미 점유되어 있었지만, 후보 선택을 통해 `0x02000000`에 relocated image를 성공적으로 배치했다.

이제 원본 image는 낮은 DOS/4GW 고정 주소가 아니라 선택된 safe base에 올라갈 수 있다.

다음 단계는 이 배치 위에서 guest stack과 entry trampoline을 설계하는 것이다. 다만 원본 entry 호출 전 skipped relocation 10개, 특히 selector/far pointer 계열 의미를 더 확인하는 것이 안전하다.

## Work Log

## Result

Added Win32 process-memory placement API for relocated image buffers and connected it to the execution host.

The Win32 x86 host image base was changed to `0x10000000` so it does not collide with relocated image candidates.

The execution host probes relocated base candidates, selects the first free candidate, then rebuilds the relocatable plan and relocated image buffer for that selected base.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/win32_execution_host/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-022-win32-relocated-memory-placement.md`
* `docs/work-orders/20260708-022-win32-relocated-memory-placement.md`

## Verification

* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`: passed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed
* `cmake --build build\vs2022_debug --config Debug`: passed

## Observed Output

Win32 x86 execution host:

* `Win32 host image base policy: 0x10000000`
* `Win32 relocated base candidate 0x01000000: occupied`
* `Win32 relocated base candidate 0x02000000: available`
* `Win32 selected relocated image base: 0x02000000`
* `Win32 relocated image placement result: placed`
* `Win32 relocated image placed base: 0x02000000`
* `Win32 relocated image placed size: 0x005D7000`
* `Win32 relocated image copied objects: 4`
* `Win32 relocated image protected objects: 4`

## Retrospective

`0x01000000` was already occupied in the actual Win32 x86 process, but candidate selection successfully placed the relocated image at `0x02000000`.

The original image can now be placed at a selected safe base instead of the low DOS/4GW fixed address.

The next step is to design the guest stack and entry trampoline on top of this placement. Before calling the original entry, it would be safer to further inspect the 10 skipped relocations, especially selector/far pointer cases.
