# 20260803-404 AOT 세대 실패 원인 계측 작업 지시 / AOT Generation Failure Attribution Work Order

설계: [20260803-404](../design/20260803-404-aot-generation-failure-attribution.md)

## 한국어

### 목적

재번역 실패 1건이 페이지를 영구 격리하고 wall의 35~40%를 소모하는데 **실패 사유가
기록되지 않습니다.** 사유를 확보합니다. 동작은 바꾸지 않습니다.

### 범위

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | 세대 실패 trace 구조체와 카운터 추가 |
| `src/platform/win32/aot/aot_runtime_dispatch.cpp` | 실패 분기에서 trace 기록 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.{h,cpp}` | attempt 스냅샷으로 전달 |
| `src/host/win32/main.cpp` | 요약 로그 2종 추가 |

### 단계

1. `ThreadContext`에 `kGenerationFailureTraceCapacity = 8`, `GenerationFailureTraceEntry`
   (`target`, `page`, `quarantined`, `terminal`, `message[96]`), 배열, `..._count`,
   `..._overflow_count`를 추가한다.
2. `ResolveAotTransferTarget`의 `dynamic_translation_failed && retired_target` 분기에서,
   격리/terminal 분기 결과가 확정된 뒤 한 항목을 기록한다. `message`는
   `context->aot_translation_result.message`를 잘라 복사하고 항상 NUL 종료한다.
3. 스냅샷 구조체에 같은 필드를 추가하고 복사한다. 기존 `quarantine_trace` 전달 방식을
   그대로 따른다.
4. `main.cpp`에 요약을 추가한다.
   * `Win32 AOT generation failure events/overflow: {}/{}`
   * `Win32 AOT generation failure #{} target/page/quarantined/terminal/message: ...`
5. Debug와 Release로 `repiu_loader_win32`, `repiu_aot_probe`를 빌드한다.

### 검증

* `repiu_aot_probe` 두 구성 모두 종료 코드 0, 기존 항목 전부 true.
* pumpit3 45초 실행을 격리가 재현될 때까지 반복하고, 재현된 실행에서 사유 문자열을
  기록한다. 오늘 기준 재현율은 10회 중 6회다.
* 동작 불변: 같은 실행의 `AOT generation publishes/quarantines`,
  `hle reentry funnel`, 프레임 수, 종료 사유가 계측 전 분포와 어긋나지 않는다.
* pumpit1 45초 1회로 회귀가 없음을 확인한다.

### 완료 조건

실패 사유 문자열을 실측으로 확보하고 작업 로그에 남긴다. 정책 변경은 이 지시에
포함되지 않는다.

---

## English

### Purpose

A single re-translation failure permanently quarantines a page and costs 35-40% of wall
clock, yet **the reason is never recorded**. Capture the reason. Change no behaviour.

### Scope

`thread_context.h` gains the trace structure and counters; `aot_runtime_dispatch.cpp`
records one entry at the failure branch; the live telemetry snapshot forwards the fields
the way `quarantine_trace` already is; `main.cpp` prints two summary lines.

### Steps

1. Add `kGenerationFailureTraceCapacity = 8`, a `GenerationFailureTraceEntry` holding
   `target`, `page`, `quarantined`, `terminal`, and `message[96]`, the array, a count, and
   an overflow count.
2. Record one entry in the `dynamic_translation_failed && retired_target` branch of
   `ResolveAotTransferTarget`, after the quarantine/terminal outcome is decided. Copy
   `context->aot_translation_result.message` truncated and always NUL-terminated.
3. Mirror the fields into the attempt snapshot, following the existing `quarantine_trace`
   pattern.
4. Add `Win32 AOT generation failure events/overflow` and a per-entry
   `target/page/quarantined/terminal/message` line.
5. Build `repiu_loader_win32` and `repiu_aot_probe` in Debug and Release.

### Verification

Both probe configurations exit zero with existing checks true; repeated 45-second pumpit3
runs until the quarantine reproduces (six of ten today) yield the reason string; quarantine
counts, the re-entry funnel, frames, and the exit reason stay in their pre-change
distribution; and one 45-second pumpit1 run shows no regression.

### Done when

The failure reason is captured by measurement and written into the work log. Policy changes
are not part of this order.
