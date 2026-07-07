# Win32 Relocated Memory Placement 작업 지시

Relocated image buffer를 Win32 x86 process memory에 배치하고 object별 protection을 적용한다.

## 작업 범위

* Win32 x86 host image base를 `0x10000000`으로 변경
* Win32 relocated image placement API 추가
* `VirtualAlloc` reserve/commit 구현
* object buffer copy 구현
* object flags 기반 `VirtualProtect` 적용
* execution host에서 PIU.EXE load → relocated buffer 생성 → Win32 memory placement 수행
* analyzer 기존 동작 유지
* 관련 설계 문서 갱신
* 작업 완료 후 작업 로그 작성

## 제외 범위

* 원본 entry 호출
* guest stack 전환
* HLE dispatcher 연결
* skipped relocation 전체 해석

## 검증 절차

1. `scripts\build_win32_x86.bat`를 실행한다.
2. `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`를 실행한다.
3. relocated placement 출력과 object copy/protection count를 확인한다.
4. `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`를 실행한다.
5. `cmake --build build\vs2022_debug --config Debug`를 실행한다.

## Work Order

Place the relocated image buffer into Win32 x86 process memory and apply per-object protection.

## Scope

* Change Win32 x86 host image base to `0x10000000`.
* Add Win32 relocated image placement API.
* Implement `VirtualAlloc` reserve/commit.
* Implement object buffer copy.
* Apply `VirtualProtect` based on object flags.
* Make the execution host load `PIU.EXE`, build the relocated buffer, and place it into Win32 memory.
* Preserve existing analyzer behavior.
* Update related design documents.
* Write a work log after completion.

## Out of Scope

* Calling the original entry point.
* Switching to the guest stack.
* Connecting the HLE dispatcher.
* Fully interpreting skipped relocations.

## Verification Procedure

1. Run `scripts\build_win32_x86.bat`.
2. Run `build\vs2022_win32_debug\Debug\repiu_win32_execution_host.exe`.
3. Confirm relocated placement output and object copy/protection counts.
4. Run `build\vs2022_win32_debug\Debug\repiu_exe_analyzer.exe`.
5. Run `cmake --build build\vs2022_debug --config Debug`.

## 추가 작업

`0x01000000`이 점유되어 있으면 execution host는 더 높은 relocated base 후보를 probe하고, 선택된 base로 relocated image를 다시 생성한다.

## Additional Work

If `0x01000000` is occupied, the execution host probes higher relocated base candidates and rebuilds the relocated image for the selected base.
