# 20260725-290 작업 로그: YMZ280B 사운드 에뮬레이션 / Work log: YMZ280B sound emulation

설계: [docs/design/20260725-290-ymz280b-sound-emulation.md](../design/20260725-290-ymz280b-sound-emulation.md)
지시: [docs/work-orders/20260725-290-ymz280b-sound-emulation.md](../work-orders/20260725-290-ymz280b-sound-emulation.md)

## 한국어

### 결과 요약

목표를 달성했습니다. `pumpit1` 구동 중 `F5`(COIN1)를 누르면 YMZ280B가 샘플 ROM에서
코인 효과음을 디코드해 SDL3 오디오 장치로 출력합니다. 4회 투입 → 4회 재생을
파형으로 확인했습니다.

### 구현

| 계층 | 파일 | 내용 |
| :--- | :--- | :--- |
| ZIP | `assets/rom_zip_archive.{h,cpp}` | 엔트리 1개 메모리 추출 + CRC32 검증 |
| ROM | `sound/ymz280b_sample_rom.{h,cpp}` | 4 MiB `0xFF` 공간에 `piu10.u9` 배치 |
| 칩 코어 | `sound/ymz280b_device.{h,cpp}` | MAME 레지스터 맵 이식, 8보이스, 88200 Hz 스테레오 |
| backend | `platform/win32/ymz280b_audio_out.{h,cpp}` | SDL3 stream, 워커 스레드, 뮤텍스, WAV 캡처 |
| 게스트 ABI | `platform/win32/io/piu10_sound_port.{h,cpp}` | ISA 바이트 레인 → 칩 오프셋 변환 |

수정한 기존 파일은 `port_io_emulator.{h,cpp}`, `thread_context.h`,
`execution_trampoline.{h,cpp}`, `host/win32/main.cpp`, `CMakeLists.txt`,
`README.md`입니다.

miniz는 **새로 벤더링하지 않았습니다.** libchdr가 이미 번들하고 있으므로
`MINIZ_ARCHIVE_APIS`만 켜고 include 경로를 추가했습니다.

### 검증 증거

빌드: `scripts/build_win32_x86.ps1` 오류 0.

구동: `repiu_supervisor_win32.exe pumpit1 180000`, `REPIU_EXECUTION_BACKEND=aot-dynamic`,
`REPIU_YMZ_WAV_PATH` 지정. `F5`는 `keybd_event`로 6회 주입.

1. **ROM 적재** — CRC가 MAME `pumpit1` 정의와 정확히 일치했습니다.
   ```
   [repiu-ymz] loaded piu10.u9 (2097152 bytes) into a 4194304 byte space,
               crc32 0x9c436cfa (matches MAME pumpit1)
   [repiu-ymz] YMZ280B ready through SDL3 at 88200 Hz
   ```

2. **코인 입력 → key-on** — 인식된 코인 입력 4회 전부가 key-on을 유발했고 파라미터가
   매번 동일했습니다.
   ```
   [repiu-input] COIN1  PRESSED  port=0x02A9 value=0xFB
   [repiu-ymz]   keyon voice=0 mode=ADPCM start=0x000000 stop=0x00A150
                 rate=44100Hz level=255 pan=8 loop=no
   [repiu-input] COIN1  released port=0x02A9 value=0xFF
   ```

3. **파형** — 178.8초 캡처에서 무음이 아닌 구간이 정확히 4개, 피크 13547/32767.
   ```
   distinct sound events: 4
     t= 106.34s  len=1.874s  peak=13547
     t= 116.61s  len=1.874s  peak=13547
     t= 119.95s  len=1.874s  peak=13547
     t= 123.44s  len=1.874s  peak=13547
   ```

4. **길이 교차 검증 (가장 강한 증거)** — key-on이 보고한 `stop=0x00A150`은 41,296
   바이트이고 ADPCM은 바이트당 니블 2개이므로 82,592 샘플, 보고된 44,100 Hz에서
   **1.8728초**입니다. 측정된 재생 길이 **1.874초**와 일치합니다. 주소 단위(니블),
   샘플레이트 유도(`clock / 384 * 2` 후 `output_step / 512`), ADPCM 디코드가 모두
   독립적으로 맞다는 뜻입니다.

5. **생성 속도** — 240초 구동에서 239.3초 분량 생성(99.7%). 워커가 실시간을
   앞지르거나 뒤처지지 않습니다.

