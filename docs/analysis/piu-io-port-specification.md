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
| `0x02A0` | 8/16/32-bit | Read / Write | YMZ280B 칩 오프셋 0. 쓰기=레지스터 번호 선택, 읽기=외부 메모리 readback 래치 | `ymz280b_device` read/write offset 0 |
| `0x02A1` | — | — | 미디코드(16비트 워드의 상위 바이트). 쓰기는 버림, 읽기는 `0xFF` | `umask16(0x00ff)`에 의해 라우팅 제외 |
| `0x02A2` | 8/16/32-bit | Read / Write | YMZ280B 칩 오프셋 1. 쓰기=선택된 레지스터에 데이터, 읽기=상태 레지스터 | `ymz280b_device` read/write offset 1 |
| `0x02A3` | — | — | 미디코드. 쓰기는 버림, 읽기는 `0xFF` | `umask16(0x00ff)`에 의해 라우팅 제외 |
| `0x02A8` | 8-bit | Read | P1 (1인용) 발판 센서 입력 (Up-Left, Up-Right, Center, Down-Left, Down-Right) | `P1 Inputs` read |
| `0x02A9` | 8-bit | Read | 시스템 입력 (코인 센서, 서비스 버튼, 테스트 버튼, 스타트 등) | `System Inputs` read |
| `0x02AA` | 8-bit | Read | P2 (2인용) 발판 센서 입력 | `P2 Inputs` read |
| `0x02AB` | 8-bit | Read | 캐비닛 기타 입력 (보조 코인기, 캐비닛 특수 신호 등) | `Cabinet Inputs` read |
| `0x02AC` | 8-bit | Write | 93C46 Serial EEPROM 제어선 쓰기<br>- Bit 0: Chip Select (CS)<br>- Bit 1: Clock (CLK)<br>- Bit 2: Data Input (DI) | `m_eeprom->clk_write / cs_write / di_write` |
| `0x02AE` | 8-bit | Read | 93C46 Serial EEPROM 데이터 읽기<br>- Bit 0: Data Output (DO) 상태 반환 (읽기 결과: `do_read() \| 0xfe`) | `m_eeprom->do_read() \| 0xfe` |

### 2.1 시스템 입력 비트와 호스트 키 정책 (Task 361)
### 2.1 System input bits and host-key policy (Task 361)

`0x02A9`는 active-low 시스템 입력 포트다. MAME `pumpitup` 정의로 확인된
TEST, COIN1, CLEAR 비트와 rePIU의 SERVICE 호환 매핑은 다음과 같다.
SERVICE `0x40`은 MAME 원본에서 기능이 확정되지 않은 비트이므로, 이 표에서는
사용자 지정 호스트 호환 정책으로 구분한다.

`0x02A9` is the active-low system input port. The TEST, COIN1, and CLEAR bits
confirmed by MAME's `pumpitup` definition and rePIU's SERVICE compatibility
mapping are listed below. MAME does not identify `0x40` as a confirmed service
input, so the table explicitly classifies it as a user-selected host
compatibility policy.

| 마스크 / Mask | rePIU 기능 / Function | 호스트 키 / Host key | 상태 / Status |
|---:|---|---|---|
| `0x02` | TEST | F1 | MAME 정의 확인 / Confirmed by MAME |
| `0x04` | COIN1 | F5 | MAME 정의 확인 / Confirmed by MAME |
| `0x40` | SERVICE | F2 | rePIU 호환 정책 / rePIU compatibility policy |
| `0x80` | CLEAR | F3 | MAME `pumpitup` 정의 확인 / Confirmed by MAME `pumpitup` |

released 상태는 `0xFF`이며, 키를 누르면 해당 마스크가 0으로 내려간다. 따라서
단독 입력의 예상값은 F1=`0xFD`, F2=`0xBF`, F3=`0x7F`, F5=`0xFB`이다.

The released value is `0xFF`; pressing a key clears its mask. Expected values
for individual presses are therefore F1=`0xFD`, F2=`0xBF`, F3=`0x7F`, and
F5=`0xFB`.

