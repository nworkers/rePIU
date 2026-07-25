# 20260725-290 작업 지시: YMZ280B 사운드 에뮬레이션 / Work order: YMZ280B sound emulation

설계: [docs/design/20260725-290-ymz280b-sound-emulation.md](../design/20260725-290-ymz280b-sound-emulation.md)

## 한국어

### 목표

PIU10 ISA 보드의 Yamaha YMZ280B를 에뮬레이션하여, `roms/pumpit1.zip`의 `piu10.u9`
샘플 ROM을 적재하고 **코인 투입 시 SDL3 오디오 장치로 소리가 출력되는 지점까지**
구현한다. 게스트 실행 의미, CD-DA 경로, 렌더 경로는 바꾸지 않는다.

### 범위

- 신규: `assets/rom_zip_archive.{h,cpp}`, `sound/ymz280b_device.{h,cpp}`,
  `sound/ymz280b_sample_rom.{h,cpp}`,
  `platform/win32/ymz280b_audio_out.{h,cpp}`,
  `platform/win32/io/piu10_sound_port.{h,cpp}`
- 수정: `port_io_emulator.{h,cpp}`, `thread_context.h`,
  `execution_trampoline.{h,cpp}`, `host/win32/main.cpp`, `CMakeLists.txt`
- 비대상: `cd_audio_wave_out.cpp`, Glide/OpenGL 경로, AOT/DBT 정책,
  YMZ280B IRQ 전달·DSP 레지스터·외부 RAM 쓰기

### 작업 단계 (전 단계 완료, 검증 결과는 작업 로그 참조)

- [x] **Stage 1 — ROM 적재 경로**
  - [x] libchdr 번들 miniz의 ZIP 리더 API를 활성화(`MINIZ_ARCHIVE_APIS`)하고
        include 경로를 `repiu_exe`에 추가한다. miniz를 별도로 벤더링하지 않는다.
  - [x] `RomZipArchive::ExtractEntry(zip, entry, out, message)` 구현.
        추출 후 ZIP 디렉터리의 CRC32와 대조한다.
  - [x] `Ymz280bSampleRom::LoadPumpIt1(zip_path)` 구현.
        4 MiB를 `0xFF`로 채우고 `piu10.u9`를 오프셋 0에 배치한다.
  - [x] 적재 성공/실패와 CRC를 stderr에 1회 기록한다.

- [x] **Stage 2 — 칩 코어 (플랫폼 공용)**
  - [x] `Ymz280bDevice`에 MAME 레지스터 맵을 그대로 이식한다.
        하위 `0x00`~`0x7F`는 `reg & 0xE3` 패턴, 상위는 `0x80`~`0xFF` 개별 처리.
  - [x] 주소는 니블 단위(`<<1`, `<<9`, `<<17`)로 합성하고 ROM 접근은
        `read_byte(position / 2)`로 단일화한다.
  - [x] ADPCM/PCM8/PCM16 생성기와 루프 처리(`loop_signal`/`loop_step` 포함)를 이식한다.
  - [x] `output_step` = ADPCM `(fnum & 0xFF) + 1`, PCM `(fnum & 0x1FF) + 1`.
        `FRAC_BITS = 9`로 선형 보간한다.
  - [x] 종료 시 램프다운(`t * 15 >> 4`)과 `status_register` 갱신을 이식한다.
  - [x] 88200 Hz 스테레오 s16 인터리브 버퍼를 생성하는 `Generate()`를 제공한다.
  - [x] IRQ는 발생시키지 않되 상태 레지스터는 유지한다.

- [x] **Stage 3 — SDL3 backend**
  - [x] `Ymz280bAudioOut::Open(rom_zip_path)`가 ROM 적재 → `SDL_InitSubSystem`
        → `SDL_OpenAudioDeviceStream(88200 Hz stereo s16)` → 워커 스레드 기동.
  - [x] 워커는 큐 깊이가 임계값(약 40 ms) 아래일 때만 블록을 생성해 넣는다.
  - [x] 레지스터 접근 4종(select write / data write / readback read / status read)을
        뮤텍스 아래에서 코어로 위임한다.
  - [x] `Close()`는 워커 정지 → 스트림 파기 → `SDL_QuitSubSystem` 순으로 쌍을 맞춘다.
  - [x] `REPIU_YMZ_WAV_PATH` 지정 시 생성 PCM을 WAV로 캡처한다.
  - [x] `REPIU_YMZ_VOLUME`으로 출력 이득을 조정한다(기본 1.0).

