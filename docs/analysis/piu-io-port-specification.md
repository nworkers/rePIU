# 펌프 잇 업 (PIU) I/O 포트 사양 분석 및 유지 지침
# Pump It Up (PIU) I/O Port Specification Analysis and Maintenance Guidelines

## 1. 하드웨어 개요
## 1. Hardware Overview
초기 도스(DOS) 기반 펌프 잇 업(Pump It Up 1st ~ 3rd) 아케이드 시스템은 안다미로(Andamiro)의 독자적인 **PIU10 ISA PCB(또는 JAMMA I/O 보드)**를 사용합니다. 이 보드는 PC의 ISA 슬롯에 장착되어 시스템 제어, 발판 센서 입력, 코인 입력, 사운드(Yamaha YMZ280B), 딥스위치 세팅 및 93C46 EEPROM 제어 등의 기능을 수행합니다. 
해당 하드웨어는 PC의 I/O 포트 영역 중 `0x02A0`부터 `0x02AF`까지의 주소를 레지스터 맵으로 사용하여 CPU와 통신합니다.

Early DOS-based *Pump It Up* (PIU 1st to 3rd) arcade systems utilize Andamiro's proprietary **PIU10 ISA PCB (or JAMMA I/O board)**. Mounted in the PC's ISA slot, it interfaces with the cabinet's inputs (stage sensors, coins), sound (Yamaha YMZ280B), dip switches, and a 93C46 EEPROM.
This hardware communicates with the CPU via the I/O port address range `0x02A0` through `0x02AF`.

---

