# Win32 로더 앱 구조 정리 설계

## 요구사항

현재 실제 로더 진입점이 `src/tools/win32_execution_host/main.cpp` 아래에 있어 분석 도구처럼 보인다.

이 파일을 Win32 host 애플리케이션 구조인 `src/host/win32/main.cpp`로 이동하고, CMake executable target 이름을 `repiu_loader_win32`로 변경한다.

## 설계 결정

`src/tools/`는 실행 파일 분석, 덤프, 검증처럼 보조 목적의 도구를 둔다.

원본 DOS/4GW executable을 읽고, relocated image를 만들고, Win32 process memory에 배치한 뒤 최소 실행 trampoline까지 호출하는 진입점은 보조 도구가 아니라 실제 로더 앱의 시작점이다.

따라서 Win32 전용 로더 앱 진입점은 `src/host/win32/main.cpp`에 둔다.

## CMake 변경 방향

기존 `repiu_win32_execution_host` executable target은 `repiu_loader_win32`로 이름을 바꾼다.

Win32 x86 image base 정책 함수도 execution host 임시 용어 대신 loader host 용어를 사용하도록 `repiu_configure_win32_loader_host`로 바꾼다.

## 범위

이번 단계는 경로와 target 이름 정리만 수행한다.

로더 동작 순서, relocation 정책, minimal execution trampoline 동작은 바꾸지 않는다.

## 검증

* Win32 x86 빌드가 성공해야 한다.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe`가 실행되어 기존 minimal execution attempt 결과를 출력해야 한다.
* x64 Debug 빌드가 성공해야 한다.
* `build\vs2022_debug\Debug\repiu_loader_win32.exe`는 direct x86 execution unsupported 경로를 안전하게 출력해야 한다.

# Win32 Loader App Layout Design

## Requirement

The current practical loader entry point lives under `src/tools/win32_execution_host/main.cpp`, which makes it look like an analysis tool.

Move it to the Win32 host application structure at `src/host/win32/main.cpp`, and rename the CMake executable target to `repiu_loader_win32`.

## Design Decision

`src/tools/` is reserved for auxiliary tools such as executable analysis, dumps, and verification.

The entry point that reads the original DOS/4GW executable, builds the relocated image, places it in Win32 process memory, and calls the minimal execution trampoline is not an auxiliary tool. It is the start of the real loader application.

Therefore, the Win32-specific loader application entry point belongs in `src/host/win32/main.cpp`.

## CMake Direction

Rename the existing `repiu_win32_execution_host` executable target to `repiu_loader_win32`.

Also rename the Win32 x86 image-base policy helper from the temporary execution-host wording to `repiu_configure_win32_loader_host`.

## Scope

This step only reorganizes the path and target name.

It does not change loader flow, relocation policy, or minimal execution trampoline behavior.

## Verification

* The Win32 x86 build must pass.
* `build\vs2022_win32_debug\Debug\repiu_loader_win32.exe` must run and print the existing minimal execution attempt result.
* The x64 Debug build must pass.
* `build\vs2022_debug\Debug\repiu_loader_win32.exe` must safely print the direct x86 execution unsupported path.
