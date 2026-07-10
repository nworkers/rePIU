# DOS environment HLE 작업 지시

## 목표

`piu_1st`가 low-memory DS 문자열 스캔에서 읽는 값을 host 실제 환경 변수 기반 DOS environment block으로 제공한다.

## 범위

* Win32 실행 trampoline의 `ThreadContext`에 DOS environment byte buffer를 추가한다.
* guest 실행 시작 전에 host process 환경 변수를 DOS environment block 형식(`NAME=VALUE\0...\0\0`)으로 직렬화한다.
* `ReadSegmentByte`와 `ReadSegmentDword`가 DS low-memory offset에서 해당 buffer를 읽도록 변경한다.
* loader 출력에 environment block 크기와 마지막 low-memory 관측을 유지한다.
* `piu_1st` 실행 결과가 기존 chdir/open 관측을 유지하는지 확인한다.

## 정책

* host 환경 변수 이름은 현재 host 값 그대로 사용하되 DOS environment 관례에 맞춰 대문자로 정규화한다.
* buffer 끝은 double-NUL로 종료한다.
* buffer 범위를 벗어난 DS low-memory 읽기는 기존처럼 `0`을 반환한다.
* 민감한 환경 변수 값을 loader 로그에 직접 출력하지 않는다.

## 제외 범위

* PSP 전체 구조를 구현하지 않는다.
* environment segment selector 모델을 완성하지 않는다.
* 원본 게임 로직을 수정하지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# DOS Environment HLE Work Order

## Goal

Provide values read by the `piu_1st` low-memory DS string scan from a DOS environment block built from the host process environment.

## Scope

* Add a DOS environment byte buffer to the Win32 execution trampoline `ThreadContext`.
* Serialize host process environment variables before guest execution in DOS environment block format (`NAME=VALUE\0...\0\0`).
* Make `ReadSegmentByte` and `ReadSegmentDword` read from that buffer for DS low-memory offsets.
* Keep loader output for environment block size and the last low-memory observation.
* Verify that current chdir/open observations remain intact for `piu_1st`.

## Policy

* Use host environment variable values as-is, while uppercasing names to match DOS environment convention.
* Terminate the buffer with a double NUL.
* Keep returning `0` for DS low-memory reads outside the buffer.
* Do not print sensitive environment variable values directly in loader logs.

## Out Of Scope

* Do not implement the full PSP structure.
* Do not complete an environment segment selector model.
* Do not modify original game logic.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