### 2.2 2P NumLock OFF 숫자패드 정책 (Task 362)
### 2.2 P2 NumLock-off numeric-keypad policy (Task 362)

2P 발판은 NumLock OFF 상태의 숫자패드 7, 9, 5, 1, 3을 사용한다. Win32는
이 상태의 키를 각각 `VK_HOME`, `VK_PRIOR`, `VK_CLEAR`, `VK_END`, `VK_NEXT`로
보고한다. `VK_CLEAR`는 숫자패드 5의 가상키 이름이며 시스템 입력 CLEAR와는
관련이 없다.

The P2 stage uses numeric-keypad 7, 9, 5, 1, and 3 with NumLock off. Win32
reports these keys as `VK_HOME`, `VK_PRIOR`, `VK_CLEAR`, `VK_END`, and
`VK_NEXT`, respectively. `VK_CLEAR` is the virtual-key name for keypad 5 in
this mode and is unrelated to the system CLEAR input.

| `0x02AA` mask | P2 위치 / Position | NumLock OFF 숫자패드 / Keypad | Win32 가상키 / Virtual key |
|---:|---|---:|---|
| `0x01` | Top-Left | 7 | `VK_HOME` |
| `0x02` | Top-Right | 9 | `VK_PRIOR` |
| `0x04` | Center | 5 | `VK_CLEAR` |
| `0x08` | Bottom-Left | 1 | `VK_END` |
| `0x10` | Bottom-Right | 3 | `VK_NEXT` |

### 2.3 사운드 창의 ISA 바이트 레인 디코드 (Task 290에서 확정)
### 2.3 ISA byte-lane decode of the sound window (confirmed in Task 290)

MAME io_map은 `map(0x00, 0x03).rw("ymz", read, write).umask16(0x00ff)`입니다. 16비트
버스에서 `umask16(0x00ff)`는 **각 16비트 워드의 하위 바이트만** 라우팅한다는 뜻이므로,
칩이 보이는 오프셋은 바이트 주소의 절반입니다. 짝수 포트만 디코드되고 홀수 포트는
미디코드로 남습니다.

이 규칙이 폭에 무관하게 성립하는 것이 중요합니다. 게스트가 `OUT DX, EAX`(width 4)로
`0x02A0`에 쓰면 4개의 바이트 레인으로 분해되어 레지스터 번호 1회와 데이터 1회가
연속 수행됩니다. 예전에 `unsupported`로 분류하던 `0x02A0 ← 0x00000010`,
`0x02A0 ← 0x00000001`, `0x02A2 ← 0x00000000` 트레이스는 모두 이 형태의 정상적인
YMZ280B 레지스터 프로그래밍이었습니다.

The MAME io_map is `map(0x00, 0x03).rw("ymz", read, write).umask16(0x00ff)`. On a
16-bit bus, `umask16(0x00ff)` routes **only the low byte of each 16-bit word**, so
the chip-visible offset is half the byte address: even ports decode and odd ports
stay undecoded.

The rule holds regardless of access width, which is what matters. A guest
`OUT DX, EAX` (width 4) to `0x02A0` decomposes into four byte lanes and performs one
register-number write followed by one data write. The traces previously classified
as `unsupported` — `0x02A0 <- 0x00000010`, `0x02A0 <- 0x00000001`, and
`0x02A2 <- 0x00000000` — were all ordinary YMZ280B register programming of exactly
this shape.

레지스터 의미와 주소 단위는 [docs/kb/ymz280b-pcm-adpcm-decoder.md](../kb/ymz280b-pcm-adpcm-decoder.md)에
정리했습니다.
Register semantics and address units are documented in
[docs/kb/ymz280b-pcm-adpcm-decoder.md](../kb/ymz280b-pcm-adpcm-decoder.md).

---

