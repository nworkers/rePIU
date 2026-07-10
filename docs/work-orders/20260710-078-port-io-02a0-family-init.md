# Port I/O 0x02A0 계열 초기화 write 작업 지시

## 목표

`piu_1st`가 도달한 `OUT DX,EAX port=0x02A2 value=0x00000000`을 관측 기반 allow-list no-op으로 처리하고, 관련 조건을 helper로 묶는다.

## 범위

* `src/platform/win32/execution_trampoline.cpp`
  * 관측된 0x02A0 계열 초기화 write 판정 helper를 추가한다.
  * `port=0x02A2`, `value=0x00000000`, `width=4`를 allow-list에 추가한다.
* `scripts/test_all.ps1`
  * `piu_1st`의 현재 기대 관측 지점을 새 결과에 맞춰 갱신한다.

## 제외

* 0x02A0 포트 범위 전체 허용
* 장치 의미 확정 또는 하드웨어 모델 구현
* 원본 실행 파일 코드 수정

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Port I/O 0x02A0-family Initialization Write Work Order

## Goal

Handle `OUT DX,EAX port=0x02A2 value=0x00000000`, reached by `piu_1st`, as an observation-based allow-listed no-op and group the related predicates into a helper.

## Scope

* `src/platform/win32/execution_trampoline.cpp`
  * Add a helper that identifies observed 0x02A0-family initialization writes.
  * Add `port=0x02A2`, `value=0x00000000`, `width=4` to the allow-list.
* `scripts/test_all.ps1`
  * Update the current expected `piu_1st` observation point to the new result.

## Excluded

* Allowing the entire 0x02A0 port range
* Assigning device-level meaning or implementing a hardware model
* Modifying original executable code

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
