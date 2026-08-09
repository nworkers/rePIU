# 20260809-455 PIU10 증분 MP3 재생 작업 로그 / PIU10 Incremental MP3 Playback Work Log

설계: [20260809-455-piu10-incremental-mp3-playback.md](../design/20260809-455-piu10-incremental-mp3-playback.md)

작업 지시: [20260809-455-piu10-incremental-mp3-playback.md](../work-orders/20260809-455-piu10-incremental-mp3-playback.md)

## 한국어

### 결과

- Task 454의 idle 기반 전체 구간 predecode를 제거했습니다.
- 플랫폼 공용 MPEG audio frame parser를 추가하여 MPEG-1/2/2.5와 Layer I/II/III의 bitrate,
  sample rate, padding에 따른 frame 길이를 계산합니다.
- guest byte에서 연속된 header로 frame 경계를 검증하고 완성 frame 4개부터 SDL3_mixer
  `MIX_AudioDecoder`로 decode합니다.
- 첫 PCM format으로 지속적인 `SDL_AudioStream`과 MIX track을 만들고 후속 batch를 같은
  queue에 순서대로 넣습니다. 일시적인 queue underrun은 silence로 채워 track 종료를 막습니다.
- Layer III bit reservoir가 batch 경계에서 끊기지 않도록 직전 2 frame을 다음 decoder 입력에
  겹치고, overlap PCM은 queue 전에 제거합니다.

### 검증

1. Win32 x86 Debug 전체 빌드 성공. `mp3_drmp3`만 활성화된 상태를 유지합니다.
2. 전체 AOT probe 성공. `FF FB 90 64` header를 44.1 kHz, 128 kbps, stereo,
   417-byte MPEG-1 Layer III frame으로 확인했습니다.
3. pumpito 제한 실행은 2,055 guest byte/4 frame에서 incremental playback을 시작했습니다.
4. port I/O input/output/handled/unhandled=`112897/235038/347935/0`, 실행은 예외 없이
   설정한 제한 시간에 종료했습니다.

Layer III overlap 보정 후 전체 build와 probe를 다시 통과했습니다. 이어진 50초 runtime은
기존의 비결정적인 초기 진행 편차로 음악 전송 지점에 도달하지 못했지만
input/output/handled/unhandled=`45075/17308/62383/0`으로 새 예외나 port 회귀는 없었습니다.
최초 batch 경로는 overlap이 비어 있으므로 2,055-byte 시작 조건은 보정 전후 동일합니다.

## English

### Result

- Removed Task 454's idle-based whole-segment predecode.
- Added a platform-neutral MPEG audio frame parser that calculates frame lengths for MPEG
  1/2/2.5 Layer I/II/III from bitrate, sample rate, and padding.
- Frame boundaries are validated with consecutive headers, and SDL3_mixer `MIX_AudioDecoder`
  starts decoding as soon as four complete frames arrive.
- The first PCM format creates one persistent `SDL_AudioStream` and MIX track. Later batches
  enter the same queue in order; temporary underruns receive silence so the track stays alive.
- The previous two frames overlap the next decoder input to preserve Layer III bit-reservoir
  context; PCM produced from the overlap is removed before queueing.

### Verification

1. The complete Win32 x86 Debug build passed with only `mp3_drmp3` enabled.
2. The complete AOT probe passed. Header `FF FB 90 64` resolves to a 44.1 kHz, 128 kbps,
   stereo, 417-byte MPEG-1 Layer III frame.
3. A timeout-bounded pumpito run started incremental playback at 2,055 guest bytes/four frames.
4. Port I/O input/output/handled/unhandled=`112897/235038/347935/0`; execution reached the
   configured time limit without an exception.

The full build and probes passed again after adding the Layer III overlap correction. A following
50-second runtime did not reach music transfer because of the existing nondeterministic startup
variation, but reported input/output/handled/unhandled=`45075/17308/62383/0` with no new
exception or port regression. The first batch has no overlap, so its 2,055-byte startup condition
is unchanged by the correction.

## 후속 사용자 검증 / Follow-up User Validation

### 한국어

사용자의 실제 플레이 검증에서는 Task 455 방식이 사용할 수 없는 것으로 판정되었습니다.
음악이 심하게 끊겼고 MP3 재생 중에는 게임 진행도 멈추는 현상이 확인되었습니다. 따라서
빌드와 단기 런타임 계측 성공은 실제 사용 가능성을 보장하지 않으며, 반복적인 소규모
SDL3_mixer decoder 생성과 byte 단위 일반 port-I/O 경로를 모두 교체해야 합니다.

후속 설계와 구현 계획은
[Task 456 MAS3507D streaming HLE](../design/20260809-456-mas3507d-streaming-hle.md)에
정리했습니다.

### English

The user's real-play validation rejected the Task 455 approach as unusable. Audio stuttered
severely, and game execution stopped making progress during MP3 playback. Passing the build and
short runtime instrumentation therefore did not establish practical usability. Both repeated
small SDL3_mixer decoder construction and the generic per-byte port-I/O path need replacement.

The replacement design and implementation plan are recorded in
[Task 456 MAS3507D streaming HLE](../design/20260809-456-mas3507d-streaming-hle.md).
