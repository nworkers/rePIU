# 20260809-451 PIU10 ISA 보드 HLE 작업 로그 / PIU10 ISA Board HLE Work Log

설계: [20260809-451-piu10-isa-board-hle.md](../design/20260809-451-piu10-isa-board-hle.md)

작업 지시: [20260809-451-piu10-isa-board-hle.md](../work-orders/20260809-451-piu10-isa-board-hle.md)

## 한국어

### 결과

- `hle::Piu10IsaBoard`를 추가해 `0x02D0..0x02DF`의 20-bit 주소, 12-bit 목적지,
  `piu10.u8` read-only word access와 자동 증가를 모델링했습니다.
- CAT702 PIU select·clock·data 상태 전이와 8-byte target transform을 공용 장치 안에
  구현했습니다. transform은 코드에 내장하지 않고 `<target>.cat702` ZIP entry에서 읽습니다.
- Win32 실행 준비가 기존 CRC 검증 ZIP extractor로 `piu10.u8`과 CAT702 자산을 읽어 장치를
  초기화합니다. 현재 `pumpito`, `pumpitc`, `pumpitpc`, `pumpite` ZIP 모두 두 entry를
  포함함을 확인했습니다.
- Win32 port adapter는 `0x02D0..0x02DF`의 16-bit IN/OUT을 전용 장치로 보내며 원본
  명령을 NOP 패치하지 않습니다. 다른 width와 unavailable 장치는 fail-closed입니다.
- MAME BSD-3-Clause 파생 알고리즘의 출처와 license notice를 기록했습니다.

### 검증

1. `cmd /c scripts\\build_win32_x86.bat`: 전체 Win32 x86 Debug 빌드 성공.
2. `repiu_aot_probe.exe MASTER\\PIU_1ST\\PIU\\PIU.EXE`: 전체 probe 성공.
   새 결과는 `piu10_isa_board_probe=true,destination=0x8,value=0x7`입니다.
3. `REPIU_EXECUTION_TIMEOUT_MS=3000 repiu.exe pumpito`: exception 없이 timeout까지 실행,
   port I/O input/output/handled/unhandled=`1/31/32/0`. 마지막 `0x02DA` write는
   `emulated-piu10-write`입니다.
4. 10초 `pumpito` 실행: `BGA\\00.DAT` open/read까지 진행하고 timeout됐습니다. 기존
   `0x0402106D` `IN AX,DX`/`0x02DA` blocker는 재현되지 않았습니다.

첫 빌드 검증에서는 앞서 timeout된 두 build invocation이 background에 남아 같은 PDB를
동시에 기록해 MSVC C1041이 발생했습니다. 해당 orphan build process를 종료하고 단일
invocation으로 다시 실행하여 성공했습니다. 코드 결함으로 발생한 실패는 아닙니다.

### 남은 범위

MP3 decoder의 실제 decode/audio 출력, rendering, input과 전체 gameplay는 별도 검증이
필요합니다. 현재 PIU10 MP3 상태는 reset-ready 신호를 제공합니다.

### Task 452 범위 보정

Task 452에서 PIU10 flash/CAT702 초기화와 `0x02D0..0x02DF` 가로채기를 명시적인
target-profile capability 뒤로 제한했습니다. 보드는 `pumpito`, `pumpitc`, `pumpitpc`,
`pumpite`에만 활성화됩니다. Task 451의 최초 구현은 ROM ZIP이 있는 모든 target에
적용되었으며, `pumpit1`, `pumpit2`, `pumpit3`에는 필요하지 않았습니다. YMZ280B
초기화는 이 capability와 독립적으로 유지됩니다.

## English

### Result

- Added `hle::Piu10IsaBoard`, modeling the `0x02D0..0x02DF` 20-bit address, 12-bit
  destination, read-only `piu10.u8` word access, and auto-increment.
- Implemented CAT702 PIU select, clock, and data transitions with an eight-byte target transform.
  The transform is loaded from `<target>.cat702`, not embedded in code.
- Win32 execution setup initializes the device with the existing CRC-verifying ZIP extractor.
  The current `pumpito`, `pumpitc`, `pumpitpc`, and `pumpite` ZIPs all contain both entries.
- The Win32 port adapter forwards 16-bit accesses without NOP-patching the original instruction;
  other widths and an unavailable device fail closed.
- Recorded provenance and the BSD 3-Clause notice for MAME-derived algorithms.

### Verification

1. `cmd /c scripts\\build_win32_x86.bat`: complete Win32 x86 Debug build passed.
2. `repiu_aot_probe.exe MASTER\\PIU_1ST\\PIU\\PIU.EXE`: full probe suite passed, including
   `piu10_isa_board_probe=true,destination=0x8,value=0x7`.
3. A three-second `pumpito` run timed out without an exception and reported port I/O
   input/output/handled/unhandled=`1/31/32/0`; its last `0x02DA` write was
   `emulated-piu10-write`.
4. A ten-second run progressed through opening and reading `BGA\\00.DAT`. The former
   `0x0402106D` `IN AX,DX`/`0x02DA` blocker did not recur.

The first build check encountered MSVC C1041 because two earlier timed-out build invocations
continued in the background and wrote the same PDB concurrently. After stopping those orphaned
build processes, a single invocation passed; this was not a code defect.

### Remaining Scope

Actual MP3 decoding/audio output, rendering, input, and full gameplay require separate
verification. The current PIU10 MP3 status reports reset-ready signals.

### Task 452 Scope Correction

Task 452 subsequently placed PIU10 flash/CAT702 setup and `0x02D0..0x02DF` interception behind
an explicit target-profile capability. The board is enabled only for `pumpito`, `pumpitc`,
`pumpitpc`, and `pumpite`; the initial Task 451 implementation had applied it to every target
with a ROM ZIP. YMZ280B initialization remains independent.
