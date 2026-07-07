# Relocatable Runtime Image Dry-Run 작업 지시

원본 DOS/4GW LE image를 낮은 고정 주소가 아닌 안전한 새 runtime base로 이동하는 dry-run 계획을 추가한다.

## 작업 범위

* runtime 모듈에 relocatable runtime image plan 구조 추가
* 기본 relocated image base `0x01000000` 적용
* original object base → relocated object base 매핑 계산
* relocated entry와 stack top 계산
* relocated relocation dry-run 통계 계산
* analyzer 출력 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* 실제 메모리 할당
* relocated image buffer write
* page protection 설정
* 원본 entry 호출

## 검증 절차

1. `cmake --build build\vs2022_debug --config Debug`를 실행한다.
2. `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
3. `scripts\build_win32_x86.bat`를 실행한다.
4. `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
5. relocatable runtime image dry-run 출력이 포함되는지 확인한다.

## Work Order

Add a dry-run plan for moving the original DOS/4GW LE image to a safe new runtime base instead of the low fixed address range.

## Scope

* Add relocatable runtime image plan structures to the runtime module.
* Use default relocated image base `0x01000000`.
* Calculate original object base to relocated object base mapping.
* Calculate relocated entry and stack top.
* Calculate relocated relocation dry-run statistics.
* Add analyzer output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.
* Write a work log after completion.

## Out of Scope

* Actual memory allocation.
* Writing to a relocated image buffer.
* Page protection setup.
* Calling the original entry point.

## Verification Procedure

1. Run `cmake --build build\vs2022_debug --config Debug`.
2. Run `build\vs2022_debug\Debug\repiu_exe_analyzer.exe`.
3. Run `scripts\build_win32_x86.bat`.
4. Run `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`.
5. Confirm that relocatable runtime image dry-run output is included.
