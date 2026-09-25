# Task 731 작업 로그 — 공용 로더 entry point 이동

설계: [20260922-731](../design/20260922-731-shared-loader-entry-location.md)
작업 지시: [20260922-731](../work-orders/20260922-731-shared-loader-entry-location.md)

## 결과

`src/host/win32/main.cpp`를 `src/host/loader/main.cpp`로 `git mv`했습니다. git은 이를 **R100**,
즉 내용이 바이트 단위로 같은 rename으로 판정합니다. Linux와 Win32의 두 `repiu` target은 새 경로로
빌드됩니다. `src/host/win32/`에는 Win32 전용 `supervisor_main.cpp`만, `src/host/linux/`에는 Linux
런처만 남습니다. 새 위치의 깊이가 같아 `../../engine/...` 상대 include는 수정 없이 유효합니다.

`CMakeLists.txt`의 Task 503d-17 주석, `ARCHITECTURE.md`의 디렉터리 설명과 entry point 언급,
`README.md`의 디렉터리 표를 갱신했습니다.

## 로그 접두어는 그대로 두었다

파일에는 `"Win32 ..."`로 시작하는 로그 문자열이 646개 있습니다. `test_all.ps1`을 포함한 스크립트
20개 이상, 가이드 7개, `tests/history/`의 회귀 기록이 이 문자열을 파싱합니다. 바꾸면 이들이 한꺼번에
깨지고 과거 기록과의 비교도 끊기므로 이 작업에서는 건드리지 않았습니다. 정리할지는 별도 결정입니다.

## 검증

- Win32 x86 Debug 전체 빌드 성공(`../src/host/loader/main.cpp` 컴파일 확인), core probe 29/29.
- WSL Linux x64 Debug 빌드 성공(새 경로 컴파일 확인), core probe 31/31.
- WSL `pumpit2a` 15초 실행: 1,952 프레임 present, fault 0, immediate-exit 경로로 summary 출력.
- `git diff --cached -M`: `R100 src/host/win32/main.cpp → src/host/loader/main.cpp`.

## 남은 것

- `"Win32 ..."` 로그 접두어 정리 여부(호환 유지 대 전면 교체).
- `src/host/linux/main.cpp`(Linux 런처)의 머리 주석은 "Linux에는 아직 실행 엔진이 없다"고 적혀 있고
  선택한 롬셋을 실제로 시작하지 않습니다. Task 503d-17 이후로는 사실과 맞지 않으므로 별도 확인이
  필요합니다.

---

# English

# Task 731 work log — moving the shared loader entry point

Design: [20260922-731](../design/20260922-731-shared-loader-entry-location.md)
Work order: [20260922-731](../work-orders/20260922-731-shared-loader-entry-location.md)

## Result

`src/host/win32/main.cpp` was `git mv`ed to `src/host/loader/main.cpp`, which git reports as **R100**,
a rename with byte-identical contents. Both `repiu` targets, Linux and Win32, now build from the new
path. `src/host/win32/` keeps only the Win32-only `supervisor_main.cpp`, and `src/host/linux/` only
the Linux launcher. The new location has the same depth, so the `../../engine/...` relative includes
stay valid unchanged.

The Task 503d-17 comment in `CMakeLists.txt`, the directory description and entry-point mentions in
`ARCHITECTURE.md`, and the directory table in `README.md` were updated.

## The log prefix was left alone

The file has 646 log strings beginning with `"Win32 ..."`, parsed by more than 20 scripts including
`test_all.ps1`, seven guides, and the regression records in `tests/history/`. Changing them would
break all of those at once and sever comparison with past records, so this task did not touch them.
Whether to clean them up is a separate decision.

## Verification

- Full Win32 x86 Debug build succeeded (compiling `../src/host/loader/main.cpp`); core probe 29/29.
- WSL Linux x64 Debug build succeeded (compiling the new path); core probe 31/31.
- A 15-second WSL `pumpit2a` run presented 1,952 frames with zero faults and reported its summary
  through the immediate-exit path.
- `git diff --cached -M`: `R100 src/host/win32/main.cpp -> src/host/loader/main.cpp`.

## What remains

- Whether to clean up the `"Win32 ..."` log prefix (keep compatibility versus replace throughout).
- The header comment of `src/host/linux/main.cpp` (the Linux launcher) says Linux has no execution
  engine yet, and the launcher does not actually start the selected ROM set. Since Task 503d-17 that
  no longer matches the facts and needs a separate look.
