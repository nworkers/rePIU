# 20260809-463 PIU10 DAC audio backlog 감사 작업 로그 / PIU10 DAC Audio Backlog Audit Work Log

설계: [20260809-463-piu10-dac-audio-backlog-audit.md](../design/20260809-463-piu10-dac-audio-backlog-audit.md)

작업 지시: [20260809-463-piu10-dac-audio-backlog-audit.md](../work-orders/20260809-463-piu10-dac-audio-backlog-audit.md)

## 한국어

### 구현 결과

- `Piu10Mp3AudioSnapshot`과 관측 전용 `Snapshot()` API를 추가했습니다.
- worker의 decoder pending byte와 현재 PCM sample rate/channel을 atomic snapshot 상태로
  게시합니다.
- SDL 입력 PCM queue byte와 환산 ms, audio-device buffer frame/ms, compressed ring,
  receive/decode와 frame-sync를 DAC 감사 줄에 추가했습니다.
- 기존 gain, queue, decoder, DEMAND와 frame-sync 동작은 바꾸지 않았습니다.
- 반복 가능한 사용자 측정 절차를 `docs/guides/`에 추가했습니다.

### 검증 결과

- Win32 x86 Debug `repiu`와 `repiu_aot_probe` 빌드가 성공했습니다. 기존 저장소의
  code-page C4819 경고는 있었지만 새 오류는 없었습니다.
- `repiu_aot_probe --piu10`은 `piu10_mp3_snapshot=true,queued=-1,pending=0`을 기록했습니다.
- 기존 target profile, DAC3350A, MP3 latency, ring backpressure, frame batch와 stream audit
  probe도 모두 통과했습니다.
- 기존 사용자 `repiu_log.txt`를 보존하기 위해 자동 gameplay 실행은 하지 않았습니다.

### 남은 검증

사용자 환경에서 `REPIU_PIU10_DAC_AUDIT=1`로 실행하고 음악을 끊는 `0x0101` 줄의
`pcm-queued-ms`, `compressed-ring`, `decoder-pending`을 수집해야 합니다. 이 값으로 guest
DAC timeline과 HLE 출력 timeline의 불일치 여부를 판정합니다.

## English

### Implementation result

- Added `Piu10Mp3AudioSnapshot` and the observation-only `Snapshot()` API.
- The worker publishes decoder-pending bytes and current PCM sample rate/channels atomically.
- DAC audit now includes SDL input PCM queue bytes and duration, audio-device buffer frames and
  duration, compressed-ring bytes, receive/decode counters, and frame-sync.
- Existing gain, queue, decoder, DEMAND, and frame-sync behavior is unchanged.
- Added a repeatable user procedure under `docs/guides/`.

### Verification result

- Win32 x86 Debug builds of `repiu` and `repiu_aot_probe` succeeded. Existing repository C4819
  code-page warnings remained, with no new errors.
- `repiu_aot_probe --piu10` reported `piu10_mp3_snapshot=true,queued=-1,pending=0`.
- Existing target-profile, DAC3350A, MP3-latency, ring-backpressure, frame-batch, and stream-audit
  probes all passed.
- Automated gameplay was not run, preserving the user's existing `repiu_log.txt`.

### Remaining validation

Run with `REPIU_PIU10_DAC_AUDIT=1` in the user's environment and collect `pcm-queued-ms`,
`compressed-ring`, and `decoder-pending` on the interrupting `0x0101` record. These values will
classify whether the guest DAC and HLE output timelines diverge.