6. **부작용 없음** — `SDL queue failed` 0건, `unsupported port I/O` 0건, fatal 0건.

### 발견과 판단

**`0x02A0` 계열 초기화 쓰기의 정체가 밝혀졌습니다.** 기존
`IsObservedPortInitializationWrite`가 특례로 무시하던 `0x02A0 ← 0x00000010`,
`0x02A0 ← 0x00000001`, `0x02A2 ← 0x00000000`은 미상의 장치 초기화가 아니라
**정상적인 YMZ280B 레지스터 프로그래밍**이었습니다. 32비트 `OUT DX, EAX`가 ISA
16비트 버스에서 바이트 레인 4개로 분해되면 `0x02A0`이 레지스터 번호,
`0x02A2`가 데이터가 됩니다. 이제 정식 사운드 경로가 처리하므로 해당 함수는
호출자가 없어져 삭제했습니다.

**NOP 패치가 이 창에서 치명적이었습니다.** 종전 코드는 사운드 포트 쓰기를
`deferred-ignored`로 분류한 뒤 원본 `OUT`을 NOP으로 덮었습니다. 무시만 하는 것이
아니라 명령을 영구 파괴하므로, 설령 나중에 칩을 붙여도 최초 1회 이후로는 아무
레지스터 쓰기도 도달하지 못했을 것입니다. EEPROM·JAMMA가 Task 327에서 같은 이유로
바꾼 방식(EIP 전진 + 재트랩)을 그대로 적용했습니다.

**사운드 창이 JAMMA 입력 범위 안에 있습니다.** `0x02A0`~`0x02A3`은
`0x02A0`~`0x02AF` 입력 분기에 포함되므로 라우팅 순서가 정확성을 좌우합니다.
입력 분기가 먼저 걸리면 상태 레지스터 읽기가 항상 `0xFF`가 됩니다.

**WAV 캡처 헤더를 주기적으로 갱신해야 했습니다.** supervisor가 타임아웃 시 로더
프로세스를 종료시키므로 `Close()`가 실행되지 않고, 첫 구동의 캡처 파일은 데이터
청크 길이가 0으로 남아 열리지 않았습니다. 약 1초마다 RIFF 크기 필드를 다시 쓰도록
고쳐 강제 종료돼도 재생 가능한 파일이 남습니다.

### MAME와의 의도적 차이 2건

1. **IRQ를 발생시키지 않습니다.** PIU10 보드는 IRQ 라인을 배선하지 않으므로
   상태 레지스터만 유지합니다. MAME가 0지연 타이머로 미루는 처리를 생성 시점에
   인라인으로 수행합니다.
2. **스피커 route 이득 `0.5`를 적용하지 않습니다.** MAME의 `0.5`는 다른 사운드
   소스와 섞기 위한 여유분입니다. 여기서는 장치 출력 그대로를 기준으로 하고
   `REPIU_YMZ_VOLUME`으로 조정합니다.

### 남은 과제

* DSP 레지스터(`0x80`~`0x82`)와 외부 RAM 쓰기(`0x87`)는 미구현입니다. MAME도
  동일하며 이 게임이 쓰는 흔적은 아직 없습니다.
* 게임플레이 중 다중 보이스 동시 재생, 루프 음원, PCM8/PCM16 경로는 코드 경로만
  갖춰졌을 뿐 실측하지 못했습니다. 코인 효과음은 보이스 0 ADPCM 단독 경로만
  검증합니다.
* 레지스터 쓰기가 큐에 이미 들어간 블록에는 소급 적용되지 않습니다(최대 약 40 ms).
  실시간 실행 모델의 구조적 한계이며, 현재까지 청감상 문제는 관측되지 않았습니다.

---

## English

### Outcome

The goal is met. Pressing `F5` (COIN1) while `pumpit1` runs makes the YMZ280B decode
the coin effect from the sample ROM and play it through the SDL3 audio device. Four
coin insertions produced four playbacks, confirmed in the captured waveform.

### Implementation

| Layer | File | Content |
| :--- | :--- | :--- |
| ZIP | `assets/rom_zip_archive.{h,cpp}` | extract one entry to memory with CRC32 verification |
| ROM | `sound/ymz280b_sample_rom.{h,cpp}` | place `piu10.u9` in a 4 MiB `0xFF` space |
| Chip core | `sound/ymz280b_device.{h,cpp}` | MAME register map port, 8 voices, 88200 Hz stereo |
| Backend | `platform/win32/ymz280b_audio_out.{h,cpp}` | SDL3 stream, worker thread, mutex, WAV capture |
| Guest ABI | `platform/win32/io/piu10_sound_port.{h,cpp}` | ISA byte lane to chip offset translation |

