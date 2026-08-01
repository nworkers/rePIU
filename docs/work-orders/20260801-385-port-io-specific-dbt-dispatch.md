# 20260801-385 작업 지시: Port-I/O 전용 DBT Dispatch / Work Order

설계: [20260801-385-port-io-specific-dbt-dispatch.md](../design/20260801-385-port-io-specific-dbt-dispatch.md)

## 한국어

1. code-cache option/image와 Win32 placement에 Port-I/O 전용 dispatch 정책을 추가합니다.
2. `kPortIo` switch를 일반 HLE boundary와 분리하고 전용 옵션에서만 dispatch slot을 방출합니다.
3. static placement와 dynamic append에 정책을 전달합니다.
4. HLE coverage validator가 Port-I/O dispatch slot 구조를 검증하도록 확장합니다.
5. 일반 HLE는 INT3에 남고 Port-I/O만 dispatch되는 synthetic probe를 추가합니다.
6. `REPIU_AOT_DBT_PORT_IO_DISPATCH` opt-in과 상태 로그를 추가합니다.
7. Release Win32 빌드, 전체 probe, 격리 EEPROM 스모크를 수행하고 작업 로그를 작성합니다.

## English

1. Add a Port-I/O-specific dispatch policy to code-cache options/images and Win32 placement.
2. Separate the `kPortIo` switch from ordinary HLE boundaries and emit dispatch slots only under its dedicated option.
3. Carry the policy through static placement and dynamic append.
4. Extend HLE coverage validation for the Port-I/O dispatch-slot layout.
5. Add a synthetic probe showing that Port-I/O dispatches while ordinary HLE remains INT3.
6. Add the `REPIU_AOT_DBT_PORT_IO_DISPATCH` opt-in and status log.
7. Run Release Win32 build, the full probe, and an isolated-EEPROM smoke, then write the work log.
