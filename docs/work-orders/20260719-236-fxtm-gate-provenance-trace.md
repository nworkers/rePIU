# fxTMGetTMBlock 게이트 인자 추적 작업 지시
# fxTMGetTMBlock Gate Argument Trace Work Order

## 작업 목표

장기 실행에서 재현되는 `fxTMGetTMBlock()`의 비정상 크기 `0x030FEE17`에 대해, `_GRTEXMINADDRESS@4`와 `_GRTEXMAXADDRESS@4` HLE 게이트의 실제 ABI 및 계획된 복귀 상태를 종료 로그로 보존한다.

For the abnormal `0x030FEE17` size reproduced by `fxTMGetTMBlock()` in long runs, preserve the actual ABI and planned return state of `_GRTEXMINADDRESS@4` and `_GRTEXMAXADDRESS@4` HLE gates in the termination log.

## 변경 범위

- `ThreadContext`에 고정 크기 TMU 게이트 진단 링 추가
- 게이트 경계에서 처리 직전 상태 기록
- 실행 결과 스냅샷과 loader 요약 출력에 trace 연결
- 이 작업에 대응하는 설계 및 작업 로그 작성

- Add a fixed-size TMU gate diagnostic ring to `ThreadContext`.
- Record state immediately before handling at the gate boundary.
- Connect the trace to the execution-result snapshot and loader summary.
- Write the corresponding design and work log.

## 검증 절차

1. `scripts\\build_win32_x86.bat`로 Win32 Debug 전체 빌드를 수행한다.
2. `REPIU_EXECUTION_BACKEND=aot-dynamic`, `REPIU_EXECUTION_TIMEOUT_MS=0`으로 `repiu_supervisor_win32.exe pumpit1 180000`을 실행한다.
3. `fxTMGetTMBlock` 실패가 재현될 경우 두 TMU 게이트 trace의 EIP, ESP, 인자, 반환 EAX, 예상 복귀 ESP를 확인한다.
4. trace가 없거나 게이트 반환 동작이 달라질 경우 작업을 실패로 기록하고 원인을 남긴다.

1. Perform the complete Win32 Debug build with `scripts\\build_win32_x86.bat`.
2. Run `repiu_supervisor_win32.exe pumpit1 180000` with `REPIU_EXECUTION_BACKEND=aot-dynamic` and `REPIU_EXECUTION_TIMEOUT_MS=0`.
3. If `fxTMGetTMBlock` failure reproduces, inspect EIP, ESP, argument, return EAX, and planned post-return ESP in both TMU gate trace entries.
4. If no trace is produced or gate return behavior changes, record the work as failed and document the reason.
