# Task 757: Linux 플랫폼 공용·x86·x64 디렉터리 분리 작업 지시

설계: [20260928-757](../design/20260928-757-linux-platform-arch-directories.md)

## 한국어

1. 변경 전 기준: 현재 트리로 Linux i386·x64 Debug를 빌드하고 `repiu_core_probe` 출력을 저장한다.
2. `git mv`로 i386 전용 3개를 `src/platform/linux/x86/`, x64 전용 5개를 `src/platform/linux/x64/`로 옮기고,
   `stack_bridge_probe.S`의 include 경로를 고친다.
3. `guest_cpu_context.cpp`를 공용(`ReadGuestFaultInfo`)과 `x86/`·`x64/`의 `guest_cpu_context.cpp`로 나눈다.
4. `fault_handler.cpp`에서 `fault_report_writer.h/.cpp`와 `fault_handler_arch.h`를 뽑고, `x86/`·`x64/`에
   `fault_handler_arch.cpp`를 만든다. 보고 줄의 필드 순서를 유지한다.
5. 아키텍처 파일 첫머리에 `#error` 가드를 둔다.
6. `CMakeLists.txt`의 Linux 소스 목록을 공용·x86·x64로 다시 묶는다.
7. `AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`, `src/platform/web/README.md`, 분석 문서 2개,
   엔진 주석 5곳의 경로를 갱신한다.
8. 두 구성을 다시 빌드해 probe 출력을 기준과 비교하고, `#error` 가드와 Win32 configure를 확인한 뒤 작업
   로그를 쓰고 커밋한다.
9. (결정 4) `include/repiu/platform/linux_x64_*.h` 4개를 `include/repiu/platform/linux/x64/`로 `git mv`하고
   include 줄과 문서의 경로를 고친 뒤, Linux i386·x64와 Win32 Debug를 다시 빌드하고 probe를 비교한다.

## English

1. Baseline: build Linux i386 and x64 Debug from the current tree and save the `repiu_core_probe` output.
2. `git mv` the three i386-only files to `src/platform/linux/x86/` and the five x64-only files to
   `src/platform/linux/x64/`, and fix the include path in `stack_bridge_probe.S`.
3. Split `guest_cpu_context.cpp` into the shared part (`ReadGuestFaultInfo`) and `guest_cpu_context.cpp` in
   `x86/` and `x64/`.
4. Extract `fault_report_writer.h/.cpp` and `fault_handler_arch.h` from `fault_handler.cpp`, and add
   `fault_handler_arch.cpp` to `x86/` and `x64/`. Keep the field order of the report line.
5. Put an `#error` guard at the top of each architecture file.
6. Regroup the Linux source lists in `CMakeLists.txt` into shared, x86 and x64.
7. Update paths in `AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`, `src/platform/web/README.md`,
   two analysis documents and five engine comments.
8. Rebuild both configurations and compare probe output with the baseline, check the `#error` guard and the
   Win32 configure, write the work log and commit.
9. (Decision 4) `git mv` the four `include/repiu/platform/linux_x64_*.h` headers to
   `include/repiu/platform/linux/x64/`, fix the include lines and document paths, then rebuild Linux i386,
   x64 and Win32 Debug and compare the probes.
