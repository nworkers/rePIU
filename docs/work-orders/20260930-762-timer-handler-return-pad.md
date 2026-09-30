# Task 762: 타이머 핸들러의 복귀를 보는 return pad 작업 지시

설계: [20260930-762](../design/20260930-762-timer-handler-return-pad.md)

## 한국어

1. `include/repiu/engine/timer_return_pad.h`와 `src/engine/io/timer_return_pad.cpp`에 record
   스택(`TimerReturnPad`)과 스위치(`REPIU_TIMER_RETURN_PAD`)를 둔다. 플랫폼 호출이 없는 순수
   로직이다.
2. pad 주소는 `platform::ReserveMemory`로 커밋 없이 예약한다(프로세스에 하나).
3. `InjectPendingInterrupts`: direct 모델이고 스위치가 켜져 있으면 record를 쌓고 frame의 복귀
   EIP에 pad를 넣고 추적 상태를 비운다. 주입 전에 버려진 frame의 record를 정리한다.
4. `DispatchGuestFault`의 맨 앞에서 `EIP == pad`를 받아 설계 3절의 복귀 처리를 한다.
5. 요약 출력에 pad 통계 한 줄을 더한다.
6. core probe와 aot probe에 `timer_return_pad`를 더한다.
7. 설계 4절대로 검증하고, README·ARCHITECTURE.md·작업 로그를 갱신한 뒤 커밋한다.

## English

1. Put the record stack (`TimerReturnPad`) and the switch (`REPIU_TIMER_RETURN_PAD`) in
   `include/repiu/engine/timer_return_pad.h` and `src/engine/io/timer_return_pad.cpp`; pure
   logic with no platform call.
2. Reserve the pad's address with `platform::ReserveMemory`, uncommitted, one per process.
3. `InjectPendingInterrupts`: on the direct model with the switch on, push a record, write the
   pad as the frame's return EIP and clear the trace state; prune the records of abandoned
   frames before injecting.
4. Take `EIP == pad` at the very top of `DispatchGuestFault` and handle the return as section
   3 of the design says.
5. Add one line of pad statistics to the summary.
6. Add `timer_return_pad` to the core probe and the aot probe.
7. Verify as section 4 of the design says, update README, ARCHITECTURE.md and the work log,
   and commit.
