# Win32/x86 Runtime Memory Policy 작업 지시

runtime memory dry-run 결과를 기반으로 Win32/x86 직접 실행 가능성과 필요한 예약 주소 범위를 보고한다.

## 작업 범위

* Win32 runtime memory policy report 구조 추가
* host pointer bit 수와 direct x86 execution 지원 여부 계산
* preferred allocation base와 required reserve size 계산
* analyzer 출력 추가
* `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신

## 제외 범위

* 실제 `VirtualAlloc` 호출
* 원본 entry 호출
* selector/GDT 구현
* x64 helper process 구현

## 검증 절차

1. Debug 빌드를 수행한다.
2. `repiu_exe_analyzer`를 인자 없이 실행한다.
3. Win32 runtime memory policy 출력과 기존 runtime/relocation 결과를 확인한다.

## Work Order

Report Win32/x86 direct execution capability and required reserve address range from the runtime memory dry-run result.

## Scope

* Add a Win32 runtime memory policy report structure.
* Calculate host pointer bit count and direct x86 execution support.
* Calculate preferred allocation base and required reserve size.
* Add analyzer output.
* Update `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.

## Out of Scope

* Actual `VirtualAlloc` calls.
* Calling the original entry point.
* Selector/GDT implementation.
* x64 helper process implementation.

## Verification Procedure

1. Build the Debug target.
2. Run `repiu_exe_analyzer` without arguments.
3. Confirm Win32 runtime memory policy output and existing runtime/relocation results.
