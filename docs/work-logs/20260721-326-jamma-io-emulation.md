# 20260721-326-jamma-io-emulation

## 작업 결과 요약 (Summary of Work Results)

* **목표 달성**: 펌프 잇 업 아케이드 기체의 하드웨어 포트(JAMMA I/O) 입력을 Win32 키보드 입력과 연결하는 에뮬레이션 구현을 성공적으로 완료했습니다.
* **구현 세부사항**:
  - `src/platform/win32/io/port_io_emulator.cpp` 에 `ReadJammaPort8` 도우미 함수를 신규 구현하여 Win32 `GetAsyncKeyState` API를 폴링하도록 하였습니다.
  - Active-Low 방식의 JAMMA 하드웨어 스펙에 맞춰 포트별 맵핑(0x02A8: P1 Pad, 0x02A9: System, 0x02AA: P2 Pad)을 MAME(`xtom3d.cpp`) 사양과 일치시켰습니다.
  - x86의 8-bit(AL), 16-bit(AX), 32-bit(EAX) 포트 IN 명령어 크기에 완벽히 대응하기 위해 `HandlePortIoInstruction` 내부 로직을 동적 바이트 읽기 루프로 리팩토링했습니다.
* **검증 완료**: `aot-dynamic` 백엔드 환경에서 60초간의 타임아웃 검증을 실행하여 키보드 폴링 부하로 인한 에러나 안정성 저하 없이 구동됨을 확인했습니다.

---

## 작업 결과 세부사항 (Detailed Work Results)

### 키 맵핑 (Key Mappings)
* **P1 Pad (`0x02A8`)**: Q(Top-Left), E(Top-Right), S(Center), Z(Bottom-Left), C(Bottom-Right)
* **P2 Pad (`0x02AA`)**: Home(Top-Left), PgUp(Top-Right), Numpad 5(Center), End(Bottom-Left), PgDn(Bottom-Right)
* **System (`0x02A9`)**: F5(Coin), F2(Service), F1(Test/Clear)

### 검증 과정
정적 빌드 완료 후 `pumpit1` 타겟을 60초 한도로 구동했습니다. `GetAsyncKeyState` 호출이 exception-driven HLE 후킹 루틴에서 초당 수천 회 수행됨에도 불구하고 게임 로직이나 CPU 타이머가 멈추지 않고 타임아웃까지 유연하게 도달하는 것을 증명했습니다.

---

## Summary of Work Results (English)

* **Goal Achieved**: Successfully implemented Keyboard-to-JAMMA I/O mapping for *Pump It Up* hardware.
* **Implementation Details**:
  - Introduced `ReadJammaPort8` helper in `port_io_emulator.cpp` to poll real-time states using Win32 `GetAsyncKeyState`.
  - Mapped keys dynamically into active-low bitmasks for P1 Pad (`0x02A8`), System (`0x02A9`), and P2 Pad (`0x02AA`) according to hardware specs and MAME.
  - Upgraded `HandlePortIoInstruction` to handle 8-bit, 16-bit, and 32-bit variable `IN` widths seamlessly.
* **Verification Completed**: A 60-second execution test under the `aot-dynamic` backend confirmed that the polling hook introduces no hangs or instability.
