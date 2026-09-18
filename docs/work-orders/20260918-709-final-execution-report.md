# Task 709 작업 지시 — 회수 거절 경로의 최종 실행 요약

설계: [20260918-709](../design/20260918-709-final-execution-report.md)

## 범위

게스트 스레드 회수가 거절된 실행도 최종 요약을 출력하게 한다. Task 507/508의
정리 정책, 종료 코드, 게스트 실행 경로는 바꾸지 않는다.

## 단계

1. `include/repiu/engine/final_execution_report.h`,
   `src/engine/telemetry/final_execution_report.cpp`
   * `SetFinalExecutionReport(callback, user)`
   * `EmitFinalExecutionReport(attempt)` — 등록이 없으면 무동작, 한 번만 호출
   * `FinalExecutionReportEmitted()`
2. `src/engine/execution/execution_trampoline.cpp`의 `!gracefully_interrupted`
   갈래
   * `CopyThreadObservationToAttempt` 호출
   * `attempt->valid = true`, `message`에 이 갈래를 말하는 고정 문자열
   * `std::string` 필드는 건드리지 않는다
   * `EmitFinalExecutionReport` 뒤 기존 `_Exit`
3. `src/host/win32/main.cpp`
   * 실행 시작 전에 `PrintExecutionAttempt`를 부르는 콜백 등록
   * 기존 정상 경로 출력도 `EmitFinalExecutionReport`를 지나가게 해 중복 방지
4. CMake에 새 source 등록
5. `final_execution_report` core probe group 신규, 목록 등록
6. 문서: 작업 로그, `docs/analysis/linux-port-frontier.md`

## 검증

* Linux x64 Debug 빌드, core probe 전체
* Win32 x86 Debug 빌드, core probe 전체
* Linux x64 `pumpit2a` 30초: `immediate-exit` 앞에 요약이 나오고
  `Win32 DOS path trace` / `Win32 DOS file I/O` 줄이 보이는지
* Win32 x86 `pumpit2a`: 요약이 정확히 한 번

## 완료 조건

* Linux x64 실행 로그에 최종 요약이 나타난다.
* Win32 요약이 중복되지 않는다.
* 새 probe가 수정 전 구현에서 실패하고 수정 후 통과한다.
* 기존 group이 하나도 회귀하지 않는다.

## 완료 조건이 아닌 것

텍스처 업로드가 왜 생략되는지는 이 작업이 답하지 않는다.

---

## English

Design: [20260918-709](../design/20260918-709-final-execution-report.md)

### Scope

Make a run whose guest-thread recovery was refused still print its final
summary. The Task 507/508 cleanup policy, the exit code, and the guest
execution path are unchanged.

### Steps

1. Add `include/repiu/engine/final_execution_report.h` and
   `src/engine/telemetry/final_execution_report.cpp` with
   `SetFinalExecutionReport`, `EmitFinalExecutionReport` (no-op when nothing is
   registered, invoked once) and `FinalExecutionReportEmitted`.
2. On the `!gracefully_interrupted` arm of
   `src/engine/execution/execution_trampoline.cpp`, call
   `CopyThreadObservationToAttempt`, set `valid` and a fixed `message` naming
   this arm, leave the `std::string` fields untouched, emit, then `_Exit` as
   before.
3. In `src/host/win32/main.cpp`, register a callback that calls
   `PrintExecutionAttempt`, and route the existing normal-path print through
   `EmitFinalExecutionReport` so it cannot print twice.
4. Register the new source with CMake.
5. Add a `final_execution_report` core-probe group and register it.
6. Documents: the work log and `docs/analysis/linux-port-frontier.md`.

### Verification

* Linux x64 and Win32 x86 Debug builds and every core-probe group
* A 30-second Linux x64 `pumpit2a` run showing the summary ahead of
  `immediate-exit`, including the `Win32 DOS path trace` and
  `Win32 DOS file I/O` lines
* A Win32 x86 run printing the summary exactly once

### Done when

* The Linux x64 run log contains the final summary.
* The Win32 summary is not duplicated.
* The new probe fails against the pre-fix implementation and passes after it.
* No existing group regresses.

### Not a completion condition

Why the texture upload is skipped is not answered here.
