# 20260809-456 MAS3507D MP3 HLE 세션 인계 / MAS3507D MP3 HLE Session Handoff

설계: [20260809-456-mas3507d-streaming-hle.md](../design/20260809-456-mas3507d-streaming-hle.md)

다음 작업 지시: [20260809-456-mas3507d-streaming-hle.md](../work-orders/20260809-456-mas3507d-streaming-hle.md)

## 한국어

### 저장소 상태

- 브랜치: `task453-target-scoped-jamma`
- 버전: `0.0.144`
- `656c043`: JAMMA를 target profile capability로 제한
- `80d6b36`: SDL3_mixer idle/predecode 기반 PIU10 MP3 HLE
- `bf71997`: SDL3_mixer 4-frame 증분 batch 재생
- 아직 `main` merge, version bump와 tag는 수행하지 않았습니다.

### 확인된 계약

- PIU10 `0x02D0..0x02DF`는 JAMMA/YMZ280B `0x02A0..0x02AF`와 별도 ISA16 board입니다.
- 목적지 `0x008`의 `0x02DA` byte write는 MAS3507D `sid_w` PIO-DMA 입력입니다.
- 목적지 `0x008` read bit 2/1/0은 MPEG frame sync, send-ready, decoder demand입니다.
- `pumpito`가 `AUDIO/02.AUD`를 읽은 뒤 실행한 `OUT DX,AL`, DX=`0x02DA`는 정상 MP3 byte
  write였습니다. width 1 거부가 기존 `0xC0000096` 종료 원인이었습니다.
- mount의 `.AUD`는 host에서 바로 재생 가능한 MP3 header로 시작하지 않습니다. 원본 guest가
  변환해 port로 내보낸 stream은 정상 MPEG frame이므로 HLE 입력은 port byte여야 합니다.
- PIU10 capability는 `pumpito`, `pumpitc`, `pumpitpc`, `pumpite`에만 활성화되며
  `pumpit1`, `pumpit2`, `pumpit3`에는 적용하면 안 됩니다.

### 구현과 측정 이력

Task 454는 width-1 write와 SDL3_mixer 3.2.0 `dr_mp3` backend를 추가했습니다. 30 ms idle 뒤
전체 구간을 predecode했고 pumpito에서 50,585-byte stream 재생과 port unhandled=0을
확인했지만 시작이 지나치게 늦었습니다.

Task 455는 MPEG frame parser를 추가하고 완성 frame 4개마다 `MIX_AudioDecoder`로 decode하여
지속 PCM stream에 넣었습니다. 최초 재생은 guest 2,055 byte로 앞당겨졌고 port I/O
input/output/handled/unhandled는 `112897/235038/347935/0`이었습니다. Layer III reservoir를
위해 직전 2 frame overlap과 중복 PCM 제거도 추가했습니다. 전체 Win32 x86 Debug build와
probe는 통과했습니다.

사용자 실기 결과는 **Task 455 사용 불가**입니다. 음악이 너무 끊기고 MP3 재생 중 게임 진행이
멈춥니다. `bf71997`은 대체 구현의 출발점일 뿐 최종 해법이 아닙니다.

### 원인 판단

1. 4-frame마다 SDL3_mixer decoder를 재생성하여 decode가 불연속적입니다.
2. 지속 PCM track underrun을 silence로 채워 청감상 끊김이 생깁니다.
3. 더 큰 병목은 guest의 매-byte `OUT`이 일반 HLE, 계측과 동기화를 통과하는 것입니다.
   decoder가 worker에 있어도 원본 전송 loop가 늦어 게임 thread가 다음 로직으로 못 갑니다.
4. `MIX_SetTrackIOStream`은 전체 seekable data를 요구하고 SDL3_mixer 내장 dr_mp3 초기화는
   frame count/seek table을 위해 전체 stream을 scan하므로 성장형 ring을 직접 못 씁니다.

### 조사한 대안

- **upstream `minimp3` — 선택됨:** MAME 조사 후 `mp3dec_decode_frame` push API가 성장형
  stream과 persistent decoder 상태에 더 직접 맞는 것으로 판단했습니다. CC0-1.0이며 MAME
  wrapper가 아닌 upstream 공개 API로 독립 구현합니다.