## 3. 핵심 HLE 에뮬레이션 전략
## 3. Key HLE Emulation Strategy
1. **포트 입력(Read) 에뮬레이션**:
   - `0x02A8 ~ 0x02AB` (입력): 실기 캐비닛 발판 센서는 기본적으로 풀업 저항 상태인 Active-Low 신호를 가집니다. 따라서 기본값으로 `0xFFU`(또는 `0xFFFFU`)를 채워 반환해야 입력 루프 폴링에서 정상 대기 상태를 유지합니다. 입력 레지스터는 게임이 매 프레임 폴링하므로 EEPROM 읽기와 마찬가지로 **NOP 패치 없이 매번 EIP를 전진시켜 재트랩**해야 합니다. NOP 패치로 원본 IN 명령을 덮어쓰면 최초 1회만 실제 키 상태가 반영되고 이후 press/release가 무시되며, EAX가 idle(`0xFF`)로 갱신되지 않아 무입력이 눌림으로 오인됩니다 (Task 327에서 수정).
   - `0x02AE` (EEPROM 읽기): 게임 엔진은 저장된 설정값을 가져오기 위해 이 포트에서 DO 비트 변화를 감지합니다. `Eeprom93c46` 클래스의 16-bit Microwire 프로토콜 상태 머신을 거쳐 출력되는 데이터 비트를 반환합니다. 이전과 달리 NOP 패치 없이 매번 EIP를 전진시켜 명령을 평가합니다.
   - `0x0040` (PIT 카운터 읽기): 레거시 PC 환경의 타이머 폴링으로 인한 지연을 차단하기 위해, 읽기 요청 시 일정한 간격의 dummy counter 또는 dynamic tick 값을 반환하도록 에뮬레이트합니다.
   - `0x02A0`/`0x02A2` (YMZ280B 읽기): 각각 외부 메모리 readback 래치와 상태 레지스터를 반환합니다. 사운드 창은 JAMMA 입력 범위(`0x02A0`~`0x02AF`) 안에 있으므로 **입력 분기보다 먼저** 가로채야 합니다. 그렇지 않으면 입력 폴백이 항상 `0xFF`를 돌려주어 상태 레지스터가 무의미해집니다.
2. **포트 출력(Write) 에뮬레이션**:
   - `0x02A0`/`0x02A2` (YMZ280B 쓰기): 바이트 레인으로 분해해 레지스터 선택과 데이터 기록으로 전달합니다. **절대 NOP 패치하지 않습니다.** 사운드 레지스터는 재생마다 수십~수백 회 다시 프로그래밍되므로, 최초 1회 후 원본 `OUT`을 NOP으로 덮으면 그 시점 이후 영구 무음이 됩니다. EEPROM·JAMMA 경로와 같은 이유로 EIP만 전진시키고 매번 재트랩합니다 (Task 290).
   - `0x02AC` (EEPROM 쓰기): EEPROM에 전송하는 제어 신호(CS, CLK, DI)를 `Eeprom93c46` 상태 머신으로 전달하여 `eeprom.dat` 파일에 설정값을 지속적으로 저장/반영합니다. NOP 패치 없이 매번 트랩되어 상태를 갱신합니다.
   - `0x0043` (PIT 제어 쓰기): PC 스피커 톤 생성 및 타이머 주파수 조정을 위해 시스템 코드가 수행하는 PIT 제어 쓰기는 에뮬레이터 구동 안정성을 위해 `unsupported-ignored` 로그 기록을 남긴 뒤 무시 처리합니다.

