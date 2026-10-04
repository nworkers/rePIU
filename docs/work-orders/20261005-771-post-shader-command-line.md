# Task 771 작업 지시: shader 커맨드라인 옵션

설계: [20261005-771](../design/20261005-771-post-shader-command-line.md)

## 절차

1. `include/repiu/launcher/command_line_options.h`, `src/launcher/command_line_options.cpp`에
   `ParseCommandLineOptions`를 만들고 `repiu_exe`에 넣습니다.
2. `src/host/loader/main.cpp`: 가장 먼저 해석하고, 오류면 exit 1, `--post-shader`가 있으면 `REPIU_POST_SHADER`로
   게시하고 로그를 남긴 뒤, 남은 인자로 argv를 바꿉니다.
3. launcher probe에 `launcher_command_line_options` 검사를 더합니다.
4. README 사용 예, 후처리 shader 가이드의 고르는 방법 표.
5. Win32 x86 Debug 빌드와 probe, 실제 실행 세 가지, Linux x64 Debug(WSL) 빌드와 core probe.

## 완료 조건

probe가 통과하고, 실제 실행 로그가 설계의 검증 항목과 맞으며, 두 플랫폼에서 빌드됩니다.

---

# Task 771 Work Order: A Shader Command-Line Option

Design: [20261005-771](../design/20261005-771-post-shader-command-line.md)

## Steps

1. Add `ParseCommandLineOptions` in `include/repiu/launcher/command_line_options.h` and
   `src/launcher/command_line_options.cpp`, built into `repiu_exe`.
2. `src/host/loader/main.cpp`: parse first; exit 1 on an error; with `--post-shader`, publish `REPIU_POST_SHADER`
   and log it; then swap argv for the remaining arguments.
3. Add a `launcher_command_line_options` check to the launcher probe.
4. README usage and the selection table of the post-processing shader guide.
5. Win32 x86 Debug build and probes, the three real runs, and the Linux x64 Debug (WSL) build and core probe.

## Done when

The probes pass, the real-run logs match the design's verification items, and both platforms build.
