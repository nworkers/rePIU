# Relocated Image Buffer 작업 지시

Relocatable runtime image dry-run 계획을 실제 C++ buffer로 구체화하고, relocated relocation 값을 buffer에 기록한다.

## 작업 범위

* `RelocatedRuntimeImage` 구조 추가
* object별 relocated buffer 복사
* source kind `0x07` relocation write 구현
* analyzer 출력 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* OS executable memory 할당
* page protection 설정
* 원본 entry 호출
* skipped relocation 전체 해석

## 검증 절차

1. `cmake --build build\vs2022_debug --config Debug`를 실행한다.
2. `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
3. `scripts\build_win32_x86.bat`를 실행한다.
4. `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
5. relocated image buffer 출력과 relocation count를 확인한다.

## Work Order

Materialize the relocatable runtime image dry-run plan into C++ buffers and write relocated relocation values into those buffers.

## Scope

* Add `RelocatedRuntimeImage`.
* Copy relocated buffers per object.
* Implement source kind `0x07` relocation writes.
* Add analyzer output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.
* Write a work log after completion.

## Out of Scope

* OS executable memory allocation.
* Page protection setup.
* Calling the original entry point.
* Full skipped relocation interpretation.

## Verification Procedure

1. Run `cmake --build build\vs2022_debug --config Debug`.
2. Run `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`.
3. Run `scripts\build_win32_x86.bat`.
4. Run `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`.
5. Confirm relocated image buffer output and relocation counts.