1. **Port Read Emulation**:
   - `0x02A8 ~ 0x02AB` (Inputs): Real cabinet sensors use active-low signals. Returning default pulled-up values like `0xFFU` (or `0xFFFFU`) prevents hang on sensor loops. Because the game polls these registers every frame, the input read — like the EEPROM read — must **advance EIP and re-trap each time rather than NOP-patch** the guest IN. NOP-patching latches only the first sample, ignores later press/release transitions, and leaves EAX un-refreshed so idle is misread as pressed (fixed in Task 327).
   - `0x02AE` (EEPROM Read): The engine queries this port to fetch persistent configurations. It returns the data bit outputted from the `Eeprom93c46` 16-bit Microwire protocol state machine. The instruction is evaluated dynamically without NOP patching by manually advancing EIP.
   - `0x0040` (PIT Counter Read): To prevent hang during timer calibration polling, return dummy ticks or dynamic values.
   - `0x02A0`/`0x02A2` (YMZ280B reads): return the external memory readback latch and the status register respectively. The sound window sits inside the JAMMA input range (`0x02A0`–`0x02AF`), so it must be intercepted **before** the input branch; otherwise the input fallback answers `0xFF` and the status register becomes meaningless.
2. **Port Write Emulation**:
   - `0x02A0`/`0x02A2` (YMZ280B writes): decomposed into byte lanes and forwarded as register select and register data. **Never NOP-patched.** Sound registers are reprogrammed dozens to hundreds of times per playback, so overwriting the original `OUT` with NOPs after the first execution means permanent silence from that point on. As on the EEPROM and JAMMA paths, EIP advances and the instruction re-traps each time (Task 290).
   - `0x02AC` (EEPROM Write): Writes to EEPROM control lines (CS, CLK, DI) are routed to the `Eeprom93c46` state machine, which persists configurations into `eeprom.dat`. It is continuously trapped without NOP patching.
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

## 5. 입력 포트 읽기 비용과 타이틀별 빈도 (Task 402 측정)

### 확인됨: 게스트는 입력 포트 읽기를 지연 수단으로 쓴다

pumpit3의 폴링 루틴 `0x0301DB10`은 포트 `0x02A8`을 **200회 연속으로 읽고 값을 전부
버립니다**(매 반복 `sub eax,eax`). 읽은 데이터가 목적이 아니라 멀티플렉서 strobe 사이의
settle 시간을 버는 것이 목적입니다. 실기에서는 ISA 버스 사이클 한 번이 곧 지연이므로
이것이 자연스러운 관용구입니다.

이 루프는 반복 횟수가 200으로 **고정**이며, `INT 21h AH=2Ch` 지연 루틴과 달리
자기 보정되지 않습니다. 따라서 호출당 비용을 줄이면 wall time이 그대로 줄어듭니다.

### 확인됨: 호출당 비용의 근인은 `ReadJammaPort8`의 전수 키 조회

`src/platform/win32/io/port_io_emulator.cpp`의 `ReadJammaPort8`은 포트 **1바이트마다**
`GetAsyncKeyState`를 최대 5회 호출합니다. `0x02A8`의 16비트 읽기는 2바이트를 합성하므로
`in ax,dx` 한 번에 최대 **10회**입니다.

측정된 핸들러 본체 비용은 호출당 약 16,000~29,000 cycle이며, 이는 Glide gate 호출
(약 78,000 cycle)의 1/3 수준입니다. 포트 읽기 하나가 그만큼 비쌀 이유는 없습니다.

### 확인됨: 빈도는 타이틀마다 크게 다르다

각 45초, `aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1` 기준입니다.

| 타겟 | 프레임 | port I/O 호출 | 초당 | 배수 | port-io 비중 | Glide gate 비중 | 호출당 cycle |
|---|---:|---:|---:|---:|---:|---:|---:|
| pumpit1 | 2,222 | 25,091 | 558 | 1.0x | 0.42% | 57.20% | 27,615 |
| pumpit2 | 1,985 | 40,162 | 892 | 1.6x | 0.38% | 35.77% | 15,922 |
| **pumpit3** | **1,026** | **1,846,040** | **41,023** | **73.6x** | **30.53%** | 12.64% | 29,395 |

**호출당 비용은 세 타이틀이 같은 자릿수이므로, pumpit3의 큰 비중은 비싼 호출이 아니라
호출 횟수에서 옵니다.** pumpit3의 초당 41,023회는 200회 루프 기준 초당 약 205회 폴링이며,
게스트가 프로그램한 PIT 240Hz와 같은 자릿수입니다.