Modified: `port_io_emulator.{h,cpp}`, `thread_context.h`,
`execution_trampoline.{h,cpp}`, `host/win32/main.cpp`, `CMakeLists.txt`, `README.md`.

miniz was **not vendored again** — libchdr already bundles it, so only
`MINIZ_ARCHIVE_APIS` was enabled and its include path added.

### Verification evidence

Build: `scripts/build_win32_x86.ps1`, zero errors.

Run: `repiu_supervisor_win32.exe pumpit1 180000` with
`REPIU_EXECUTION_BACKEND=aot-dynamic` and `REPIU_YMZ_WAV_PATH` set; `F5` injected six
times via `keybd_event`.

1. **ROM load** — the CRC matched the MAME `pumpit1` definition exactly
   (`crc32 0x9c436cfa`), and the stream opened at 88200 Hz.
2. **Coin input to key-on** — all four recognized coin inputs produced a key-on with
   identical parameters: voice 0, ADPCM, `start=0x000000`, `stop=0x00A150`,
   44100 Hz, level 255, pan 8 (centre), non-looping.
3. **Waveform** — exactly four non-silent regions in a 178.8 s capture, each
   1.874 s long with a peak of 13547 out of 32767.
4. **Duration cross-check (the strongest evidence)** — the reported
   `stop=0x00A150` is 41,296 bytes; ADPCM yields two nibbles per byte, so 82,592
   samples at the reported 44,100 Hz is **1.8728 s**, matching the measured
   **1.874 s**. Address units (nibbles), the sample-rate derivation
   (`clock / 384 * 2` then `output_step / 512`), and the ADPCM decode are therefore
   each independently correct.
5. **Generation rate** — 239.3 s of audio produced in a 240 s run (99.7%), so the
   worker neither outruns nor lags real time.
6. **No side effects** — zero `SDL queue failed`, zero `unsupported port I/O`, zero
   fatal messages.

### Findings and judgements

**The `0x02A0` initialization writes are now identified.** The writes
`IsObservedPortInitializationWrite` used to ignore as an unknown device family —
`0x02A0 <- 0x00000010`, `0x02A0 <- 0x00000001`, `0x02A2 <- 0x00000000` — were
ordinary YMZ280B register programming. A 32-bit `OUT DX, EAX` decomposes into four
ISA byte lanes, making `0x02A0` the register number and `0x02A2` the data. The real
sound path handles them now, leaving that function without callers, so it was
removed.

**NOP-patching was fatal in this window.** The previous code classified sound port
writes as `deferred-ignored` and overwrote the original `OUT` with NOPs. That does
not merely ignore the write, it destroys the instruction permanently — so even
after attaching a chip, no register write past the first would ever have arrived.
The fix mirrors what the EEPROM and JAMMA paths adopted in Task 327: advance EIP and
re-trap.

**The sound window sits inside the JAMMA input range.** `0x02A0`–`0x02A3` falls
within the `0x02A0`–`0x02AF` input branch, so routing order determines correctness;
if the input branch wins, status register reads always return `0xFF`.

**WAV capture headers needed periodic refresh.** The supervisor terminates the
loader process at timeout, so `Close()` never runs and the first capture was left
with a zero-length data chunk that no player would open. Rewriting the RIFF size
fields about once per second leaves a playable file even under forced termination.

### Two deliberate differences from MAME

1. **No IRQ is raised.** The PIU10 board leaves the IRQ line unwired, so only the
   status register is maintained; what MAME defers to a zero-delay timer is done
   inline at generation time.
2. **No `0.5` speaker route gain.** MAME's `0.5` is headroom for mixing against
   other sound sources. This implementation uses raw device output and exposes
   `REPIU_YMZ_VOLUME` for adjustment.

### Remaining work

* DSP registers (`0x80`–`0x82`) and external RAM writes (`0x87`) are unimplemented,
  as in MAME, with no evidence this game uses them.
* Simultaneous multi-voice playback, looping samples, and the PCM8/PCM16 paths have
  code paths but no measurements. Only the voice-0 ADPCM path is verified.
* Register writes cannot apply retroactively to already-queued blocks (up to about
  40 ms). This is structural to the real-time execution model and no audible problem
  has been observed so far.
