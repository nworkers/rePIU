# Task 732 설계: 로더 로그의 `Win32` 접두어 제거

## 한국어

### 배경

Task 731이 공용 로더를 `src/host/loader/main.cpp`로 옮겼지만, 로그 문자열 646개는 여전히
`"Win32 ..."`로 시작합니다. Linux x64에서 찍힌 줄도 `[loader] Win32 minimal execution timed out`처럼
읽혀 플랫폼을 잘못 말합니다. 사용자는 소비자까지 전면 교체하는 안을 택했고, 새 접두어로는
**제거**를 택했습니다. 모든 줄이 이미 `[loader]` 로거 태그를 달고 있기 때문입니다.

### 설계

* 로더: 문자열 리터럴 **시작**의 `"Win32 `만 `"`로 바꿉니다. 리터럴 중간의 `Win32`와 주석은 그대로
  둡니다. 실제 플랫폼을 말하는 것이기 때문입니다.
* 소비자: 무조건 치환하지 않습니다. `Win32 Release`(빌드 구성), 스크립트 자체의
  `Win32 supervisor was not found` 메시지, 본문의 `Win32 host` 같은 표현은 플랫폼을 말하는 것이므로
  남겨야 합니다. 그래서 **로더의 실제 646개 리터럴에서 판정 목록을 만듭니다.** `Win32 ` 뒤의 처음 두
  단어가 어떤 로더 리터럴의 시작과 일치할 때만 치환하고, `Win32 (a|b)` 같은 정규식 교대는 모든
  대안이 로더 줄일 때만 치환합니다. Markdown은 코드 span과 fenced block 안에서만 치환합니다.
* 범위: `scripts/`, `docs/guides/`, `.github/`, `ARCHITECTURE.md`, `README.md`.
* 바꾸지 않는 것: `tests/history/`, `docs/work-logs/`, `docs/design/`, `docs/work-orders/`,
  `docs/analysis/`. 당시 출력을 인용한 **기록**이기 때문입니다. `exe_analyzer`의 `Win32` 출력 16개는
  별도 프로그램이고 실제로 Win32 메모리 정책을 말하므로 범위 밖입니다.

### 충돌 검사

접두어가 사라지면 소비자 패턴이 다른 줄까지 새로 잡을 수 있습니다. 기존 로그(Win32 1개, Linux 2개)에서
치환 대상 패턴 746개 각각에 대해 "원래 로그에서 `Win32 ` + 패턴이 잡은 줄 수"와 "접두어를 뗀 로그에서
패턴이 잡은 줄 수"를 비교합니다. 소비자에서 온 패턴 중 달라지는 것이 없어야 합니다.

### 검증

* Win32 x86 전체 빌드와 core probe, WSL Linux x64 빌드·core probe·실행.
* `test_all.ps1`의 pumpit1 단정 99개를 이름 변경 전후 binary에 각각 적용해 **같은 결과**인지 비교합니다.
  동작이 아니라 문자열만 바뀌었으므로, 결과가 달라진다면 소비자 치환이 틀린 것입니다.

---

## English

### Background

Task 731 moved the shared loader to `src/host/loader/main.cpp`, but its 646 log strings still begin
with `"Win32 ..."`, so a line printed on Linux x64 reads `[loader] Win32 minimal execution timed out`
and names the wrong platform. The user chose to replace consumers throughout, and chose
**removal** as the new prefix, since every line already carries the `[loader]` logger tag.

### Design

* Loader: replace only `"Win32 ` at the **start** of a string literal with `"`. `Win32` inside a
  literal and in comments stays, because there it names the actual platform.
* Consumers: not a blanket substitution. `Win32 Release` (a build configuration), a script's own
  `Win32 supervisor was not found`, and `Win32 host` in prose name the platform and must remain.
  **The decision list is built from the loader's actual 646 literals:** an occurrence is rewritten
  only when the first two words after `Win32 ` begin one of those literals, and a regex alternation
  like `Win32 (a|b)` only when every alternative does. Markdown is rewritten only inside code spans
  and fenced blocks.
* Scope: `scripts/`, `docs/guides/`, `.github/`, `ARCHITECTURE.md`, `README.md`.
* Unchanged: `tests/history/`, `docs/work-logs/`, `docs/design/`, `docs/work-orders/` and
  `docs/analysis/`, which are **records** quoting the output of their time. The 16 `Win32` outputs of
  `exe_analyzer` belong to a separate program and really do describe the Win32 memory policy, so they
  are out of scope.

### Collision check

Without the prefix a consumer pattern could newly match other lines. Using existing logs (one Win32,
two Linux), each of the 746 affected patterns is counted as "lines matched by `Win32 ` + pattern in the
original log" against "lines matched by the pattern in the log with the prefix stripped". No pattern
that comes from a consumer may differ.

### Verification

* Full Win32 x86 build and core probe; WSL Linux x64 build, core probe and a run.
* Apply the 99 pumpit1 assertions of `test_all.ps1` to the binaries before and after the rename and
  compare that the **results are identical**. Only strings changed, not behavior, so a difference
  would mean a consumer rewrite is wrong.