- [x] **Stage 4 — 포트 ABI 어댑터와 라우팅**
  - [x] `piu10_sound_port`가 `0x02A0`~`0x02A3` 접근을 바이트 레인으로 분해하고
        짝수 포트만 칩 오프셋 `(port - 0x02A0) / 2`로 전달한다. 홀수는 버린다.
  - [x] `HandlePortIoInstruction`에서 사운드 포트를 JAMMA 분기보다 먼저 처리한다.
  - [x] 사운드 포트 쓰기는 NOP 패치하지 않고 EIP만 전진시킨다.
  - [x] `IsObservedPortInitializationWrite`의 `0x02A0`/`0x02A2` 특례를 제거한다.

- [x] **Stage 5 — 수명 배선**
  - [x] `ThreadContext`에 `ymz_audio`, `ymz_audio_available`을 추가한다.
  - [x] `RunWin32ExecutionThread`에 `sound_rom_zip_path` 파라미터를 추가하고
        `cd_audio.Open` 옆에서 연다.
  - [x] `AttemptWin32GuestStackTrapExecution` /
        `AttemptWin32GuestStackAotExecution` 시그니처와 `main.cpp` 호출부를 갱신한다.
  - [x] `main.cpp`가 `mount.rom_zip_path`를 전달한다.

- [x] **Stage 6 — 빌드와 검증**
  - [x] `scripts/build_win32_x86.ps1` 빌드 성공.
  - [x] 로더 구동 후 `F5`(COIN1) 입력.
  - [x] 다음 증거를 모두 확보한다.
        (a) ROM 적재 로그와 CRC 일치
        (b) key-on 로그 1건 이상
        (c) 비무음 샘플 카운터 > 0
        (d) `REPIU_YMZ_WAV_PATH` WAV의 진폭이 0이 아님
  - [x] 증거가 하나라도 빠지면 작업 로그에 실패 지점과 관측값을 기록한다.

- [x] **Stage 7 — 문서**
  - [x] `docs/analysis/piu-io-port-specification.md`에 `0x02A0`~`0x02A3` 확정
        디코드와 NOP 패치 금지 사유를 반영한다.
  - [x] `docs/kb/`에 YMZ280B 레지스터·주소 단위·샘플레이트 배경 문서를 추가하고
        `docs/kb/README.md` 색인을 갱신한다.
  - [x] `ARCHITECTURE.md`에 사운드 계층을 반영한다.
  - [x] `docs/work-logs/20260725-290-ymz280b-sound-emulation.md` 작성.

### 검증 절차

| 단계 | 명령 | 통과 기준 |
| :--- | :--- | :--- |
| 빌드 | `powershell -ExecutionPolicy Bypass -File scripts\build_win32_x86.ps1` | 오류 0 |
| ROM | 로더 stderr | `[repiu-ymz] rom loaded ... crc=9c436cfa` |
| 소리 | 로더 구동 + `F5` | key-on 로그 ≥ 1, 비무음 샘플 > 0 |
| 파형 | `REPIU_YMZ_WAV_PATH=...` | WAV 최대 진폭 ≠ 0 |

### 롤백

사운드 경로는 `ymz_audio_available`이 거짓이면 전부 비활성화되고 포트 쓰기는 종전처럼
무시된다. ROM 적재 실패나 SDL 장치 열기 실패가 게스트 실행을 중단시키지 않아야 한다.

---

## English

### Goal

Emulate the Yamaha YMZ280B on the PIU10 ISA board, load the `piu10.u9` sample ROM
from `roms/pumpit1.zip`, and reach the point where **inserting a coin produces sound
through the SDL3 audio device**. Guest execution semantics, the CD-DA path, and the
render path are unchanged.

### Scope

New: `assets/rom_zip_archive.{h,cpp}`, `sound/ymz280b_device.{h,cpp}`,
`sound/ymz280b_sample_rom.{h,cpp}`, `platform/win32/ymz280b_audio_out.{h,cpp}`,
`platform/win32/io/piu10_sound_port.{h,cpp}`. Modified: `port_io_emulator.{h,cpp}`,
`thread_context.h`, `execution_trampoline.{h,cpp}`, `host/win32/main.cpp`,
`CMakeLists.txt`. Out of scope: `cd_audio_wave_out.cpp`, the Glide/OpenGL path,
AOT/DBT policy, YMZ280B IRQ delivery, DSP registers, external RAM writes.

