# Task 446 작업 지시 — 로더 실행 파일을 `repiu.exe`로

## 1. 목적

`repiu_loader_win32.exe`는 이 프로젝트가 dpmi 실험 트리와 여러 호스트 후보를 함께
들고 있던 시절의 이름입니다. 지금은 호스트가 하나뿐이고 이것이 **사용자가 실행하는
그 프로그램**이므로, 이름이 그 사실을 말해야 합니다. `_win32` 접미사는 배포판에서
중복입니다 — 배포되는 것이 Win32 빌드뿐입니다.

## 2. 범위

| 대상 | 처리 |
|---|---|
| `CMakeLists.txt`의 타깃 이름 | `repiu_loader_win32` → `repiu` |
| `src/host/win32/supervisor_main.cpp` | 자식 프로세스 경로 문자열 |
| `scripts/*.ps1` | 실행 경로 |
| `README.md`·`ARCHITECTURE.md`·`docs/EXE_DESIGN.*` | 사용법 |
| `docs/guides/*.md` | 지금 따라 하는 절차이므로 갱신 |
| **`docs/work-logs/`·`docs/work-orders/`·`docs/design/`·`docs/analysis/history/`** | **손대지 않습니다** |

마지막 줄이 규칙입니다. 그 문서들은 **그때 무엇을 실행했는지의 기록**이고, 이름을
소급해 바꾸면 기록이 거짓이 됩니다.

## 3. 검증

1. Release 빌드에 `repiu.exe`가 나오고 `repiu_loader_win32.exe`는 나오지 않을 것.
2. **빌드 트리에 남은 옛 바이너리를 지울 것.** 남겨 두면 Task 438에서 A/B를 무효로
   만든 그 함정(오래된 바이너리 실행)을 그대로 재현합니다.
3. `repiu.exe`가 기동해 기본 타깃을 로드할 것.
4. `scripts/package_release.ps1`의 목록과 실제 산출물이 일치할 것.

## 4. 부수 작업 — 세션 인수인계 문서화

같은 커밋에서 [current-execution-frontier](../analysis/current-execution-frontier.md)에
2026-08-07 세션(Tasks 435~445) 인수인계 항목을 추가합니다. 다음 세션이 이어서 판단할
수 있도록 **현재 지형·다음 축 우선순위·이번 세션이 정정한 것·방법 규칙**을 담습니다.

---

# Task 446 Work Order — renaming the loader executable to `repiu.exe`

## Purpose

`repiu_loader_win32.exe` is a name from when the project carried a dpmi experiment tree and
several host candidates at once. There is one host now, and it is **the program the user
runs**, so the name should say so; the `_win32` suffix is redundant in a distribution that
ships only the Win32 build.

## Scope

Rename the CMake target, the supervisor's child-process path, the scripts, and the living
documentation: `README.md`, `ARCHITECTURE.md`, `docs/EXE_DESIGN.*` and `docs/guides/`.
**Leave `docs/work-logs/`, `docs/work-orders/`, `docs/design/` and
`docs/analysis/history/` alone** — they record what was run at the time, and renaming
retroactively would make the record false.

## Verification

The Release build must produce `repiu.exe` and not `repiu_loader_win32.exe`; the stale
binary must be deleted from the build tree, because leaving it reproduces exactly the trap
that voided Task 438's A/B — running an old binary; `repiu.exe` must start and load its
default target; and `scripts/package_release.ps1`'s list must match what is built.

## Side task

The same commit adds the 2026-08-07 session handoff (Tasks 435-445) to
[current-execution-frontier](../analysis/current-execution-frontier.md): the current shape,
the ordered next axes, what this session corrected, and the method rules.
