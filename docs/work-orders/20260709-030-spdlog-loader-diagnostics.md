# spdlog loader 진단 출력 분리 작업 지시

## 목표

Win32 loader의 진단 정보와 guest executable의 실제 출력을 구분한다.

## 범위

1. CMake에 `spdlog` 의존성을 추가한다.
2. Win32 loader의 진단 출력 함수를 명시적인 `logger.info(...)` / `logger.error(...)` 호출로 변경한다.
3. guest HLE console output은 stdout에 prefix 없이 출력한다.
4. 의존성 라이선스와 검증 절차를 문서에 남긴다.
5. Win32 x86 loader와 `dos4gw_hello` 경로를 검증한다.
6. 작업 로그를 작성한다.

## 제외

* `repiu_exe_analyzer` 출력 포맷은 이번 작업에서 변경하지 않는다.
* guest 출력 내용을 spdlog로 감싸지 않는다.
* 로그 파일 sink는 이번 작업에서 추가하지 않는다.

# spdlog Loader Diagnostic Output Separation Work Order

## Goal

Separate Win32 loader diagnostics from real guest executable output.

## Scope

1. Add the `spdlog` dependency to CMake.
2. Convert Win32 loader diagnostic printing helpers to explicit `logger.info(...)` / `logger.error(...)` calls.
3. Keep guest HLE console output unprefixed on stdout.
4. Document dependency licensing and verification.
5. Verify the Win32 x86 loader and the `dos4gw_hello` path.
6. Write the work log.

## Out Of Scope

* Do not change the `repiu_exe_analyzer` output format in this task.
* Do not wrap guest output with spdlog.
* Do not add a file sink in this task.