### Stages

1. **ROM load path.** Enable the ZIP reader API in libchdr's bundled miniz
   (`MINIZ_ARCHIVE_APIS`) and add its include path to `repiu_exe` rather than
   vendoring miniz separately. Implement `RomZipArchive::ExtractEntry` with CRC32
   verification against the ZIP directory, and `Ymz280bSampleRom::LoadPumpIt1`, which
   fills 4 MiB with `0xFF` and places `piu10.u9` at offset 0. Log load result and CRC
   once to stderr.
2. **Chip core (platform-neutral).** Port the MAME register map verbatim: lower
   `0x00`–`0x7F` via the `reg & 0xE3` pattern, upper `0x80`–`0xFF` individually.
   Compose addresses in nibble units (`<<1`, `<<9`, `<<17`) with a single
   `read_byte(position / 2)` ROM access. Port the ADPCM/PCM8/PCM16 generators
   including loop handling (`loop_signal`/`loop_step`), `output_step` as
   `(fnum & 0xFF) + 1` for ADPCM and `(fnum & 0x1FF) + 1` for PCM with `FRAC_BITS = 9`
   linear interpolation, and the end-of-sample ramp-down (`t * 15 >> 4`) with status
   register updates. Expose `Generate()` producing an 88200 Hz stereo interleaved s16
   buffer. Maintain the status register but raise no IRQ.
3. **SDL3 backend.** `Ymz280bAudioOut::Open(rom_zip_path)` loads the ROM, calls
   `SDL_InitSubSystem`, opens an 88200 Hz stereo s16 stream, and starts a worker
   thread that generates a block only while queue depth is below roughly 40 ms.
   The four register accesses delegate to the core under a mutex. `Close()` stops the
   worker, destroys the stream, and quits the subsystem in matched order.
   `REPIU_YMZ_WAV_PATH` captures generated PCM to WAV; `REPIU_YMZ_VOLUME` adjusts
   output gain (default 1.0).
4. **Port ABI adapter and routing.** `piu10_sound_port` decomposes `0x02A0`–`0x02A3`
   accesses into byte lanes, forwarding only even ports to chip offset
   `(port - 0x02A0) / 2` and discarding odd ones. `HandlePortIoInstruction` handles
   sound ports before the JAMMA branch; sound writes advance EIP without NOP-patching;
   the `IsObservedPortInitializationWrite` special case for these ports is removed.
5. **Lifetime wiring.** Add `ymz_audio` and `ymz_audio_available` to `ThreadContext`,
   add a `sound_rom_zip_path` parameter to `RunWin32ExecutionThread` and open it next
   to `cd_audio.Open`, update the two `Attempt*` signatures and the `main.cpp` call
   sites, and pass `mount.rom_zip_path` from `main.cpp`.
6. **Build and verification.** Build via `scripts/build_win32_x86.ps1`, run the
   loader, press `F5` (COIN1), and collect all of: ROM load log with matching CRC, at
   least one key-on log line, a non-silent sample counter above zero, and a captured
   WAV with non-zero amplitude. Record the failing point and observed values in the
   work log if any evidence is missing.
7. **Documentation.** Update `docs/analysis/piu-io-port-specification.md` with the
   confirmed `0x02A0`–`0x02A3` decode and the reason NOP-patching is forbidden, add a
   YMZ280B knowledge topic under `docs/kb/` with its README index entry, reflect the
   sound layer in `ARCHITECTURE.md`, and write the work log.

### Verification

| Step | Command | Pass criterion |
| :--- | :--- | :--- |
| Build | `powershell -ExecutionPolicy Bypass -File scripts\build_win32_x86.ps1` | zero errors |
| ROM | loader stderr | `[repiu-ymz] rom loaded ... crc=9c436cfa` |
| Sound | loader + `F5` | key-on lines ≥ 1, non-silent samples > 0 |
| Waveform | `REPIU_YMZ_WAV_PATH=...` | peak amplitude ≠ 0 |

### Rollback

The sound path is fully disabled when `ymz_audio_available` is false, and port writes
are ignored as before. Neither ROM load failure nor SDL device open failure may abort
guest execution.
