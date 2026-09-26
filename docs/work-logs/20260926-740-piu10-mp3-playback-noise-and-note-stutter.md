# Task 740 작업 로그 — PIU10 MP3 재생의 노이즈와 노트 끊김

설계: [20260926-740](../design/20260926-740-piu10-mp3-playback-noise-and-note-stutter.md)
작업 지시: [20260926-740](../work-orders/20260926-740-piu10-mp3-playback-noise-and-note-stutter.md)

## 요약

Linux x64 Release로 pumpitea를 플레이하면 음악에 노이즈가 끼고 화살표가 멈췄다가 올라간다는 보고를
계측으로 갈랐습니다. 화살표 시계의 기전을 찾아 고쳤습니다. MP3 frame-sync 토글과 decoder의 refill이
모두 **SDL 장치가 PCM을 가져가는 계단**(장치 buffer 단위)에 매여 있었습니다. WSLg의 17 ms buffer에서는
frame당 토글 하나가 유지되지만, buffer가 커지면 한 계단 안의 frame 경계 여러 개가 한 호출에서 함께
토글되고(240 Hz로 폴링하는 guest는 짝수 개를 변화 없음으로 봅니다) decoder도 계단 단위로 몰아서 채워
guest의 demand와 frame 카운터가 같은 계단으로 움직였습니다. 이제 장치 계단 사이를 PCM 속도로 보간하는
재생 시계가 토글과 decode 게이트를 움직입니다. 노이즈는 WSLg에서 재현되지 않았고, 사용자 host에서
판단할 수 있도록 최종 보고에 장치 buffer 크기와 빈 큐 횟수를 찍습니다.

## 과정

1. **계측.** `Piu10Mp3AudioOut`에 토글 수, 한 호출에 두 개 이상 토글한 횟수, PCM 큐 trough와 빈 큐
   횟수, 재생 시계 지연, 장치 buffer frame 수를 더했습니다. `REPIU_PIU10_MP3_CENSUS_MS=<ms>`로 live
   telemetry가 `[repiu-piu10-mp3-census] elapsed_ms received decoded queued_ms lag_ms inflight ring
   demand sync toggles multi starvation pcm_empty ticks_injected`를 찍고, 최종 보고에
   `PIU10 MP3 received/dropped/decoded/starvation/toggles/multi/pcm-empty/pcm-low-water-bytes/
   device-buffer-frames` 한 줄이 나옵니다.
2. **재현 경로.** attract 데모는 MP3를 틀지 않습니다(`01.AUD` 타이틀 징글만, 7.5~17.5초). 실제 곡은
   코인·시작·곡 선택을 거쳐야 하므로 Task 734의 XTest 합성 키 도구로 자동 진행했습니다(SERVICE=F2 ×5,
   P2 Center=KP_5, P2 UpRight=KP_9). 35초쯤 `AUDIO\39.AUD`가 시작해 18초 재생됩니다(입력이 없어 곡이
   끝나기 전에 게임이 끝납니다).
3. **기준 측정(768 frame, 17 ms).** 다중 토글 0, 빈 큐 0, 곡 중 큐 최저 235 ms, 토글 38~39/s, tick
   234~244/s, 곡 중 20 ms 이상 tick 공백 없음. 후보 B·C·D는 WSLg에서 해당 없음. 기아 14회는 곡 시작
   300 ms 안에서 guest 공급(25 KB/s)이 decoder를 잠시 앞서지 못한 것으로 큐가 100 ms 이상이라 무해.
4. **buffer 강제.** `SDL_AUDIO_DEVICE_SAMPLE_FRAMES=2048`이면 다중 토글 초당 4~6회(118/1352), `4096`이면
   174/429. 4096에서는 WSLg PulseAudio가 초당 세 덩어리만 가져가 재생이 1/3 속도였습니다(WSLg 산물).
5. **수정.** 재생 시계 `PlaybackPosition`: 장치가 가져간 byte 수가 바뀌면 직전 수에서 PCM 속도로
   올라가고, 뒤로 가지 않으며, 가져간 수를 넘지 않습니다. 토글은 이 시계가 frame 시작 offset을 지날
   때, decode는 시계 앞 PCM이 0.25초 미만일 때 frame 하나씩(큐가 그 절반 아래면 즉시) 합니다.
