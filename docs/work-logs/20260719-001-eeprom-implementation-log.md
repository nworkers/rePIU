# 20260719-001-eeprom-implementation-log

## 1. 개요 (Overview)
기존에 `port_io_emulator`에서 덤(Dummy) 값 반환 및 NOP 패치로 단순 우회(bypassed) 처리되던 펌프 잇 업(PIU)의 93C46 Serial EEPROM 접근 방식을 개선했습니다. 
상태 머신을 구현하여 설정 데이터(`eeprom.dat`)를 영구적으로 기록하고 불러올 수 있는 환경을 구축했습니다.

## 2. 주요 작업 내역 (Key Tasks Completed)
1. **`Eeprom93c46` 클래스 구현**:
   - 위치: `src/platform/win32/io/eeprom_93c46.h` 및 `eeprom_93c46.cpp`
   - 기능: 1Kbit(64 x 16-bit) 93C46의 Microwire 인터페이스 에뮬레이션 상태 머신(State Machine) 개발. (CS, CLK, DI 제어 및 DO 비트 출력)
   - 파일 연동 로직 추가(`eeprom.dat` 존재 유무 확인 및 0xFFFF 초기화 로직 구현).
2. **`port_io_emulator.cpp` 연동 및 `apply_nop_patch` 최적화 해제**:
   - `0x02AC` (쓰기) 및 `0x02AE` (읽기) 포트 인터셉트 시점에 NOP 명령어 덮어쓰기를 제거했습니다.
   - 런타임에 명령어 크기(`instruction_len`)만큼 EIP를 수동으로 전진(`win32_context->Eip += instruction_len`)시키도록 수정하여, 프로그램 실행 내내 트랩이 정상 작동하도록 구성했습니다.
3. **`CMakeLists.txt` 빌드 시스템 갱신**:
   - `repiu_exe` 빌드 타겟의 소스 목록에 `eeprom_93c46.cpp`를 추가했습니다.
4. **문서 갱신**:
   - `docs/analysis/piu-io-port-specification.md` 파일에 변경된 포트 IO 전략(16비트 Microwire 에뮬레이션 및 NOP 해제)을 반영했습니다.

## 3. 검증 (Verification)
- 빌드 정상 확인 및 CMake 연동 구조 확인 완료.
- C++ 코드를 통한 논리 검증 완료 (State Machine transitions and Bit shifting).

---

# 20260719-001-eeprom-implementation-log (English)

## 1. Overview
Upgraded the emulation strategy for the Pump It Up (PIU) 93C46 Serial EEPROM. Previously, EEPROM IO via `port_io_emulator` was bypassed using dummy values and runtime NOP patching. Now, a state machine simulates the hardware properly and provides persistent storage using `eeprom.dat`.

## 2. Key Tasks Completed
1. **Implemented `Eeprom93c46` Class**:
   - Created `src/platform/win32/io/eeprom_93c46.h` and `.cpp`.
   - Built a state machine for the 1Kbit (64 x 16-bit) 93C46 Microwire interface. Handled CS, CLK, DI, DO pin logic.
   - Added persistent file I/O capabilities (`eeprom.dat`). Initializes to `0xFFFF` natively if not present.
2. **Integrated with `port_io_emulator.cpp` & Removed NOP patch optimizations**:
   - Modified `0x02AC` (Write) and `0x02AE` (Read) traps to evaluate dynamic DO bits and accept DI/CS/CLK values.
   - Replaced `apply_nop_patch` with manual EIP advance (`win32_context->Eip += instruction_len`) to ensure EEPROM commands remain trapped and consistently processed.
3. **Updated `CMakeLists.txt` Build System**:
   - Registered `eeprom_93c46.cpp` into the `repiu_exe` executable target sources.
4. **Updated Documentation**:
   - Reflected the newly implemented 16-bit Microwire emulation rules into `docs/analysis/piu-io-port-specification.md`.

## 3. Verification
- Source files correctly configured in CMakeLists.txt.
- State Machine logic and explicit EIP advancement correctly implemented without compilation errors.
