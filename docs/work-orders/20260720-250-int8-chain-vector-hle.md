# INT 8 체인 벡터 HLE 작업 지시

## 목표

게임이 설치한 INT 8 핸들러가 비어 있는 이전 타이머 벡터를 chain할 때 발생하는 `002B:00000000` far-call 예외를, 원래 ISR의 `IRET` 규약을 보존하는 최소 HLE로 처리한다.

## 범위

1. Win32 timer interrupt boundary source/header를 추가한다.
2. 엄격한 관찰 조건에서만 이전 ISR의 no-op `IRET`를 보정한다.
3. 실행 결과 진단을 추가한다.
4. Win32 x86 debug build를 수행한다.

## 제외

* 임의의 far call 일반 처리
* 게임 로직 재구현
* 실제 Win32 IDT 또는 하드웨어 IRQ 수정

# INT 8 Chain Vector HLE Work Order

## Objective

Handle the `002B:00000000` far-call fault when the game-installed INT 8 handler chains an absent prior timer vector, using minimal HLE that preserves the original ISR `IRET` convention.

## Scope

1. Add a Win32 timer-interrupt boundary source/header.
2. Apply a no-op prior-ISR `IRET` adjustment only under strict observed conditions.
3. Add execution-result diagnostics.
4. Build Win32 x86 debug.

## Exclusions

* General handling of arbitrary far calls
* Reimplementation of game logic
* Modification of the real Win32 IDT or hardware IRQs