6. **첫 구현의 결함.** 계단마다 시계를 직전 추정값에 다시 고정했더니 worker의 관측 지연(2 ms sleep +
   스케줄링)을 매번 잃어 19% 느렸습니다(초당 31 토글, 지연이 초당 190 ms씩 증가, 큐가 절반 바닥에
   붙음). 직전 계단의 byte 수에 고정하는 형태로 바꿔 드리프트를 없앴습니다.

## 검증

| 검증 | 결과 |
|---|---|
| 768 frame, 수정 후, 60초 플레이 | decoded 1,251 = toggles 1,251, multi 0, pcm-empty 0, 곡 중 큐 235~244 ms, 시계 지연 0~17 ms, 토글 38~39/s |
| 2048 frame, 수정 후 | multi 0(기준 118), 시계 지연 0~35 ms, 곡 중 큐 218~226 ms, 토글 38~39/s, decode와 토글 일치 |
| 20 ms 표본당 guest 공급 | 100~600 byte(대부분 200~400), 곡 시작 외 2,000 byte 계단 없음 |
| Linux x64 Release core probe | `core_probe_all=true` |
| pumpit2a 25초(PIU10 없음) | 폴트 0, 2,687 frame, MP3 줄 전부 0 |
| Win32 x86 Debug 빌드 + core probe | 빌드 성공(기존 C4819 경고), `core_probe_all=true` |

로그: `build/task740-play1.err.log`(기준 768), `build/task740-play-sf2048.err.log`,
`build/task740-play-sf4096.err.log`, `build/task740-fix-play1.err.log`(느린 시계),
`build/task740-fix2-play1.err.log`, `build/task740-fix2-sf2048.err.log`. 재현 스크립트는
`scripts/task740_mp3_census_capture.sh`입니다(합성 키 도구는 세션 임시 디렉터리에 있어 커밋하지
않았습니다).

## 사용자 확인 (2026-09-26)

수정본으로 플레이한 사용자가 노트가 뛰어넘거나 멈칫거리는 증상이 사라졌고 노이즈도 들리지 않았다고
확인했습니다. 사용자 로그의 최종 보고: received 379,332, decoded 901, toggles 892, multi 0,
starvation 10(곡 시작 구간), pcm-empty 0, device-buffer-frames 768. 노이즈의 원인은 이 환경에서 큐가
빈 적이 없었으므로 확정하지 못했고, 이전 빌드의 계단식 refill이 유력한 후보로 남습니다.

## 남은 것

* 노이즈는 WSLg에서 재현되지 않았습니다. 사용자 host에서 최종 보고의 `pcm-empty`와
  `device-buffer-frames`를 확인해야 합니다. `pcm-empty`가 0이 아니면 큐 상한(0.25초)이 그 host의
  장치 계단에 비해 작은 것입니다.
* 토글 간격의 지터(2 ms sleep 깨어남 지연)는 그대로입니다. status 읽기 시점에 시계를 평가하면 없앨 수
  있지만 guest 폴링마다 SDL stream lock을 잡게 되어 미루었습니다.
* 곡 중 tick이 234~244/s로 240에서 ±2.5% 흔들리는 것과, 장면 전환 중 40~160 ms의 tick 공백은
  보았지만 조사하지 않았습니다.

---

# English

# Task 740 work log — noise and arrow stutter in PIU10 MP3 playback

Design: [20260926-740](../design/20260926-740-piu10-mp3-playback-noise-and-note-stutter.md)
Work order: [20260926-740](../work-orders/20260926-740-piu10-mp3-playback-noise-and-note-stutter.md)

## Summary

The report was that pumpitea on Linux x64 Release plays music with occasional noise and arrows that
stall and jump. Instrumentation separated the two, and the arrow clock's mechanism was found and fixed:
both the MP3 frame-sync toggles and the decoder's refills were tied to **the steps in which the SDL
device pulls PCM** (its buffer size). With WSLg's 17 ms buffer one toggle per frame held, but with a
larger buffer several frame boundaries inside one step toggled in the same call (a guest polling at 240
Hz reads an even number as no change) and the decoder refilled a whole step at once, so the guest's
demand and frame counter moved in the same steps. A playback clock interpolated at the PCM rate between
device steps now drives both the toggles and the decode gate. The noise did not reproduce on WSLg; the
final report prints the device buffer size and the empty-queue count so the user's host can be judged.

## Steps