**정적 개수로는 이 차이를 알 수 없습니다.** 200회 지연 루프(`cmp ebx,0xC8; jl`)는
pumpit1 4곳(`0x44E03` `0x450C7` `0x45377` `0x45627`), pumpit2 4곳, pumpit3 1곳
(`0x28D24`)으로, 오히려 pumpit1/pumpit2에 더 많습니다.

### 유지 지침

입력 포트 읽기 비용을 줄일 때 지켜야 할 제약은 Task 327이 확정한 것입니다: **매 폴링마다
EIP를 전진시켜 재트랩해야 하며 NOP 패치로 대체하면 안 됩니다.** 최초 1회만 반영되면
press/release 전이가 무시되고 무입력이 눌림으로 오인됩니다.

이 제약과 양립하는 방향은 트랩 자체를 없애는 것이 아니라 **호스트 키보드 전수 조회를
매 읽기마다 하지 않는 것**입니다. 입력 스냅샷을 프레임 또는 타이머 틱 주기로 갱신하고
포트 읽기는 스냅샷을 소비하면, 스냅샷 주기가 게스트 폴링 주기보다 촘촘한 한 전이는
보존됩니다.

### 미확정

- 호출당 약 29,000 cycle 중 `GetAsyncKeyState`가 차지하는 비중은 직접 계측하지
  않았습니다. `RecordPortIo`가 `const std::string&`를 받아 호출마다 문자열을 만드는
  비용도 분해되지 않았습니다.
- 위 세 실행은 **같은 장면이 아닙니다.** 각 타이틀의 부팅 후 45초일 뿐이며, Tasks
  363~368이 확인했듯 장면이 달라지면 같은 비용 집합도 몇 배씩 움직입니다.
- pumpit1의 200회 루프 4곳이 런타임에 실행되기는 하는지, 아니면 낮은 빈도가 그 경로를
  타지 않기 때문인지 분해하지 않았습니다.

## 5. Input-port read cost and per-title frequency (measured in Task 402)

### Confirmed: the guest uses input-port reads as a delay

pumpit3's polling routine at `0x0301DB10` reads port `0x02A8` **200 times in a row and
discards every value** (`sub eax,eax` each iteration). The data is not the point; the settle
time between multiplexer strobes is. On real hardware one ISA bus cycle *is* the delay, which
makes this a natural idiom.

The loop has a **fixed** 200 iterations and, unlike the `INT 21h AH=2Ch` delay routine, is
not self-calibrating, so reducing per-call cost reduces wall time directly.

### Confirmed: the per-call cost comes from a full key scan in `ReadJammaPort8`

`ReadJammaPort8` in `src/platform/win32/io/port_io_emulator.cpp` calls `GetAsyncKeyState` up
to five times **per port byte**. A 16-bit read at `0x02A8` composes two bytes, so a single
`in ax,dx` performs up to **ten**.

Measured handler-body cost is roughly 16,000-29,000 cycles per call — about a third of a
Glide gate call (~78,000 cycles). A port read has no reason to be that expensive.

### Confirmed: frequency differs sharply by title

45 seconds each, `aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1`:

| Target | Frames | Port I/O calls | Per second | Ratio | Port-io share | Glide gate share | Cycles/call |
|---|---:|---:|---:|---:|---:|---:|---:|
| pumpit1 | 2,222 | 25,091 | 558 | 1.0x | 0.42% | 57.20% | 27,615 |
| pumpit2 | 1,985 | 40,162 | 892 | 1.6x | 0.38% | 35.77% | 15,922 |
| **pumpit3** | **1,026** | **1,846,040** | **41,023** | **73.6x** | **30.53%** | 12.64% | 29,395 |

**Per-call cost is the same order across titles, so pumpit3's large share comes from call
count, not from more expensive calls.** Its 41,023 reads per second is about 205 polls per
second at 200 reads each, the same order as the guest-programmed 240 Hz PIT rate.

