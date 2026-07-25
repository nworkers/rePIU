# 20260725-294-preemptive-timer-injection

## 작업 목표 (Objective)
- `grLfbUnlock` 이후 발생하는 타이머 카운터 폴링 무한 루프(busy-wait) 문제를 해결하기 위해, 호스트 감시 루프 내에 선점형 INT 8h 타이머 인터럽트 주입 메커니즘 구현.

## 변경 사항 (Changes)
- `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` 수정
  - `PollThreadUntilExit` 함수 내에서 경과된 시간에 따라 `timer_interrupt_pending`을 업데이트하던 로직을 확장.
  - 새 타이머 틱 주입 조건 만족 시, Guest 스레드를 `SuspendThread`로 중단.
  - `GetThreadContext`로 획득한 EIP가 Guest 메모리 범위(`IsGuestInstructionPointer`) 혹은 AOT 캐시 범위(`IsAotCacheAddress`)일 경우, Eip/Esp/EFlags 레지스터를 직접 조작해 인터럽트 IRET 스택 프레임을 Guest 스택 영역에 쓰고 Eip를 INT 8h 핸들러의 offset으로 덮어씀.
  - 수정을 마친 컨텍스트를 `SetThreadContext`로 반영하고 `ResumeThread`를 통해 실행을 재개시킴.
  - EIP가 호스트 코드나 HLE 드라이버 내부에 있을 때는, 선점 주입을 취소하고 기존과 동일하게 `timer_interrupt_pending = true` 플래그만 세팅하여 VEH 예외 핸들러가 Guest 코드로 돌아갈 때 주입되도록 결합함.

## 검증 (Verification)
- CMake 빌드가 오류 없이 완벽하게 컴파일 완료됨 (`cmake --build build --config Release`).
