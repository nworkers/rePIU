# Port I/O 0x02A0 allow-list 확장 작업 지시

## 목표

`piu_1st`가 Port I/O 라우터 추가 후 도달한 `OUT DX,EAX port=0x02A0 value=0x00000001`을 관측 기반 allow-list no-op으로 처리한다.

## 범위

* `src/platform/win32/execution_trampoline.cpp`
  * Port I/O allow-list에 `port=0x02A0`, `value=0x00000001`, `width=4`를 추가한다.
* `scripts/test_all.ps1`
  * `piu_1st`의 현재 기대 관측 지점을 새 결과에 맞춰 갱신한다.

## 제외

* `0x02A0`의 장치 의미 확정
* 모든 Port I/O의 무조건 no-op 처리
* 원본 실행 파일 코드 수정

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Port I/O 0x02A0 Allow-list Extension Work Order

## Goal

Handle `OUT DX,EAX port=0x02A0 value=0x00000001`, reached by `piu_1st` after adding the Port I/O router, as an observation-based allow-listed no-op.

## Scope

* `src/platform/win32/execution_trampoline.cpp`
  * Add `port=0x02A0`, `value=0x00000001`, `width=4` to the Port I/O allow-list.
* `scripts/test_all.ps1`
  * Update the current expected `piu_1st` observation point to the new result.

## Excluded

* Assigning device-level meaning to `0x02A0`
* Treating every Port I/O as an unconditional no-op
* Modifying original executable code

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