1. **Instrumentation.** `Piu10Mp3AudioOut` gained toggle count, multi-toggle events, the PCM queue
   trough and empty count, clock lag and the device buffer frames. `REPIU_PIU10_MP3_CENSUS_MS=<ms>`
   makes the live telemetry print `[repiu-piu10-mp3-census] elapsed_ms received decoded queued_ms
   lag_ms inflight ring demand sync toggles multi starvation pcm_empty ticks_injected`, and the final
   report prints `PIU10 MP3 received/dropped/decoded/starvation/toggles/multi/pcm-empty/
   pcm-low-water-bytes/device-buffer-frames`.
2. **Reproduction path.** The attract demo plays no MP3 (only the `01.AUD` title jingle at 7.5-17.5 s).
   A real song needs coin, start and song select, driven with Task 734's XTest key tool (SERVICE=F2
   ×5, P2 Center=KP_5, P2 UpRight=KP_9); `AUDIO\39.AUD` starts at about 35 s and plays 18 s (the game
   ends before the song, since nothing is pressed).
3. **Baseline (768 frames, 17 ms).** No multi-toggles, no empty queue, mid-song queue minimum 235 ms,
   38-39 toggles and 234-244 ticks a second, no tick gap of 20 ms or more during the song: candidates
   B, C and D absent on WSLg. The 14 starvation events sit inside the first 300 ms of the song, where
   the guest's 25 KB/s feed briefly trails the decoder while the queue holds over 100 ms: harmless.
4. **Forced buffers.** `SDL_AUDIO_DEVICE_SAMPLE_FRAMES=2048` gave four to six multi-toggles a second
   (118/1352); `4096` gave 174/429. At 4096 WSLg's PulseAudio pulled only three chunks a second and
   playback ran at a third of its speed (a WSLg artefact).
5. **Fix.** The playback clock `PlaybackPosition`: when the pulled count changes, the clock rises at
   the PCM rate from the previous count, never runs backwards and never passes the pulled count.
   Toggles fire when the clock passes a frame's start offset; the decoder produces one frame at a
   time while less than a quarter second of PCM lies ahead of the clock, and refills at once only if
   the queue has dropped below half of that.
6. **The first version's defect.** Re-anchoring the clock at its previous estimate at every step lost
   the worker's observation delay (2 ms sleep plus scheduling) each time: 19% slow (31 toggles a
   second, lag growing 190 ms every second, the queue pinned at its half floor). Anchoring at the
   previous pulled count removed the drift.

## Verification

| Check | Result |
|---|---|
| 768 frames, fixed, 60 s play | decoded 1,251 = toggles 1,251, multi 0, pcm-empty 0, mid-song queue 235-244 ms, clock lag 0-17 ms, 38-39 toggles/s |
| 2048 frames, fixed | multi 0 (baseline 118), clock lag 0-35 ms, mid-song queue 218-226 ms, 38-39 toggles/s, decode and toggles match |
| Guest feed per 20 ms sample | 100-600 bytes (mostly 200-400), no 2,000-byte step outside song start |
| Linux x64 Release core probe | `core_probe_all=true` |
| pumpit2a 25 s (no PIU10) | no faults, 2,687 frames, MP3 line all zero |
| Win32 x86 Debug build + core probe | build succeeded (existing C4819 warning), `core_probe_all=true` |

Logs: `build/task740-play1.err.log` (768 baseline), `build/task740-play-sf2048.err.log`,
`build/task740-play-sf4096.err.log`, `build/task740-fix-play1.err.log` (the slow clock),
`build/task740-fix2-play1.err.log`, `build/task740-fix2-sf2048.err.log`. The capture script is
`scripts/task740_mp3_census_capture.sh` (the synthetic-key tool lives in the session's temporary
directory and was not committed).

## User confirmation (2026-09-26)

Playing the fixed build, the user confirmed that the arrows no longer skip or stall and that no
noise was heard. The user's final report: received 379,332, decoded 901, toggles 892, multi 0,
starvation 10 (song start), pcm-empty 0, device-buffer-frames 768. The noise's cause is not settled,
since the queue never ran empty on this host; the previous build's step-wise refill remains the likely
candidate.

## What remains

* The noise did not reproduce on WSLg. On the user's host, the final report's `pcm-empty` and
  `device-buffer-frames` decide it; a non-zero `pcm-empty` means the 0.25 s queue cap is small against
  that host's device step.
* Toggle-interval jitter (the 2 ms sleep wake-up latency) is unchanged. Evaluating the clock at the
  status read would remove it but would take the SDL stream lock on every guest poll, so it was left.
* The mid-song tick rate wandering 234-244 a second (±2.5% of 240) and the 40-160 ms tick gaps during
  scene loads were seen but not investigated.