- **standalone `dr_mp3` — 미선택:** 순차 callback도 가능하지만 starvation과 blocking read
  수명 관리가 frame-push API보다 복잡합니다.
- **Windows Media Foundation:** MP3 MFT가 있지만 Win32 전용이며 COM/MFT 수명과
  backpressure가 복잡하여 멀티플랫폼 우선 원칙에 맞지 않습니다.
- **mpg123, FFmpeg/libavcodec, libmad:** LGPL/GPL 또는 큰 의존성 때문에 도입하지 않습니다.

### 다음 세션 시작 순서

1. 이 handoff와 Task 456 설계·작업 로그를 읽습니다.
2. 현재 branch와 Task 456 구현 commit을 확인합니다.
3. pumpito 실제 플레이로 음악 연속성, 입력 반응과 장면 전환을 검증합니다.
4. `received/dropped/decoded/pcm/starved/ring-high`와 fast-path 활성 로그를 수집합니다.
5. 문제가 남으면 decoder를 교체하지 말고 ring starvation, SDL queue와 실제 fast-path byte 수를
   먼저 분해합니다.

### 참고 자료

- MAME PIU10: https://github.com/mamedev/mame/blob/master/src/mame/misc/xtom3d_piu10.cpp
- MAS3507D F10: https://floe.butterbrot.org/matrix/hacking/limp/docs/mas3507d_3pds.pdf
- dr_mp3: https://github.com/mackron/dr_libs/blob/master/dr_mp3.h
- dr_libs license: https://github.com/mackron/dr_libs/blob/master/LICENSE
- minimp3: https://github.com/lieff/minimp3
- SDL3_mixer 조사 코드:
  https://github.com/libsdl-org/SDL_mixer/blob/release-3.2.0/src/decoder_drmp3.c

## English

### Repository State

The branch is `task453-target-scoped-jamma`, version is `0.0.144`, and the relevant commits are
`656c043`, `80d6b36`, and `bf71997` as listed above. No merge to `main`, version bump, or tag has
been performed.

### Confirmed Contract and History

PIU10 is separate from JAMMA/YMZ280B. Destination-`0x008` byte writes at `0x02DA` are MAS3507D
PIO-DMA input; read bits 2/1/0 expose frame sync, send-ready, and demand. The pumpito width-one
`OUT DX,AL` was valid, and rejecting it caused `0xC0000096`. Host `.AUD` files are not directly
playable; the HLE boundary is the transformed guest port stream. Scope is limited to pumpito,
pumpitc, pumpitpc, and pumpite.

Task 454 played after a 50,585-byte idle-delimited segment but started too late. Task 455 moved
startup to 2,055 bytes/four frames and achieved `112897/235038/347935/0` port I/O accounting, with
builds and probes passing. User testing nevertheless rejected Task 455 because music stutters and
game progress stops. Commit `bf71997` is a replacement starting point, not an accepted solution.

### Cause and Selected Alternative

Repeated SDL3_mixer decoder construction and underrun silence explain the audible discontinuity.
The larger game-progress bottleneck is one generic HLE/telemetry crossing per compressed byte.
Growing `MIX_SetTrackIOStream` input does not solve this because it requires complete seekable data
and SDL3_mixer scans the input during dr_mp3 initialization.

Use one persistent upstream `minimp3` frame-push decoder fed by a compressed SPSC ring, output PCM
directly through SDL3, and add destination-`0x008` byte-`OUT` AOT/arena fast paths. MAME is a
contract reference only and no MAME wrapper code is incorporated. Standalone dr_mp3 was not chosen;
Media Foundation is too platform-specific; mpg123, FFmpeg/libavcodec, and libmad do not fit project
license/dependency policy.

### Next-Session Startup

Read this handoff plus the Task 456 design and work log, verify the implementation commit, then run
real pumpito play validation. Capture audio continuity, rendering/input/scene progress,
received/dropped/decoded/PCM/starved/ring-high telemetry, and fast-path activation. If a problem
remains, decompose ring starvation, SDL queueing, and actual fast-path byte totals before changing
decoders. The authoritative links in the Korean section apply here as well.
