# 20260801-382 HLE 서비스 및 selector telemetry / HLE service and selector telemetry

## 한국어

Music Select HLE 병목을 명령군과 서비스별로 나누기 위해, hot path에서 원자 연산·동적 할당 없이 단순 카운터만 수집합니다.

- DOS `INT 21h`는 AH별 256칸 histogram을 기록합니다.
- segment load/store는 opcode별 256칸 histogram을 기록합니다.
- Port I/O는 opcode별 histogram과 input/output·handled/unhandled 합계를 기록합니다.

기존의 마지막 이벤트와 제한된 trace는 그대로 보존합니다. histogram은 guest 실행 스레드만 갱신하고 종료 snapshot에서 복사하여 상위 항목만 로그로 출력합니다. 관측 결과가 나오기 전에는 fast-path 동작을 변경하지 않습니다.

## English

To split Music Select HLE cost by instruction family and service, collect only simple counters on the hot path: no atomics or allocations.

- DOS `INT 21h` records a 256-slot histogram by AH.
- Segment load/store record a 256-slot histogram by opcode.
- Port I/O records an opcode histogram and input/output plus handled/unhandled totals.

Existing last-event fields and bounded traces remain unchanged. The guest execution thread updates histograms, exit snapshot copies them, and logs print only ranked entries. No fast-path behavior changes before evidence is captured.
