# 작업 지시서: 포트 I/O 한도 도달 오류 완화 및 8비트 포트 I/O 지원
# Work Order: Port I/O Limit Tolerance and 8-bit Port I/O Support

## 목표
## Goal
`pumpit1`을 `aot-dynamic` 백엔드 환경에서 구동 시, 포트 I/O 지연 로깅 상한선에 도달하여 발생하는 강제 종료 문제와, 8비트 포트 I/O 명령어(`EE`, `EC`) 미지원으로 인한 크래시 문제를 방지하고 연속 실행이 보장되도록 개선합니다.

Prevent process crashes caused by reaching the port I/O trace limit or executing unsupported 8-bit port I/O instructions (`EE`, `EC`) when running `pumpit1` on the `aot-dynamic` backend, ensuring continuous execution.

---

## 변경 계획
## Proposed Changes

### [Component: Platform Win32 / Trampoline]

#### [MODIFY] [execution_trampoline.h](file:///e:/MYWORK/Projects/rePIU/include/repiu/platform/win32/execution_trampoline.h)
- `kWin32DeferredPortIoLimit` 값을 `65536`으로 상향 조정합니다. (완료됨)
- Bump `kWin32DeferredPortIoLimit` to `65536`. (Done)

#### [MODIFY] [execution_trampoline.cpp](file:///e:/MYWORK/Projects/rePIU/src/platform/win32/execution_trampoline.cpp)
- `IsPortIoTraceCandidate` 함수가 1, 2, 4바이트 모든 출력을 candidate로 처리할 수 있도록 수정합니다.
- `HandlePortIoInstruction` 함수가 `0x66` prefix가 없는 8비트/32비트 포트 I/O 명령어(`EC`, `ED`, `EE`, `EF`)도 함께 처리할 수 있도록 디코딩 로직을 확장합니다.
- `HandlePortIoInstruction` 내 한도 초과 시 강제 종료(`return false;`) 대신 성공 처리(`return true;`)하도록 완화 로직을 8비트/16비트/32비트 처리에 각각 동일하게 보장합니다.

- Modify `IsPortIoTraceCandidate` to handle 1, 2, and 4-byte writes as candidates.
- Extend decoding logic in `HandlePortIoInstruction` to handle 8-bit and 32-bit port I/O instructions (`EC`, `ED`, `EE`, `EF`) without `0x66` prefix.
- Ensure that the relaxed error handling logic (returning `true` instead of `false` on limit breach) applies universally across 8-bit, 16-bit, and 32-bit instructions.

---

## 검증 계획
## Verification Plan

### 수동 검증
### Manual Verification
1. `.\scripts\build_win32_x86.bat`를 실행하여 로더를 다시 빌드합니다.
2. `build\win32_x86_debug\Debug\repiu_loader_win32.exe pumpit1`를 실행해 지연 포트 I/O 한도가 1024회를 넘어섰을 때 정상적으로 생존하고 `out dx, al` (`EE`), `in al, dx` (`EC`) 등 8비트 하드웨어 예외를 만나도 정상적으로 우회되어 그 다음 실행 블록으로 넘어가는지 검증합니다.

1. Run `.\scripts\build_win32_x86.bat` to rebuild the loader.
2. Execute `build\win32_x86_debug\Debug\repiu_loader_win32.exe pumpit1` and verify that the loader survives beyond 1024 deferred port I/O writes and successfully skips 8-bit port instructions like `out dx, al` (`EE`) and `in al, dx` (`EC`), continuing to subsequent code paths.
