# 20260809-456 MAS3507D 지속형 MP3 HLE 작업 로그 / Persistent MAS3507D MP3 HLE Work Log

설계: [20260809-456-mas3507d-streaming-hle.md](../design/20260809-456-mas3507d-streaming-hle.md)

## 한국어

### 결과

- SDL3_mixer와 반복 `dr_mp3` batch decoder를 제거했습니다.
- upstream `minimp3` commit `ea99364f61c14656440e8d77e9c233ccf3124633`을 CC0-1.0으로
  고정했습니다. MAME 코드는 포함하지 않고 PIU10/MAS3507D 계약만 참고했습니다.
- 공용 4 MiB bounded SPSC byte ring을 추가했습니다. guest producer는 mutex나 decode 없이
  byte만 기록하고 Win32 worker가 하나의 persistent `mp3dec_t`로 frame을 decode합니다.
- 첫 세 개의 호환 MPEG header로 stream sync를 확정하고 S16 PCM을 SDL3 audio-device
  stream에 직접 공급합니다. batch 재초기화와 silence 삽입은 없습니다.
- board demand를 ring 여유 공간에 연결하고 성공한 frame decode마다 frame-sync를 토글합니다.
- destination `0x008`, `DX=0x02DA`, `OUT DX,AL`에 한정된 AOT resolver/arena port-handler
  조기 경로와 원자적 byte count를 추가했습니다.

### 검증

1. Win32 x86 Debug 전체 빌드가 완료됐습니다. SDL3_mixer link는 제거됐습니다.
2. `repiu_aot_probe --piu10`이 target 범위, 동적 status source, SPSC full/wrap/high-water와
   기존 flash/CAT702/byte sink 계약을 통과했습니다.
3. 55초 pumpito 실행에서 minimp3 playback이 1,257 guest byte에서 시작했고 이후 다른
   `AUDIO/D47.AUD`와 BGA 자산까지 진행했습니다. 실행 예외는 없고 port I/O
   input/output/handled/unhandled는 `1077877/2601171/3679048/0`이었습니다.
4. 후속 40초 실행은 초기 진행 편차로 504-byte 준비 전송까지만 도달했고
   received/dropped/decoded/pcm/starved/ring-high=`504/0/0/0/0/2`, unhandled=0이었습니다.

장시간 청감에서 끊김이 사라졌는지와 본 전송 중 arena fast-path byte 수는 사용자의 실제 플레이
검증이 남아 있습니다. 자동 실행에서는 소리를 평가할 수 없고 후속 실행이 본 음악 전송에
도달하지 않았으므로 이를 완료로 과장하지 않습니다.

## English

### Result

- Removed SDL3_mixer and repeated dr_mp3 batch decoders.
- Pinned upstream `minimp3` commit `ea99364f61c14656440e8d77e9c233ccf3124633` under CC0-1.0.
  MAME supplied contract reference only; no MAME code was incorporated.
- Added a shared 4 MiB bounded SPSC byte ring. The guest producer only writes bytes without a mutex
  or decode work; a Win32 worker continuously decodes frames with one persistent `mp3dec_t`.
- Three compatible MPEG headers establish initial sync. S16 PCM goes directly to an SDL3
  audio-device stream, without batch reinitialization or inserted silence.
- Ring capacity drives demand, successful frame decode toggles frame-sync, and exact
  destination-`0x008`, `DX=0x02DA`, `OUT DX,AL` receives AOT-resolver and arena-handler early paths.

### Verification

The full Win32 x86 Debug build completed without SDL3_mixer. `repiu_aot_probe --piu10` passed
target scope, dynamic status, SPSC full/wrap/high-water, flash, CAT702, and byte-sink checks. A
55-second pumpito run began minimp3 playback after 1,257 guest bytes, continued to later audio/BGA
asset loading, raised no execution exception, and reported port I/O
`1077877/2601171/3679048/0`. A later 40-second startup-variation run reached only the 504-byte
preparation transfer and reported received/dropped/decoded/PCM/starved/ring-high
`504/0/0/0/0/2` with zero unhandled ports.

User real-play validation remains necessary for long-listening continuity and main-transfer arena
fast-path byte totals. Automated execution cannot judge audible continuity, and the follow-up run
did not reach the main music transfer.