## 2. 레지스터 포트 매핑 사양
## 2. Register Port Mapping Specification
MAME(Multiple Arcade Machine Emulator) 프로젝트의 공식 드라이버 [src/mame/misc/xtom3d.cpp](https://github.com/mamedev/mame/blob/master/src/mame/misc/xtom3d.cpp) 분석에 근거한 상세 포트 사양은 다음과 같습니다.

Based on our analysis of the official MAME driver [src/mame/misc/xtom3d.cpp](https://github.com/mamedev/mame/blob/master/src/mame/misc/xtom3d.cpp), the detailed port specifications are as follows:

| 포트 주소 (Port) | 데이터 너비 (Width) | 입출력 (Direction) | 기능 및 대상 장치 (Description) | MAME 내부 매핑 함수 / HLE 처리 (MAME Driver / HLE Treatment) |
| :--- | :--- | :--- | :--- | :--- |
| `0x0040 ~ 0x0043` | 8-bit | Read / Write | Intel 8253/8254 PIT (Programmable Interval Timer) 제어 및 카운터 포트 | `unsupported-ignored` 로깅 후 쓰기 무시 및 더미 타이머 에뮬레이션 |
| `0x02A0 ~ 0x02A3` | 16-bit / 8-bit | Read / Write | Yamaha YMZ280B 사운드 칩 제어 및 데이터 포트 | `ymz280b_device` read/write |
| `0x02A8` | 8-bit | Read | P1 (1인용) 발판 센서 입력 (Up-Left, Up-Right, Center, Down-Left, Down-Right) | `P1 Inputs` read |
| `0x02A9` | 8-bit | Read | 시스템 입력 (코인 센서, 서비스 버튼, 테스트 버튼, 스타트 등) | `System Inputs` read |
| `0x02AA` | 8-bit | Read | P2 (2인용) 발판 센서 입력 | `P2 Inputs` read |
| `0x02AB` | 8-bit | Read | 캐비닛 기타 입력 (보조 코인기, 캐비닛 특수 신호 등) | `Cabinet Inputs` read |
| `0x02AC` | 8-bit | Write | 93C46 Serial EEPROM 제어선 쓰기<br>- Bit 0: Chip Select (CS)<br>- Bit 1: Clock (CLK)<br>- Bit 2: Data Input (DI) | `m_eeprom->clk_write / cs_write / di_write` |
| `0x02AE` | 8-bit | Read | 93C46 Serial EEPROM 데이터 읽기<br>- Bit 0: Data Output (DO) 상태 반환 (읽기 결과: `do_read() \| 0xfe`) | `m_eeprom->do_read() \| 0xfe` |

---

## 3. 핵심 HLE 에뮬레이션 전략
## 3. Key HLE Emulation Strategy
1. **포트 입력(Read) 에뮬레이션**:
   - `0x02A8 ~ 0x02AB` (입력): 실기 캐비닛 발판 센서는 기본적으로 풀업 저항 상태인 Active-Low 신호를 가집니다. 따라서 기본값으로 `0xFFU`(또는 `0xFFFFU`)를 채워 반환해야 입력 루프 폴링에서 정상 대기 상태를 유지합니다.
   - `0x02AE` (EEPROM 읽기): 게임 엔진은 저장된 설정값을 가져오기 위해 이 포트에서 DO 비트 변화를 감지합니다. 지속 실행을 위해 기본 비트 풀업 값(`0xFF` 또는 `0xFE`)을 모사해 에뮬레이트합니다.
   - `0x0040` (PIT 카운터 읽기): 레거시 PC 환경의 타이머 폴링으로 인한 지연을 차단하기 위해, 읽기 요청 시 일정한 간격의 dummy counter 또는 dynamic tick 값을 반환하도록 에뮬레이트합니다.
2. **포트 출력(Write) 에뮬레이션**:
   - `0x02AC` (EEPROM 쓰기): EEPROM에 전송하는 제어 신호는 에뮬레이터 수준에서 별도의 물리 연동이 필요 없으므로, 쓰기 동작 자체는 기록(`RecordPortIo`) 후 정상 무시 처리합니다.
   - `0x0043` (PIT 제어 쓰기): PC 스피커 톤 생성 및 타이머 주파수 조정을 위해 시스템 코드가 수행하는 PIT 제어 쓰기는 에뮬레이터 구동 안정성을 위해 `unsupported-ignored` 로그 기록을 남긴 뒤 무시 처리합니다.

1. **Port Read Emulation**:
   - `0x02A8 ~ 0x02AB` (Inputs): Real cabinet sensors use active-low signals. Returning default pulled-up values like `0xFFU` (or `0xFFFFU`) prevents hang on sensor loops.
   - `0x02AE` (EEPROM Read): The engine queries this port to fetch persistent configurations. Emulating a pulled-up state (e.g. `0xFF` or `0xFE`) avoids polling lock.
   - `0x0040` (PIT Counter Read): To prevent hang during timer calibration polling, return dummy ticks or dynamic values.
2. **Port Write Emulation**:
   - `0x02AC` (EEPROM Write): Writes to EEPROM control lines are logged and bypassed since physical EEPROM write is not strictly required.
   - `0x0043` (PIT Control Write): Hardware-level timer rate initialization commands (e.g., `out 0x43, al`) are logged as `unsupported-ignored` and safely bypassed.

---

## 4. 추가 관찰 시 사양 업데이트 및 관리 지침
## 4. Maintenance and Update Guidelines for Future Observations

새로운 포트 번지가 발견되거나 기존 매핑의 다른 사용 사례가 포착되면 아래 지침에 따라 사양을 지속 업데이트해야 합니다.

When new I/O ports or mapping variations are identified, the specification must be updated according to the following guidelines:

1. **바이너리/로더 로그 교차 검증**:
   - 실행 중 `unsupported port I/O` 오류가 발생하면, 즉시 예외 시점의 `EIP` 바이트 창에서 명령어 크기(8비트: 1바이트, 16/32비트: 2바이트) 및 대상 포트 번지(`DX` 레지스터)를 확인하십시오.
2. **문서 동기화**:
   - 확인된 포트 주소와 성격(Read/Write, Bit Map)을 이 문서의 2번 섹션 테이블에 누적하여 추가하십시오.
   - 외부 아케이드 하드웨어 레퍼런스(예: MAME 드라이버, `piutools`/`pumptools` 후킹 저장소)의 실장 사양 출처 링크를 명시하십시오.
3. **HLE 라우터 기능 추가**:
   - `HandlePortIoInstruction`에 해당 포트의 에뮬레이션 계약(더미 반환값 및 EIP 오프셋)을 구현하고, `IsPortIoTraceCandidate` Allow-list에 포트를 등록하십시오.

1. **Cross-validate Loader Logs**:
   - If an `unsupported port I/O` error occurs, trace the instruction size (1 byte for 8-bit, 2 bytes for 16/32-bit) and the target port (`DX` register) from the EIP window immediately.
2. **Synchronize Documentation**:
   - Add new port addresses and details (read/write registers, bit map) to the table in Section 2.
   - Reference authoritative sources (e.g. MAME source, `piutools`/`pumptools` hook repos).
3. **Extend HLE Router**:
   - Implement the emulation contract for the new port in `HandlePortIoInstruction` and add the target port to the allow-list in `IsPortIoTraceCandidate`.
