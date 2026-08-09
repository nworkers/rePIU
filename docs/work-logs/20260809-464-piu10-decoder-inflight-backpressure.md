# 20260809-464 PIU10 decoder inflight backpressure 작업 로그 / PIU10 Decoder Inflight Backpressure Work Log

설계: [20260809-464-piu10-decoder-inflight-backpressure.md](../design/20260809-464-piu10-decoder-inflight-backpressure.md)

작업 지시: [20260809-464-piu10-decoder-inflight-backpressure.md](../work-orders/20260809-464-piu10-decoder-inflight-backpressure.md)

## 한국어

### 원인 확정

사용자 재현의 중단 `0x0101`은 PCM 261.995 ms 외에 compressed ring 3,311 byte와 decoder
pending 287,755 byte를 남겼습니다. 총 291,066 byte는 약 18.2초이며, 앞선 72 KiB backlog가
173 frame 동안 drain된 관측과 일치합니다. 512-byte pop과 약 417-byte/frame 소비 차이가
staging에 누적되는데 ring-only DEMAND가 이를 빈 공간으로 잘못 보고했습니다.

### 구현 결과

- 공용 `sound::DecoderInputFifo`를 추가하여 ring과 staging을 포괄하는 atomic inflight를
  추적합니다.
- batch는 `0xE00` 논리 공간만 예약하고 byte path는 4 KiB 물리 headroom까지 허용합니다.
- worker의 ring pop은 inflight를 줄이지 않고 parser cursor 전진량만 차감합니다.
- PIU10 `DEMAND`, batch 수락량, 종료 통계와 DAC audit를 inflight에 연결했습니다.
- DAC gain, startup latency, PCM queue와 playback-offset frame-sync는 변경하지 않았습니다.

### 검증 결과

- Win32 x86 Debug `repiu`와 `repiu_aot_probe` 빌드가 성공했습니다. 기존 C4819 code-page
  경고 외에 새 오류는 없습니다.
- `repiu_aot_probe --piu10`은
  `demand_low=true,pop_keeps_low=true,demand_high=true,inflight_high=4096`으로 통과했습니다.
- 기존 target scope, latency, DAC3350A, frame batch, frame audit와 stream chunk audit도 모두
  통과했습니다.
- 최신 사용자 `repiu_log.txt`를 보존하기 위해 자동 gameplay 실행은 하지 않았습니다.

### 사용자 검증

2026-08-10 사용자 재실행에서 기존의 곡 중도 끊김 증상이 사라졌습니다. 따라서
decoder inflight 기반 backpressure가 관측된 재생 중단에 대한 유효한 대책임을
실기 환경에서 확인했습니다. 중단 시점의 `compressed-inflight` 수치와 노트 sync의
정밀 측정은 이번 확인에 포함되지 않았습니다.

## English

### Confirmed cause

The interrupting `0x0101` in the user reproduction left 261.995 ms of PCM, 3,311 compressed-ring
bytes, and 287,755 decoder-pending bytes. The 291,066-byte total is about 18.2 seconds and matches
the earlier observation of a 72 KiB backlog draining over 173 frames. The difference between a
512-byte pop and roughly 417-byte frame consumption accumulated in staging, while ring-only DEMAND
incorrectly reported that space as free.

### Implementation result

- Added shared `sound::DecoderInputFifo` to track atomic inflight across ring and staging.
- Batch input reserves only logical `0xE00` space; the byte path may use 4 KiB physical headroom.
- Worker ring pops do not reduce inflight; only parser-cursor advancement does.
- Connected PIU10 `DEMAND`, batch acceptance, shutdown statistics, and DAC audit to inflight.
- DAC gain, startup latency, PCM queue, and playback-offset frame-sync remain unchanged.

### Verification result

- Win32 x86 Debug builds of `repiu` and `repiu_aot_probe` succeeded, with only existing C4819
  code-page warnings.
- `repiu_aot_probe --piu10` passed with
  `demand_low=true,pop_keeps_low=true,demand_high=true,inflight_high=4096`.
- Existing target-scope, latency, DAC3350A, frame-batch, frame-audit, and stream-chunk-audit checks
  all passed.
- Automated gameplay was not run, preserving the latest user `repiu_log.txt`.

### User validation

On 2026-08-10, the user reran the game and reported that the previous mid-song interruption no
longer occurred. This confirms on the real gameplay path that decoder-inflight backpressure is an
effective mitigation for the observed interruption. This validation did not include a precise
measurement of `compressed-inflight` at the former interruption point or note synchronization.
