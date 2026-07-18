# 20260718-233 작업 로그 — Phase 1 증분 2 (Port I/O TU 분리)

관련: [설계](../design/20260718-233-execution-trampoline-decomposition.md) · [작업 지시서](../work-orders/20260718-233-execution-trampoline-decomposition-order.md) · [증분1 로그](20260718-233-execution-trampoline-phase1-increment1-log.md)

## 한 일 / What was done

Port I/O 하위 시스템(`RecordPortIo`, `IsObservedPortInitializationWrite`, `IsPortIoTraceCandidate`, `HandlePortIoInstruction`)을 별도 TU `port_io_emulator.{h,cpp}`로 분리했다. 이 모듈은 트램폴린 내부 함수 `IsAotCacheAddress`·`WriteGuestBytes` 두 개를 호출하므로, 이들을 **익명 네임스페이스 밖(외부 링크)으로 승격**하고 새 헤더 `execution_internal.h`에 선언했다.

Extracted the Port I/O subsystem into a separate translation unit. Because it calls two trampoline-internal functions (`IsAotCacheAddress`, `WriteGuestBytes`), those were promoted to external linkage — their definitions moved out of the anonymous namespace (relocated just after the anonymous namespace closes) and declared in a new `execution_internal.h`.

- 신규: `src/platform/win32/execution_internal.h`(경계 함수 선언), `port_io_emulator.h`, `port_io_emulator.cpp`.
- `CMakeLists.txt`: `port_io_emulator.cpp`를 `repiu_exe` 소스에 추가.
- `execution_trampoline.cpp`: 11,592 → 11,371줄. port_io 4개 함수 정의 제거, 경계 함수 2개 재배치, 관련 forward 선언 2개 제거, top-level include 2개 추가.

## 핵심: 경계 함수 승격 패턴 / Boundary promotion pattern

익명 네임스페이스에 정의된 함수는 내부 링크라 다른 TU에서 링크 불가다(이전 WIP가 실패한 지점). 해결:

1. 공유 헤더(`execution_internal.h`)에 named 네임스페이스로 **선언**(외부 링크).
2. **정의를 익명 네임스페이스 밖으로 이동**. 익명 네임스페이스 닫힘 직후(named 영역)에 두면, 함수가 호출하는 익명 헬퍼(`IsGuestRangeWritable`, `NoteSuccessfulAotGuestWrite`)는 암묵적 using-directive로 계속 보인다.
3. 익명 네임스페이스에 남아있던 forward 선언은 제거(외부/내부 링크 충돌 방지).

이후 모듈도 동일 패턴을 반복하며, 필요한 경계 함수만 그때그때 승격한다.

## 검증 / Verification

```
cmake --build build/win32_x86_dpmi --config Debug --target repiu_exe          # green (execution_trampoline.cpp, port_io_emulator.cpp)
cmake --build build/win32_x86_dpmi --config Debug --target repiu_loader_win32  # green (링크 성공, unresolved 없음)
```
정적 라이브러리 컴파일뿐 아니라 실행 파일 링크까지 확인해 크로스-TU 심볼(`IsAotCacheAddress`, `WriteGuestBytes`, port_io 4함수) 정합성을 검증했다. `HandlePortIoInstruction` 호출부 2곳은 헤더 선언으로 정상 해결.

## 다음 / Next

증분 3: `exception_rescue_win32.{h,cpp}`(ExceptionDispatchScope·VEH 엔트리) 분리. 경계 승격 대상: `g_active_thread_context`(및 `g_recovery_*`), `DispatchGuestException`.
