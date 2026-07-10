# Port I/O trace buffer 설계

## 배경

현재 `piu_1st`는 `OUT DX,EAX port=0x02A0 value=0x00000005`에서 중단된다. 이 값을 바로 장치 HLE로 구현하기에는 `0x02A0` 계열 포트가 어떤 장치를 의미하는지 근거가 부족하다.

기존 로그는 마지막 Port I/O 하나만 보여주므로, 초기화 write 시퀀스인지, 장치 상태 확인 전 단계인지, 특정 register block인지 판단하기 어렵다.

## 설계

`0x02A0` 계열 Port I/O 의미 분석을 위해 작은 trace buffer를 추가한다.

* `Win32PortIoObservation`에 고정 크기 trace entry 배열을 추가한다.
* 각 entry에는 address, opcode, port, width, value, direction, handled 여부를 저장한다.
* loader는 저장된 trace entry를 순서대로 출력한다.
* 기존 exact allow-list는 유지한다.
* 아직 의미가 확정되지 않은 `0x02A0..0x02AF` 범위의 4-byte `OUT DX,EAX`는 trace buffer 용량 안에서만 `trace-ignored`로 통과시킨다.
* trace buffer가 가득 찬 뒤 같은 범위의 미확정 Port I/O가 계속 나오면 `trace-limit`으로 중단한다.
* 범위 밖 Port I/O는 기존처럼 `unsupported`로 중단한다.

이 방식은 모든 Port I/O를 무조건 no-op 처리하지 않으면서, 장치 후보를 판단할 수 있는 연속 시퀀스를 확보하기 위한 진단 단계이다.

## 기대 결과

`piu_1st`가 `0x02A0/0x5` 하나에서 바로 멈추지 않고, 제한된 수의 `0x02A0` 계열 Port I/O 시퀀스를 출력해야 한다. 그 결과를 바탕으로 다음 작업에서 exact allow-list 유지, small device 후보 문서화, 또는 실제 장치 HLE 설계를 결정한다.

## 범위 밖

* `0x02A0` 계열의 실제 장치 의미를 이번 작업에서 확정하지 않는다.
* `IN` port 응답 모델을 만들지 않는다.
* 모든 Port I/O를 통과시키지 않는다.

# Port I/O Trace Buffer Design

## Background

`piu_1st` currently stops at `OUT DX,EAX port=0x02A0 value=0x00000005`. There is not enough evidence yet to implement a device HLE for the `0x02A0` port family.

The existing log only shows the last Port I/O operation, which is not enough to tell whether this is an initialization write sequence, a prelude to a status read, or a register block for a specific device.

## Design

Add a small trace buffer for `0x02A0`-family Port I/O analysis.

* Add a fixed-size trace entry array to `Win32PortIoObservation`.
* Store address, opcode, port, width, value, direction, and handled state for each entry.
* Print the stored trace entries from the loader in order.
* Keep the existing exact allow-list.
* For not-yet-understood 4-byte `OUT DX,EAX` writes in the `0x02A0..0x02AF` range, continue only while trace buffer capacity remains and record them as `trace-ignored`.
* If the trace buffer is full and the same family continues, stop with `trace-limit`.
* Port I/O outside that range still stops as `unsupported`.

This is a diagnostic step that gathers a continuous sequence without turning every Port I/O into a no-op.

## Expected Result

`piu_1st` should proceed past the single `0x02A0/0x5` write and print a bounded `0x02A0`-family Port I/O sequence. That data will drive the next decision: keep exact allow-listing, document a small device candidate, or design an actual device HLE.

## Out Of Scope

* Do not determine the real device meaning of the `0x02A0` family in this task.
* Do not implement an `IN` port response model.
* Do not pass every Port I/O through.
