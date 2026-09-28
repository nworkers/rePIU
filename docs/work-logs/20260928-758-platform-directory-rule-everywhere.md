# Task 758 작업 로그 — 플랫폼·아키텍처 디렉터리 규칙의 전체 적용 (중단, Task 759로 대체)

설계: [20260928-758](../design/20260928-758-platform-directory-rule-everywhere.md)
작업 지시: [20260928-758](../work-orders/20260928-758-platform-directory-rule-everywhere.md)

## 요약

네 단계 가운데 둘을 마친 뒤 사용자가 방향을 확인하도록 했고, 규칙을 바꾸기로 해 이 작업은 여기서 끝났습니다.
이어지는 작업은 [Task 759](20260929-759-execution-model-and-platform-moves.md)입니다.

| 단계 | 커밋 | 결과 |
|---|---|---|
| 선행 | `48ca9e3` | Win32 core probe가 `dos_console_device`에서 abort하던 것을 고침(`CON:` 경로의 `filesystem_error`). 유지 |
| 1. 플랫폼 계층과 컴파일러 shim | `9f4c1b1` | `src/platform/`의 네 파일과 헤더 다섯 개를 OS별로 나눔. CMake에 `REPIU_PLATFORM`, `REPIU_ARCH`. **유지** |
| 2. 엔진 지원 파일 | `a7ad129` | 엔진 안에 `win32/`, `linux/`, `x86/`, `linux/x64/` 디렉터리를 만듦. **Task 759 단계 1에서 되돌림** |
| 3. 엔진 핵심 | — | 하지 않음 |
| 4. 호스트와 도구 | — | 하지 않음 |

## 규칙을 바꾼 까닭

단계 2의 결과를 확인하니 세 가지가 어긋나 있었습니다.

* 아키텍처만 묻는 엔진 코드(`IsDirectX86ExecutionSupported`)가 플랫폼 계층의 헤더(`host_architecture.h`)로
  갔습니다.
* x86 쪽은 `D/x86/`, x64 쪽은 `D/linux/x64/`로 비대칭이었습니다.
* OS에 의존하는 코드가 플랫폼 계층으로 가지 않고 엔진 디렉터리 안의 `win32/`, `linux/`에 남았습니다. `AGENTS.md`의
  기존 규칙("플랫폼 종속 코드는 `src/platform/` 아래")과 달랐고, 작업 지시 1번(규칙 문서 갱신)은 수행되지 않은
  상태였습니다.

플랫폼 계층 밖의 분기 162개를 분류하니 엔진에서 아키텍처가 실제 이유인 것은 실행 방식 하나였고, 아키텍처로
나눈 파일 가운데에는 기능을 켤지 여부와 값 하나가 있었습니다.

## 검증

단계 1과 2의 검증은 각 커밋이 한 것으로, 이 로그를 쓴 세션에서는 다시 하지 않았습니다. Task 759의 기준선
(커밋 `a7ad129`)을 모을 때 네 구성(Linux x64 Release·Debug, Linux i386, Win32 x86 Debug)이 빌드되고 core probe가
통과하는 것은 확인했습니다.

---

# English

# Task 758 work log — the platform and architecture directory rule everywhere (stopped, replaced by Task 759)

Design: [20260928-758](../design/20260928-758-platform-directory-rule-everywhere.md)
Work order: [20260928-758](../work-orders/20260928-758-platform-directory-rule-everywhere.md)

## Summary

After two of its four phases the user asked for the direction to be checked, the rule was changed, and
this task ended there. The work continues as [Task 759](20260929-759-execution-model-and-platform-moves.md).

| Phase | Commit | Outcome |
|---|---|---|
| Prerequisite | `48ca9e3` | The Win32 core probe no longer aborts in `dos_console_device` (a `filesystem_error` on the `CON:` path). Kept |
| 1. The platform layer and compiler shims | `9f4c1b1` | Four files of `src/platform/` and five headers split by OS; `REPIU_PLATFORM` and `REPIU_ARCH` in CMake. **Kept** |
| 2. Engine support files | `a7ad129` | Made `win32/`, `linux/`, `x86/` and `linux/x64/` directories inside the engine. **Undone by Task 759's phase 1** |
| 3. Engine core | — | not done |
| 4. Hosts and tools | — | not done |

## Why the rule changed

Checking phase 2's result showed three things out of line.

* Engine code that asks about the architecture alone (`IsDirectX86ExecutionSupported`) went to a header of
  the platform layer (`host_architecture.h`).
* The x86 side went to `D/x86/` and the x64 side to `D/linux/x64/`, which is not symmetric.
* Code that depends on the OS did not go to the platform layer; it stayed inside engine directories, in
  `win32/` and `linux/`. That differed from the rule `AGENTS.md` already had ("platform-specific code
  under `src/platform/`"), and step 1 of the work order (updating the rule documents) had not been done.

Classifying the 162 branches outside the platform layer showed that in the engine the architecture is
the real reason for one thing, how the guest is run, and that the files split by architecture included
whether a feature is on and a single value.

## Verification

Phases 1 and 2 were verified by their own commits, not again in the session that wrote this log. When
Task 759's baseline was collected (commit `a7ad129`), the four configurations (Linux x64 Release and
Debug, Linux i386, Win32 x86 Debug) built and their core probes passed.
