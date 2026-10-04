# Task 771 설계: shader 커맨드라인 옵션

작업 지시: [20261005-771](../work-orders/20261005-771-post-shader-command-line.md) ·
작업 로그: [20261005-771](../work-logs/20261005-771-post-shader-command-line.md) ·
선행: [20261004-768](20261004-768-post-process-shaders.md)

## 배경

Task 768의 shader는 런처, 환경 변수 `REPIU_POST_SHADER`, 게임 중 `Tab` OSD에서 고릅니다. 실행 인자는 롬셋
이름(또는 실행 파일 경로) 하나뿐이라, 사용자가 커맨드라인 옵션을 요청했습니다(2026-10-05).

## 결정

* **형식:** `--post-shader <id>`와 `--post-shader=<id>`. id는 `REPIU_POST_SHADER`와 같습니다(`none`, `crt`,
  `scanline`, `shaders\` 안 파일 이름). 롬셋 앞뒤 어디에 와도 됩니다.
  ```text
  repiu pumpit8 --post-shader crt
  repiu --post-shader=scanline pumpitea
  repiu --post-shader crt            (런처가 열리고, 그 세션의 게임에 적용)
  ```
* **전달:** 옵션을 읽어 `REPIU_POST_SHADER`로 게시하고, argv에서 지운 뒤 기존 경로에 넘깁니다. 엔진, 런처, 롬셋
  선택(`argv[1]`) 코드는 바뀌지 않습니다. Task 766·768과 같은 "환경 변수로 게시" 방식입니다.
* **우선순위:** 커맨드라인 > 환경 변수 > `cfg\repiu.ini`. 커맨드라인은 이번 실행에 대한 가장 직접적인 지시이므로
  호출자의 환경 변수도 덮어씁니다. 게시 뒤에는 기존 규칙대로 "환경 변수가 있으면 파일 값은 게시하지 않음"이
  적용되므로 파일보다도 이깁니다. 런처를 거치면 자식 프로세스가 같은 환경을 물려받으므로 그 세션 내내 유지됩니다.
* **오류:** 값이 없거나(`--post-shader`가 마지막 인자) 비어 있으면(`--post-shader=`) 로그에 이유를 남기고 exit 1.
  알 수 없는 id는 지금처럼 엔진이 경고를 남기고 `none`으로 실행합니다(fail-closed, 검사 위치를 하나로 유지).
  같은 옵션이 여러 번 오면 마지막 값을 씁니다. `--` 뒤의 인자는 옵션으로 해석하지 않습니다.
* **다른 `--` 인자:** 건드리지 않고 그대로 넘깁니다. 지금까지 동작을 바꾸지 않기 위해서입니다.
* **코드 위치:** 해석은 GL·OS와 무관한 순수 함수 `ParseCommandLineOptions`
  (`include/repiu/launcher/command_line_options.h`, `src/launcher/command_line_options.cpp`)로 두고, 모든 플랫폼에서
  빌드되는 `repiu_core_probe`의 launcher probe로 검사합니다. `main`은 호출, 게시, argv 교체만 합니다.

## 검증

* launcher probe에 `launcher_command_line_options` 추가: 두 형식, 롬셋 앞뒤 위치, 반복 시 마지막 값, `--` 종료,
  값 없음·빈 값 오류, 다른 인자 보존 순서.
* 실제 실행: `repiu pumpit1 --post-shader crt`와 `repiu --post-shader=scanline pumpit1`에서
  `[repiu-post] shader:` 줄이 각 id를 말하고, 환경 변수 `REPIU_POST_SHADER=none`이 있어도 커맨드라인이 이기며,
  `--post-shader`만 주면 exit 1.
* Win32 x86 Debug와 Linux x64 Debug(WSL)에서 빌드와 core probe.

---

# Task 771 Design: A Shader Command-Line Option

Work order: [20261005-771](../work-orders/20261005-771-post-shader-command-line.md) ·
Work log: [20261005-771](../work-logs/20261005-771-post-shader-command-line.md) ·
Builds on: [20261004-768](20261004-768-post-process-shaders.md)

## Background

Task 768's shaders are chosen in the launcher, with the `REPIU_POST_SHADER` environment variable, or from the
in-game `Tab` OSD. The only argument is the ROM set name (or an executable path), and the user asked for a
command-line option (2026-10-05).

## Decisions

* **Form:** `--post-shader <id>` and `--post-shader=<id>`, with the ids of `REPIU_POST_SHADER` (`none`, `crt`,
  `scanline`, a file name in `shaders\`), before or after the ROM set.
  ```text
  repiu pumpit8 --post-shader crt
  repiu --post-shader=scanline pumpitea
  repiu --post-shader crt            (opens the launcher; applies to that session's games)
  ```
* **Delivery:** the option is read, published as `REPIU_POST_SHADER`, removed from argv, and the rest goes down
  the existing path; the engine, the launcher and the ROM set selection (`argv[1]`) are untouched. It is the
  "publish as an environment variable" approach of Tasks 766 and 768.
* **Precedence:** command line > environment variable > `cfg\repiu.ini`. The command line is the most direct
  instruction for this run, so it overwrites the caller's variable too; once published, the existing rule (a
  variable present means the file's value is not published) makes it beat the file as well. Through the launcher
  the child process inherits the same environment, so it holds for the whole session.
* **Errors:** a missing value (`--post-shader` as the last argument) or an empty one (`--post-shader=`) logs the
  reason and exits 1. An unknown id is left to the engine, which warns and runs `none` as today (fail-closed, one
  place that checks). A repeated option takes the last value. Arguments after `--` are never read as options.
* **Other `--` arguments:** passed through untouched, so nothing that worked before changes.
* **Where the code lives:** parsing is a pure function free of GL and OS, `ParseCommandLineOptions`
  (`include/repiu/launcher/command_line_options.h`, `src/launcher/command_line_options.cpp`), checked by the
  launcher probe in `repiu_core_probe`, which builds on every platform. `main` only calls it, publishes and swaps
  argv.

## Verification

* A `launcher_command_line_options` check in the launcher probe: both forms, before and after the ROM set, the last
  of a repeated option, `--` ending options, missing and empty values as errors, and other arguments kept in order.
* Real runs: `repiu pumpit1 --post-shader crt` and `repiu --post-shader=scanline pumpit1` print the id on the
  `[repiu-post] shader:` line, the command line wins over `REPIU_POST_SHADER=none` in the environment, and
  `--post-shader` alone exits 1.
* Build and core probe on Win32 x86 Debug and Linux x64 Debug (WSL).
