# Relocated Image Buffer 작업 로그

## 작업 결과

Relocatable runtime image plan을 실제 C++ owned buffer로 구체화하는 `RelocatedRuntimeImage`를 추가했다.

각 object buffer는 기존 mapped object memory에서 복사하며, source kind `0x07` relocation에 대해 relocated target address를 32-bit little-endian 값으로 기록한다.

이번 단계는 아직 OS executable memory를 할당하지 않고, 원본 entry도 호출하지 않는다.

## 변경 파일

* `include/repiu/runtime/runtime_memory.h`
* `src/runtime/runtime_memory.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-021-relocated-image-buffer.md`
* `docs/work-orders/20260708-021-relocated-image-buffer.md`

## 검증

* `cmake --build build\vs2022_debug --config Debug`: 성공
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: 성공
* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공

## 확인된 출력

* `Relocated image buffer: valid`
* `Relocated image buffer base: 0x01000000`
* `Relocated image buffer entry: 0x010F3818`
* `Relocated image buffer stack top: 0x015D6E10`
* `Relocated image applied relocations: 14637`
* `Relocated image skipped relocations: 10`
* `Relocated image failed relocations: 0`
* `Relocated image first written relocation: ... previous=0x002A4B3D applied=0x01294B3D`

## 회고

relocatable dry-run이 실제 buffer write 단계로 자연스럽게 이어졌다.

첫 relocation sample에서 기존 original relocation 값이 relocated 값으로 교체되는 것을 확인했으므로, 다음 단계에서는 이 buffer를 Win32 x86 process memory에 배치하는 정책으로 넘어갈 수 있다.

남은 위험은 skipped relocation 10개이며, 실행 전 또는 실행 중 문제가 생기면 이 항목을 우선 분석해야 한다.

## Work Log

## Result

Added `RelocatedRuntimeImage`, which materializes the relocatable runtime image plan into C++ owned buffers.

Each object buffer is copied from the existing mapped object memory, and source kind `0x07` relocations write the relocated target address as a 32-bit little-endian value.

This step still does not allocate OS executable memory and does not call the original entry point.

## Changed Files

* `include/repiu/runtime/runtime_memory.h`
* `src/runtime/runtime_memory.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-021-relocated-image-buffer.md`
* `docs/work-orders/20260708-021-relocated-image-buffer.md`

## Verification

* `cmake --build build\vs2022_debug --config Debug`: passed
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: passed
* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed

## Observed Output

* `Relocated image buffer: valid`
* `Relocated image buffer base: 0x01000000`
* `Relocated image buffer entry: 0x010F3818`
* `Relocated image buffer stack top: 0x015D6E10`
* `Relocated image applied relocations: 14637`
* `Relocated image skipped relocations: 10`
* `Relocated image failed relocations: 0`
* `Relocated image first written relocation: ... previous=0x002A4B3D applied=0x01294B3D`

## Retrospective

The relocatable dry-run now naturally materializes into actual buffer writes.

The first relocation sample confirms that the original relocation value is replaced by the relocated value, so the next step can move toward placing this buffer into Win32 x86 process memory.

The remaining risk is the 10 skipped relocations. If execution fails later, these should be analyzed first.