**Static counts cannot reveal this difference.** The 200-iteration delay loop
(`cmp ebx,0xC8; jl`) appears four times in pumpit1 (`0x44E03`, `0x450C7`, `0x45377`,
`0x45627`), four times in pumpit2, and once in pumpit3 (`0x28D24`) — more in pumpit1 and
pumpit2, not fewer.

### Maintenance guidance

The constraint to preserve when reducing input-port read cost is the one Task 327 established:
**advance EIP and re-trap on every poll; never NOP-patch the guest `IN`.** Latching only the
first sample ignores later press/release transitions and misreads idle as pressed.

The direction compatible with that constraint is not removing the trap but **not rescanning
the host keyboard on every read**: refresh an input snapshot per frame or timer tick and have
port reads consume it. Transitions are preserved as long as the snapshot cadence is finer than
the guest's polling cadence.

### Unresolved

- How much of the ~29,000 cycles per call is `GetAsyncKeyState` was not measured directly,
  nor was the cost of `RecordPortIo` taking a `const std::string&` and constructing a string
  per call.
- The three runs above are **not the same scene** — each is a title's first 45 seconds — and
  Tasks 363-368 showed scene choice moves these figures several-fold.
- Whether pumpit1's four 200-iteration loops execute at runtime at all, or whether its low
  frequency means that path is not taken, was not decomposed.

### 5.1 확인됨: 호출당 비용은 사실상 전부 `GetAsyncKeyState` (Task 403 분해)

Task 402가 미확정으로 남긴 "호출당 약 29,000 cycle 중 `GetAsyncKeyState` 비중"을
직접 계측했습니다. `ReadJammaPort8` 루프에 rdtsc 구간을 두고 `GetAsyncKeyState` 호출을
셌습니다(pumpit3, 45초, `aot-dbt`).

| 항목 | 값 | 비중 |
|---|---:|---:|
| wall cycles | 166,599,116,119 | 100% |
| port I/O 핸들러 본체 | 51,474,994,231 | 30.90% of wall |
| └ JAMMA scan 루프 | 51,070,572,912 | **99.21% of port-io body** |
| └ → wall 대비 | | **30.65%** |
| scan 수 | 1,868,147 | 초당 41,514 |
| `GetAsyncKeyState` 호출 | 16,776,443 | 초당 372,810 |
| scan당 key query | **8.98** | |
| **key query당 cycle** | **3,044** | 약 0.82µs |

**9 × 3,044 = 27,396 cycle이고 실측 scan당 27,337 cycle이므로, key query가 scan 비용의
100.2%를 설명합니다.** 디코드, `RecordPortIo`, 문자열 생성 등 나머지는 측정 오차 범위
안입니다.

따라서 **port I/O 비용의 근인은 `GetAsyncKeyState` 하나**이며, scan 비용을 0으로 만들 때의
상한은 `1 / (1 - 0.3065) = 1.44배`입니다. 커널 예외 왕복은 Task 327 규칙상 그대로
남으므로 상한에 포함되지 않습니다.

### 5.2 정확도를 유지하면서 호출을 줄이는 조건

**게스트가 관측할 수 있는 주기가 상한입니다.** pumpit3는 초당 41,514회 읽지만 그중
200회가 하나의 폴링 안에서 값을 버리는 지연이므로, 실제 폴링은 초당 약 208회
(약 4.8ms)입니다. 한 폴링 안의 200회 읽기는 약 1.5ms에 걸쳐 있고 그 사이 사람 손가락의
전이는 일어나지 않습니다.

따라서 **스냅샷 갱신 주기가 게스트 폴링 주기보다 촘촘하면 게스트가 볼 수 있었던 전이는
하나도 잃지 않습니다.** 갱신 주기 1ms 기준:

| | 현재 | 스냅샷 1ms |
|---|---:|---:|
| `GetAsyncKeyState` 초당 호출 | 372,810 | 약 14,000 (키 14개 × 1,000) |
| 감소 | | 약 26배 |
| 추가 입력 지연 최악 | 0 | 1ms (게스트 폴링 4.8ms의 1/5) |

