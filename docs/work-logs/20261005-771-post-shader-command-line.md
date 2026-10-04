# Task 771 작업 로그: shader 커맨드라인 옵션

설계: [20261005-771](../design/20261005-771-post-shader-command-line.md) ·
작업 지시: [20261005-771](../work-orders/20261005-771-post-shader-command-line.md)

## 바꾼 것

* `include/repiu/launcher/command_line_options.h`, `src/launcher/command_line_options.cpp`: `ParseCommandLineOptions`.
  `--post-shader <id>`·`--post-shader=<id>`, 마지막 값 우선, `--` 뒤는 옵션 아님, 값 없음·빈 값은 오류, 그 밖의
  인자는 순서대로 positional. `repiu_exe`에 추가.
* `src/host/loader/main.cpp`: `main` 첫머리에서 해석해 오류면 exit 1, 옵션이 있으면 `REPIU_POST_SHADER`를 덮어써
  게시하고 `Command line post shader: <id>`를 남긴 뒤, argv를 남은 인자로 바꿉니다. 그 아래 코드는 그대로입니다.
* `launcher_probe`: `launcher_command_line_options` 검사(7가지 경우).
* README 사용 예, [후처리 shader 가이드](../guides/post-process-shaders.md)의 고르는 방법 표.

## 검증

* Win32 x86 Debug 전체 빌드 exit 0, 새 경고 없음(기존 `aot_probe/main.cpp`의 C4005만).
* Linux x64 Debug(WSL) 전체 빌드 exit 0.
* `repiu_aot_probe --launcher`(Win32), `repiu_core_probe`(Win32, Linux x64): `launcher_command_line_options=true`,
  `launcher_all=true`, core probe exit 0.
* 실제 실행(Win32 Debug, pumpit1, 8초):

| 명령 | 결과 |
|---|---|
| `repiu pumpit1 --post-shader crt` | `Command line post shader: crt`, `loader target: pumpit1`, `[repiu-post] shader: crt (6 parameters)`, exit 0 |
| `REPIU_POST_SHADER=none` + `repiu --post-shader=scanline pumpit1` | 커맨드라인이 이김: `[repiu-post] shader: scanline (2 parameters)`, exit 0 |
| `repiu pumpit1 --post-shader` | `Command line: --post-shader needs a shader id …`, exit 1 |

## 하지 않은 것

* 롬셋 없이 `repiu --post-shader crt`로 런처를 여는 경로는 실행하지 않았습니다(GUI 조작 필요). 옵션을 뺀 argv의
  argc가 1이 되어 기존 런처 조건을 그대로 타고, 게시한 변수를 자식 프로세스가 물려받는 것은 코드로 확인했습니다.

---

# Task 771 Work Log: A Shader Command-Line Option

Design: [20261005-771](../design/20261005-771-post-shader-command-line.md) ·
Work order: [20261005-771](../work-orders/20261005-771-post-shader-command-line.md)

## Changes

* `include/repiu/launcher/command_line_options.h`, `src/launcher/command_line_options.cpp`:
  `ParseCommandLineOptions`. `--post-shader <id>` and `--post-shader=<id>`, the last value wins, nothing after `--`
  is an option, a missing or empty value is an error, and every other argument stays positional in order. Added to
  `repiu_exe`.
* `src/host/loader/main.cpp`: parsed at the top of `main`; an error exits 1, the option overwrites and publishes
  `REPIU_POST_SHADER` and logs `Command line post shader: <id>`, and argv becomes the remaining arguments. The code
  below is unchanged.
* `launcher_probe`: a `launcher_command_line_options` check (seven cases).
* README usage and the selection table of the [post-processing shader guide](../guides/post-process-shaders.md).

## Verification

* Full Win32 x86 Debug build, exit 0, no new warnings (only the existing C4005 in `aot_probe/main.cpp`).
* Full Linux x64 Debug (WSL) build, exit 0.
* `repiu_aot_probe --launcher` (Win32) and `repiu_core_probe` (Win32 and Linux x64): `launcher_command_line_options=true`,
  `launcher_all=true`, core probe exit 0.
* Real runs (Win32 Debug, pumpit1, 8 s):

| Command | Result |
|---|---|
| `repiu pumpit1 --post-shader crt` | `Command line post shader: crt`, `loader target: pumpit1`, `[repiu-post] shader: crt (6 parameters)`, exit 0 |
| `REPIU_POST_SHADER=none` + `repiu --post-shader=scanline pumpit1` | the command line wins: `[repiu-post] shader: scanline (2 parameters)`, exit 0 |
| `repiu pumpit1 --post-shader` | `Command line: --post-shader needs a shader id …`, exit 1 |

## Not done

* Opening the launcher with `repiu --post-shader crt` and no ROM set was not run (it needs GUI interaction). With the
  option removed, argc is 1 and takes the existing launcher condition, and the child processes inherit the published
  variable; both were checked in the code.
