# Task 427 작업 지시 — 호출자 없는 실행 진입점 2개 삭제

선행 조사: [Task 426](20260805-426-backend-consolidation-residue.md)

**이 작업은 backend 축소(Tasks 424~426)와 무관합니다.** 그 조사 중에 발견된, 이미
오래전부터 죽어 있던 코드입니다.

## 1. 대상

`include/repiu/platform/win32/execution_trampoline.h`에 선언되고
`src/platform/win32/execution/execution_trampoline.cpp`에 정의된 두 함수는
**선언과 정의 외에 어떤 참조도 없습니다** — `src`, `include`, `tests`, `tools`
전체에서 확인했습니다.

| 함수 | 성격 |
|---|---|
| `AttemptWin32MinimalExecution` | `RunWin32ExecutionThread`에 모든 기능 플래그를 끄고 넘기는 래퍼 |
| `AttemptWin32GuestStackExecution` | 위에 guest stack switch만 더한 래퍼 |

둘 다 `aot_placement = nullptr`과 `ExecutionBackend::kLegacy`를 하드코딩합니다.

## 2. 언제부터 죽었는가

`git log -S`로 확인한 도입 시점은 `58db6f2 Add minimal Win32 loader execution path`와
`baa89f4 Add Win32 guest stack trampoline`이고, 마지막 변경은 `d1673e2`(Task 233,
v0.0.60)의 모듈 분해입니다. **초기 bring-up 진입점이 상위 경로가 발전하면서 호출자를
잃은 것**이며, Tasks 424~426이 만든 죽은 코드가 아닙니다.

## 3. 변경

두 함수의 선언과 정의를 제거합니다. 그 외에는 아무것도 건드리지 않습니다.

`main.cpp`가 쓰는 세 진입점(`AttemptWin32GuestStackAotExecution`,
`AttemptWin32GuestStackHleExecution`, `AttemptWin32GuestStackTrapExecution`)과
공용 `RunWin32ExecutionThread`는 그대로입니다.

## 4. 검증

| # | 확인 | 통과 조건 |
|---:|---|---|
| 1 | Release 빌드 | exit 0 — 링크 오류가 없으면 호출자 부재가 컴파일러로 재확인됩니다 |
| 2 | `aot_probe` 전체 | 전 항목 통과 |
| 3 | `dynamic` 1초 smoke | Task 426 표의 값이 그대로 |
| 4 | legacy 미지정 smoke | 정상 진행 — 삭제한 것이 legacy 래퍼이므로 이 확인이 중요합니다 |

## 5. 완료 기준

1. 두 함수가 header와 source에서 사라졌습니다.
2. 빌드·probe·smoke 두 종이 통과합니다.
3. 작업 로그를 남겼습니다.

---

# Task 427 Work Order — delete two unreferenced execution entry points

Follow-up from the [Task 426](20260805-426-backend-consolidation-residue.md) sweep. **This is
unrelated to the backend consolidation** — it is long-dead code that the sweep surfaced.

## 1-2. Targets and history

`AttemptWin32MinimalExecution` and `AttemptWin32GuestStackExecution`, declared in
`execution_trampoline.h` and defined in `execution_trampoline.cpp`, have **no references beyond
their own declaration and definition** anywhere in `src`, `include`, `tests`, or `tools`. Both
are thin wrappers that pass a null `aot_placement` and `ExecutionBackend::kLegacy` to
`RunWin32ExecutionThread`. `git log -S` places their introduction at
`58db6f2 Add minimal Win32 loader execution path` and `baa89f4 Add Win32 guest stack
trampoline`, last touched by `d1673e2` (Task 233, v0.0.60) during the module split: early
bring-up entry points that lost their callers as the paths above them matured, not residue from
Tasks 424-426.

## 3. Change

Remove both declarations and both definitions, and nothing else. The three entry points
`main.cpp` uses and the shared `RunWin32ExecutionThread` are untouched.

## 4-5. Verification and completion

Release build exit 0 — a clean link re-confirms the absence of callers through the compiler —
the full `aot_probe`, a one-second `dynamic` smoke matching the Task 426 values, and **a legacy
smoke with the backend unset**, which matters most here because the deleted wrappers were the
legacy ones. Done when both functions are gone from header and source, all four checks pass, and
the work log is written.