Task 327 제약은 그대로 지켜집니다. **트랩을 없애는 것이 아니라 트랩 안에서 호스트
키보드를 전수 조회하지 않는 것**이므로, 게스트 `IN`은 매번 실행되고 EIP는 매번
전진하며 NOP 패치도 하지 않습니다.

캐시해도 안전한 근거가 하나 더 있습니다. 현재 코드는 `GetAsyncKeyState`의 반환에서
`0x8000`(현재 눌림 상태)만 사용하고 **"마지막 호출 이후 눌린 적 있음" 비트(`0x0001`)는
쓰지 않습니다.** 그 비트는 호출이 소비하는 상태이므로 캐시하면 의미가 달라지지만,
쓰지 않으므로 호출 횟수를 줄여도 관측 의미가 바뀌지 않습니다.

**캐시 대상은 JAMMA 키 스캔으로 한정합니다.** EEPROM 포트와 사운드 포트는 상태를 가지며
읽기마다 부작용이 있으므로 캐시하면 안 됩니다.

### 5.1 Confirmed: per-call cost is essentially all `GetAsyncKeyState` (Task 403)

The item Task 402 left unresolved — how much of the ~29,000 cycles per call is
`GetAsyncKeyState` — was measured directly by timing the `ReadJammaPort8` loop with rdtsc
and counting the calls (pumpit3, 45 seconds, `aot-dbt`).

| Metric | Value | Share |
|---|---:|---:|
| Wall cycles | 166,599,116,119 | 100% |
| Port I/O handler body | 51,474,994,231 | 30.90% of wall |
| ��� JAMMA scan loop | 51,070,572,912 | **99.21% of port-io body** |
| └ against wall | | **30.65%** |
| Scans | 1,868,147 | 41,514/s |
| `GetAsyncKeyState` calls | 16,776,443 | 372,810/s |
| Key queries per scan | **8.98** | |
| **Cycles per key query** | **3,044** | ~0.82 µs |

**9 × 3,044 = 27,396 cycles against a measured 27,337 per scan, so key queries account for
100.2% of the scan cost.** Decode, `RecordPortIo`, and string construction fall within
measurement noise.

The root cause of port I/O cost is therefore `GetAsyncKeyState` alone, and driving the scan
cost to zero caps the gain at `1 / (1 - 0.3065) = 1.44x`. The kernel exception round trip
stays, since the Task 327 rule keeps the trap.

### 5.2 Conditions for calling it less without losing accuracy

**The guest's own observation period is the ceiling.** pumpit3 issues 41,514 reads per second,
but 200 of every batch are a value-discarding delay, so it actually polls about 208 times per
second (~4.8 ms). The 200 reads in one poll span roughly 1.5 ms, within which no human key
transition occurs.

**A snapshot refreshed faster than the guest polls therefore loses no transition the guest
could have observed.** At a 1 ms refresh:

