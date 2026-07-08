# spdlog loader 진단 출력 분리 작업 로그

## 결과

Win32 loader의 진단 출력 경로를 `spdlog`로 분리했다.
loader 진단 함수와 main 오류 경로는 코드에서 명확하게 `logger.info(...)` 또는 `logger.error(...)`를 호출한다.
이전 stream buffer redirect 방식은 제거했다.

guest executable의 HLE console output은 `std::cout`을 거치지 않고 stdout에 직접 쓴다.
따라서 샘플 executable이 출력한 문자열과 loader 내부 정보가 기본 stream에서 섞이지 않는다.

`spdlog`는 CMake `find_package`를 먼저 시도하고, 없으면 `FetchContent`로 `v1.14.1`을 받아 사용한다.
라이선스는 MIT이며 GPL/LGPL/AGPL 계열이 아니므로 프로젝트 의존성 정책과 충돌하지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 성공
  * `spdlog` 외부 헤더에서 MSVC C4819 경고가 있었지만 빌드는 성공했다.
* `cmd /c "build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello > build\loader_hello_stdout_cmd.txt 2> build\loader_hello_stderr_cmd.txt"`: 성공
  * stdout: `Hello, world!`
  * stderr: `[info] [loader] ...` loader 로그
* `cmd /c "build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st > build\loader_piu_stdout_cmd.txt 2> build\loader_piu_stderr_cmd.txt"`: 성공
  * stdout line count: 0
  * stderr: 기존 `0x020F3890` / `STI` / `HLE trap candidate` 관찰 유지

## 다음 단계

다음 작업에서는 `STI` HLE trap 후보를 실제 dispatcher handler 호출/복귀 규약에 연결한다.
분석 도구 `repiu_exe_analyzer`의 출력도 장기적으로는 spdlog 또는 structured output으로 분리할 수 있지만, 이번 작업에서는 loader와 guest output 분리를 우선 완료했다.

# spdlog Loader Diagnostic Output Separation Work Log

## Result

Separated Win32 loader diagnostics through `spdlog`.
Loader diagnostic helpers and main error paths now call `logger.info(...)` or `logger.error(...)` explicitly.
The previous stream-buffer redirection approach was removed.

Guest executable HLE console output bypasses `std::cout` and is written directly to stdout.
This keeps sample executable strings separate from loader internal information by default.

CMake first tries `find_package(spdlog)`, then falls back to `FetchContent` for pinned `v1.14.1`.
The license is MIT, not GPL/LGPL/AGPL-family software, so it fits the project dependency policy.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: passed
  * MSVC emitted C4819 warnings from external `spdlog` headers, but the build passed.
* `cmd /c "build\vs2022_win32_debug\Debug\repiu_loader_win32.exe dos4gw_hello > build\loader_hello_stdout_cmd.txt 2> build\loader_hello_stderr_cmd.txt"`: passed
  * stdout: `Hello, world!`
  * stderr: `[info] [loader] ...` loader logs
* `cmd /c "build\vs2022_win32_debug\Debug\repiu_loader_win32.exe piu_1st > build\loader_piu_stdout_cmd.txt 2> build\loader_piu_stderr_cmd.txt"`: passed
  * stdout line count: 0
  * stderr preserved the existing `0x020F3890` / `STI` / `HLE trap candidate` observation

## Next Step

The next task is to connect the `STI` HLE trap candidate to the real dispatcher handler call/return convention.
The analyzer output can later move to spdlog or structured output as well, but this task prioritizes separating loader diagnostics from guest output.
