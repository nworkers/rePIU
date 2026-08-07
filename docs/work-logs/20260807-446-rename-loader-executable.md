# Task 446 작업 로그 — 로더 실행 파일을 `repiu.exe`로, 그리고 세션 인수인계

작업지시: [20260807-446](../work-orders/20260807-446-rename-loader-executable.md)

## 1. 이름 변경

`repiu_loader_win32` → `repiu`. CMake 타깃 이름 자체를 바꿨으므로 산출물도
`repiu.exe`입니다.

| 파일 | 건수 |
|---|---:|
| `CMakeLists.txt` | 3 |
| `src/host/win32/supervisor_main.cpp` | 1 (자식 프로세스 경로) |
| `scripts/*.ps1` | 14 |
| `README.md`·`ARCHITECTURE.md`·`docs/EXE_DESIGN.*` | 9 |
| `docs/guides/*.md` | 13 |
| **합계** | **41** |

**기록 문서는 손대지 않았습니다** — `docs/work-logs/`, `docs/work-orders/`,
`docs/design/`, `docs/analysis/history/`에 남은 옛 이름은 그대로입니다. 그것들은 그때
무엇을 실행했는지의 증거이고, 소급해 바꾸면 증거가 거짓이 됩니다.

## 2. 검증

| 검사 | 결과 |
|---|---|
| Release 빌드 | `repiu.exe` 생성, 오류 0 |
| **옛 바이너리 제거** | 빌드 트리에서 `repiu_loader_win32.exe` 삭제 |
| 기동 | 기본 타깃 `piu_1st` 로드까지 확인 후 종료 |
| `package_release.ps1` 목록 | 산출물과 일치 |

옛 바이너리 삭제는 형식이 아닙니다. 남겨 두면 다음 A/B가 **이름만 다른 두 실행 파일**
중 옛것을 집을 수 있고, 그것이 Task 438의 A/B를 무효로 만든 바로 그 함정입니다.

census가 호스트 사이트를 찍을 때 쓰는 모듈 이름은 실행 중
`GetModuleFileName`으로 얻으므로 자동으로 `repiu.exe`가 됩니다. 하드코딩된 곳은
supervisor의 자식 경로 하나뿐이었습니다.

## 3. 세션 인수인계 (Tasks 435~445)

[current-execution-frontier](../analysis/current-execution-frontier.md)에 2026-08-07
항목을 추가했습니다. 담은 것:

* **이번 세션 요약** — setter 생략(batch 2~4)과 draw batching으로 게이트 비용을 계속
  깎았으나 프레임은 안 움직였고, census가 지목한 **inline cache 패치 왕복**을 없애자
  fps +54.7%.
* **정정 4건** — 440의 축 선택은 vsync ON에서 잰 측정 오류, 437의 `grTexSource` 제외는
  오판, "비용은 사라지지 않고 옮겨간다", 크래시는 추측이 아니라 덤프로.
* **현재 pumpit2 지형** — fps 107.2, census `InvokeOnHostThread` 48.0%,
  `PatchWin32AotIndirectInlineCache` 21.8%, 생략률 89.6%, 평균 배치 32.1.
* **다음 축 6개(우선순위 포함)** — 랑데부 제거(48%), 패치 빈도(frontier 5),
  **시간 프로파일 사각지대(먼저 닫을 것)**, `grFogTable`, LFB 쌍, 발동하지 않는 port I/O
  batching.
* **방법 규칙** — vsync OFF, 성능 캡처에 timeout 금지(감시견), 프레임을 쓸 수 있는
  조건, A/B 전 바이너리 확인.

또한 오래된 "다음 할 일" 절(2026-08-04 기준) 머리에 **현재 순위는 새 인수인계 항목에
있다**는 안내를 달았습니다. 그 절을 지우지 않은 이유는 거기 담긴 닫힌 항목의 근거가
아직 유효하기 때문입니다.

---

# Task 446 Work Log — renaming the loader executable, and the session handoff

## 1. The rename

`repiu_loader_win32` became `repiu` at the CMake target level, so the artifact is
`repiu.exe`. Forty-one references moved: three in `CMakeLists.txt`, one in the supervisor's
child-process path, fourteen in `scripts/`, nine across `README.md`, `ARCHITECTURE.md` and
`docs/EXE_DESIGN.*`, and thirteen in `docs/guides/`.

**The record documents were left alone.** Old names remain in `docs/work-logs/`,
`docs/work-orders/`, `docs/design/` and `docs/analysis/history/`, because those are
evidence of what was run at the time and renaming retroactively would make the evidence
false.

## 2. Verification

The Release build produces `repiu.exe` with no errors, `repiu.exe` starts and loads its
default `piu_1st` target, and `package_release.ps1`'s list matches what is built. The stale
`repiu_loader_win32.exe` was deleted from the build tree — not a formality: leaving it
would let a later A/B pick the older of two similarly named executables, which is precisely
the trap that voided Task 438's comparison.

The module name the census prints for host sites comes from `GetModuleFileName` at runtime
and follows the rename by itself; the supervisor's child path was the only hard-coded
occurrence.

## 3. The session handoff, Tasks 435-445

A 2026-08-07 entry was added to
[current-execution-frontier](../analysis/current-execution-frontier.md). It carries the
session summary — setter elision through batches two to four and draw batching kept cutting
gate cost without moving frames, until removing the inline-cache patch round trip the
census pointed at raised throughput 54.7% — the four corrections this session made, the
current pumpit2 shape, six ordered next axes with the time-profile blind spot marked as the
one to close first, and the method rules.

The older "next work" section, dated 2026-08-04, now opens with a pointer saying the
current ordering lives in the new handoff entry. It was not deleted because the evidence it
records for the items it closed is still valid.