| | Now | 1 ms snapshot |
|---|---:|---:|
| `GetAsyncKeyState` per second | 372,810 | ~14,000 (14 keys × 1,000) |
| Reduction | | ~26x |
| Worst-case added input latency | 0 | 1 ms (a fifth of the guest's 4.8 ms poll) |

The Task 327 constraint still holds: this does not remove the trap, it stops the trap from
rescanning the host keyboard, so the guest `IN` still executes, EIP still advances, and
nothing is NOP-patched.

One more reason caching is safe: the current code uses only the `0x8000` bit (current physical
state) and **never the `0x0001` "pressed since last call" bit**. That bit is state a call
consumes, so caching would change its meaning — but it is unused, so reducing call frequency
does not change what is observed.

**Only the JAMMA key scan may be cached.** The EEPROM and sound ports carry state with
per-read side effects and must not be.

### 5.3 스냅샷 도입 결과 (Task 403 구현 + A/B)

`ReadJammaPort8`을 `ScanJammaPort8`(실제 조회)과 스냅샷 조회로 분리하고, 갱신 주기를
기본 500µs로 제한했습니다. `REPIU_JAMMA_SNAPSHOT=0`이면 매 읽기 조회로 되돌아갑니다.

pumpit3 45초 각 6회, `REPIU_EEPROM_PATH`로 EEPROM을 격리한 A/B입니다.

| | 렌더 도달 | 프레임(정렬) | port-io 비중 중앙값 | key query/s 중앙값 |
|---|---|---|---:|---:|
| off | 4/6 | 0, 0, 867, 867, 867, 1106 | 22.40% | 285,425 |
| on | 3/6 | 0, 0, 0, 867, 1340, 1389 | **2.75%** | **20,080** |

**확인됨: 비용 제거.** key query **14.2배** 감소, port-io wall 비중 22.40% → 2.75%.

**미확정: 프레임 개선.** 렌더 도달 실행만의 중앙값은 867 → 1,340이지만 표본이 6회씩이고
결과가 0/약1,300으로 양극화되어 있습니다. 특히 **867이 여러 실행에서 정확히 반복**되므로
이 값은 처리량 상한이 아니라 장면 경계일 가능성이 높고, 그렇다면 프레임 수는 throughput
지표가 아닙니다.

**미확정: 렌더링 도달 자체가 비결정적.** 두 arm 모두 6회 중 2~3회가 45초 안에 렌더링에
도달하지 못했습니다. 스냅샷과 무관한 기존 문제이며, 해소 전에는 프레임 기반 성능 판정을
신뢰할 수 없습니다.

**회귀 없음.** pumpit1 2,251 프레임(이전 2,222), pumpit2 2,204(이전 1,985).

**측정 절차 주의.** `eeprom.dat`는 git에 추적되지 않는 영속 상태입니다. 격리하지 않은
첫 A/B는 무효였고, 매 실행마다 고정 fixture를 복사해 `REPIU_EEPROM_PATH`로 지정한 뒤에야
신호가 나왔습니다. `scripts/benchmark_*.ps1`이 쓰는 방식과 같습니다.

### 5.3 Snapshot result (Task 403 implementation and A/B)

`ReadJammaPort8` was split into `ScanJammaPort8` (the real query) and a snapshot lookup with a
default 500 µs refresh bound; `REPIU_JAMMA_SNAPSHOT=0` restores per-read querying.

Six 45-second pumpit3 runs per arm with the EEPROM isolated through `REPIU_EEPROM_PATH`:

| | Reached rendering | Frames (sorted) | Port-io share median | Key queries/s median |
|---|---|---|---:|---:|
| off | 4/6 | 0, 0, 867, 867, 867, 1106 | 22.40% | 285,425 |
| on | 3/6 | 0, 0, 0, 867, 1340, 1389 | **2.75%** | **20,080** |

**Confirmed: the cost is removed.** Key queries fell **14.2x** and the port-io wall share went
from 22.40% to 2.75%.

**Unresolved: whether frames improve.** The median among rendering runs moves 867 to 1,340, but
with six runs per arm, outcomes polarised at 0 or ~1,300, and the value **867 recurring exactly**
across runs — which suggests a scene boundary rather than a throughput ceiling, making frame
count a "how far did it get" measure rather than throughput.

**Unresolved: reaching rendering is itself nondeterministic.** Two to three of six runs in both
arms never rendered within 45 seconds. This predates the snapshot, and until it is understood,
frame-based performance judgements cannot be trusted.

**No regression.** pumpit1 rendered 2,251 frames (2,222 before) and pumpit2 2,204 (1,985 before).

**Measurement caveat.** `eeprom.dat` is untracked persistent state. The first A/B run without
isolating it was invalid; a usable signal appeared only after copying a fixed fixture per run
and pointing `REPIU_EEPROM_PATH` at it, the method `scripts/benchmark_*.ps1` already use.
