# Relocatable Runtime Image Dry-Run 작업 로그

## 작업 결과

Relocatable runtime image dry-run 구조를 추가했다.

기본 relocated image base는 `0x01000000`으로 두었고, 원본 image base `0x00010000`과의 delta `0x00FF0000`을 사용해 object별 새 base를 계산한다.

analyzer는 relocated object map, relocated entry, relocated stack top, relocated HLE reserve base, relocated relocation dry-run 통계를 출력한다.

이번 단계는 실제 메모리를 할당하거나 relocated image buffer에 값을 쓰지 않는다.

## 변경 파일

* `include/repiu/runtime/runtime_memory.h`
* `src/runtime/runtime_memory.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-020-relocatable-runtime-image.md`
* `docs/work-orders/20260708-020-relocatable-runtime-image.md`

## 검증

* `cmake --build build\vs2022_debug --config Debug`: 성공
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: 성공
* `scripts\build_win32_x86.bat`: 성공
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: 성공

## 확인된 출력

* `Relocatable runtime image dry run: valid`
* `Relocatable original image base: 0x00010000`
* `Relocatable image base: 0x01000000`
* `Relocatable delta: 0x00FF0000`
* `Relocatable entry: 0x010F3818`
* `Relocatable stack top: 0x015D6E10`
* `Relocatable HLE reserve base: 0x015D7000`
* `Relocatable applied relocations: 14637`
* `Relocatable skipped relocations: 10`
* `Relocatable failed relocations: 0`
* `Relocatable first applied relocation: ... original=0x002A4B3D relocated=0x01294B3D`

## 회고

원본 object 간격을 유지한 채 전체 image를 `0x01000000`으로 옮기는 계산은 성립한다.

relocated relocation dry-run의 applied/skipped/failed count는 기존 relocation dry-run과 일치한다.

다음 단계에서는 이 계획을 실제 relocated image buffer로 구체화하고, skipped relocation 10개의 의미를 더 세분화해야 한다.

## Work Log

## Result

Added the relocatable runtime image dry-run structure.

The default relocated image base is `0x01000000`. The plan calculates object bases using delta `0x00FF0000` from the original image base `0x00010000`.

The analyzer now prints the relocated object map, relocated entry, relocated stack top, relocated HLE reserve base, and relocated relocation dry-run statistics.

This step does not allocate memory or write values into a relocated image buffer.

## Changed Files

* `include/repiu/runtime/runtime_memory.h`
* `src/runtime/runtime_memory.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-020-relocatable-runtime-image.md`
* `docs/work-orders/20260708-020-relocatable-runtime-image.md`

## Verification

* `cmake --build build\vs2022_debug --config Debug`: passed
* `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`: passed
* `scripts\build_win32_x86.bat`: passed
* `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`: passed

## Observed Output

* `Relocatable runtime image dry run: valid`
* `Relocatable original image base: 0x00010000`
* `Relocatable image base: 0x01000000`
* `Relocatable delta: 0x00FF0000`
* `Relocatable entry: 0x010F3818`
* `Relocatable stack top: 0x015D6E10`
* `Relocatable HLE reserve base: 0x015D7000`
* `Relocatable applied relocations: 14637`
* `Relocatable skipped relocations: 10`
* `Relocatable failed relocations: 0`
* `Relocatable first applied relocation: ... original=0x002A4B3D relocated=0x01294B3D`

## Retrospective

Moving the full image to `0x01000000` while preserving original object spacing is a valid calculation.

The relocated relocation dry-run applied/skipped/failed counts match the existing relocation dry-run.

The next step should materialize this plan into an actual relocated image buffer and further classify the 10 skipped relocations.
