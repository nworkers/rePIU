# INT 8 체인 벡터 HLE 작업 로그

## 결과

Win32 전용 `timer_interrupt_boundary`를 추가했다. 게임이 설치한 INT 8 핸들러에서 관찰된 `9C FF 1D disp32` 패턴이 `002B:00000000`의 빈 이전 벡터를 가리킬 때, HLE는 원래 체인 ISR의 `IRET` 완료 상태를 모사한다. 즉 `ESP`에서 이미 push된 EFLAGS 4바이트를 제거하고 `EIP`를 far call 뒤로 이동한다.

처리는 현재 CPU `SegDs`, 대상 offset 0, INT 8 DPMI vector 설치 여부를 모두 확인하므로 일반 far call을 포괄하지 않는다. 실행 결과에는 처리 횟수와 source/pointer/target을 기록한다.

## 검증

* `scripts\\build_win32_x86.bat`: 성공.
* 직접 loader 조건: `REPIU_EXECUTION_BACKEND=aot-dynamic`, `REPIU_EXECUTION_TIMEOUT_MS=0`, `repiu_loader_win32.exe pumpit1`.
* 첫 관찰에서는 과거 `ThreadContext::guest_ds` 값이 현재 CPU selector와 달라 경계가 거절되었고, 예외가 기존대로 `0x03042EBE`에서 재현되었다. 조건을 예외 시점 `SegDs`로 바꿨다.
* 수정 뒤 동일 직접 loader 실행은 외부 94초 관찰 제한까지 기존의 약 82초 `0x03042EBE` 예외나 종료 로그 없이 계속 실행됐다. 관찰용으로 시작한 loader 프로세스는 이후 종료했다.

## 다음 단계

장시간 직접 loader 관찰에서 새 execution frontier가 나타나는지 확인한다. 일반 far call 또는 실제 이전 IRQ0 handler 구현은 현재 범위에 포함하지 않는다.

# INT 8 Chain Vector HLE Work Log

## Result

Added a Win32-only `timer_interrupt_boundary`. When the observed `9C FF 1D disp32` pattern in the game-installed INT 8 handler points to the absent previous vector `002B:00000000`, the HLE simulates the completed chained ISR `IRET`: it removes the four already-pushed EFLAGS bytes from `ESP` and advances `EIP` past the far call.

The handler requires the faulting CPU `SegDs`, target offset zero, and an installed DPMI INT 8 vector, so it does not generalize to arbitrary far calls. The execution result records the handling count and source/pointer/target.

## Verification

* `scripts\\build_win32_x86.bat`: succeeded.
* Direct loader: `REPIU_EXECUTION_BACKEND=aot-dynamic`, `REPIU_EXECUTION_TIMEOUT_MS=0`, `repiu_loader_win32.exe pumpit1`.
* The first observation rejected the boundary because historical `ThreadContext::guest_ds` differed from the current CPU selector, reproducing the old fault at `0x03042EBE`. The predicate was changed to the faulting `SegDs`.
* After that change, the same direct-loader run continued through the external 94-second observation limit without the prior roughly 82-second `0x03042EBE` exception or termination log. The loader process started for observation was then stopped.

## Next step

Use a longer direct-loader observation to identify the next execution frontier. General far-call handling and implementation of a real prior IRQ0 handler are outside this task.
