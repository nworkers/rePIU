# 20260726-296 작업 지시: DispatchGuestException 관측 견고화 / Work order

설계: [docs/design/20260726-296-dispatch-guest-exception-hardening.md](../design/20260726-296-dispatch-guest-exception-hardening.md)

## 한국어

### 목표
손상된 `EXCEPTION_POINTERS`(예: `ContextRecord=0x23`)를 `DispatchGuestException`이 역참조하며
발생하는 2차 Access Violation을 제거하고, 가려진 1차 게스트 예외를 관측 가능하게 만든다.

### 구현 항목
1. `src/platform/win32/execution/execution_trampoline.cpp`
   - 익명 네임스페이스에 `IsHostPointerReadable(const void*, std::size_t)` 헬퍼 추가 (`VirtualQuery` 기반).
   - `RecordMalformedExceptionPointers(ThreadContext*, EXCEPTION_POINTERS*)` 헬퍼 추가
     (카운터 증가, 마지막 손상값 저장, stderr 진단 1줄).
   - `DispatchGuestException` 진입부의 `ContextRecord == nullptr` 가드를, `info`/`ExceptionRecord`/
     `ContextRecord` 읽기 가능 검증으로 교체하고 실패 시 `EXCEPTION_CONTINUE_SEARCH` 반환.
2. `src/platform/win32/execution/thread_context.h`
   - `exception_dispatch_malformed_count`, `exception_dispatch_last_bad_context`,
     `exception_dispatch_last_bad_record` (`std::atomic<std::uint32_t>`) 추가.
3. `include/repiu/platform/win32/execution_trampoline.h`
   - `Win32MinimalExecutionAttempt`에 동일 3개 필드(`std::uint32_t`) 추가.
4. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`
   - 요약 스냅샷 복사부에서 3개 필드를 `context` → `attempt`로 복사.
5. `src/host/win32/main.cpp`
   - 실행 종료 요약에 `Win32 exception dispatch malformed count / last bad context / last bad record` 출력.

### 검증
- `cmake --build build/win32_x86_debug --config Debug` 성공.
- `aot-dbt`로 `pumpit1` 구동 후 로그에서 2차 크래시(`0x101AF9A1`) 소멸 또는 1차 예외 주소 노출 확인,
  요약의 malformed count 노출 확인.

### 범위 밖
INT 8 주입 IF 게이트/중첩 방지, 게스트 스택 TIB 정합, 디버그 스캐폴딩 정리.

## English

### Goal
Remove the secondary Access Violation caused by `DispatchGuestException` dereferencing a malformed
`EXCEPTION_POINTERS` (e.g. `ContextRecord=0x23`) and make the masked primary guest exception observable.

### Items
1. `execution_trampoline.cpp`: add `IsHostPointerReadable` (VirtualQuery-based) and
   `RecordMalformedExceptionPointers`; replace the `ContextRecord == nullptr` guard in
   `DispatchGuestException` with readability validation of `info`/`ExceptionRecord`/`ContextRecord`,
   returning `EXCEPTION_CONTINUE_SEARCH` on failure.
2. `thread_context.h`: add the three `std::atomic<std::uint32_t>` diagnostic fields.
3. `execution_trampoline.h`: add the three `std::uint32_t` fields to `Win32MinimalExecutionAttempt`.
4. `live_telemetry_snapshot.cpp`: copy the three fields into the summary snapshot.
5. `main.cpp`: print the three fields in the end-of-run summary.

### Verification
Debug build succeeds; running `pumpit1` under `aot-dbt` shows the secondary `0x101AF9A1` crash gone (or
the primary exception address surfaced) and the malformed count in the summary.

### Out of scope
INT 8 injection IF-gating / nesting, guest-stack TIB alignment, debug scaffolding cleanup.
