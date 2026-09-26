# Task 739: Linux 빌드 스크립트 정비 작업 지시

설계: [20260926-739](../design/20260926-739-linux-build-scripts-manual-use.md)

## 한국어

1. `repiu_exe`가 Linux에서 `GL`을 `PUBLIC`으로 링크하게 해 `repiu_instruction_census`를 포함한 기본
   타깃 전체가 링크되게 한다.
2. 두 스크립트가 `SDL_UNIX_CONSOLE_BUILD`를 항상 명시(`--headless`면 ON, 아니면 OFF)하고, usage와
   주석이 스위치의 실제 의미를 말하게 한다.
3. i386 스크립트에 `--build-dir`과 구성별 디렉터리 안내를 넣고 머리말을 현재 상태로 고친다.
4. README와 guides의 `--headless` 서술을 실제 의미에 맞춘다.
5. x64 Debug 전체 빌드, i386 Release 스크립트 빌드와 core probe, x64 Release 스크립트 재구성으로
   검증한다.
6. Task 738 작업 로그의 "headless 트리" 서술을 정정하고 작업 로그를 남긴 뒤 커밋한다.

## English

1. Have `repiu_exe` link `GL` `PUBLIC` on Linux so every default target, `repiu_instruction_census`
   included, links.
2. Make both scripts always pass `SDL_UNIX_CONSOLE_BUILD` (`ON` with `--headless`, `OFF` otherwise)
   and have their usage and comments state what the switch really does.
3. Add `--build-dir` and the one-configuration-per-directory guidance to the i386 script and bring its
   header up to date.
4. Match README's and the guides' `--headless` wording to the real meaning.
5. Verify with a full x64 Debug build, an i386 Release build through the script with its core probe,
   and a reconfiguration of the x64 Release tree through the script.
6. Correct the "headless tree" statement in the Task 738 work log, write the work log, commit.
