# 20260809-462 pumpito DAC 출력 gain 설계 / Pumpito DAC Output Gain Design

## 한국어

### 문제와 확인 결과

Task 461은 DAC3350A `AVOL`의 가청→저음량 전이를 MP3 stop으로 해석했습니다. 실제 gameplay 로그에서는 한 곡 안에서 이 전이가 반복되어 SDL queue, 압축 FIFO와 minimp3 상태가 초기화되었고 음악이 끊겼습니다. `AVOL`은 decoder 수명 명령이 아니라 DAC 출력 제어입니다.

MAME의 BSD-3-Clause DAC3350A 구현은 6비트 AVOL을 다음과 같이 변환합니다.

- 0: mute
- 1~7: -75 dB부터 -57 dB까지 3 dB 간격
- 8 이상: -54 dB부터 1.5 dB 간격
- 최종 범위: -75 dB~+18 dB
- linear gain: `10^(dB/20)`

따라서 실제 guest 값 `0x2C`(44)는 0 dB, `0x01`은 -75 dB입니다.

### 설계

```mermaid
flowchart LR
    G["Guest DAC AVOL"] --> P["Dac3350aControl dB 변환"]
    P --> A["left/right linear gain"]
    A --> M["stereo 평균"]
    M --> S["SDL_SetAudioStreamGain"]
    S --> O["queued PCM 출력 gain"]
```

`Dac3350aControl`이 MAME과 같은 AVOL-to-linear-gain 변환을 플랫폼 공용 함수로 제공합니다. Win32 backend는 `SDL_SetAudioStreamGain`을 사용합니다. 이 API는 stream mutex를 내부에서 사용하고 이미 queue에 들어간 PCM에도 출력 시점 gain을 적용하므로 decoder, 압축 FIFO와 frame-sync를 건드리지 않습니다.

SDL audio stream은 좌우 독립 gain이 아니라 하나의 stream gain을 제공합니다. 실제 `pumpito`에서 확인된 명령은 좌우가 같으므로 정확히 재현됩니다. 좌우가 다르면 두 linear gain의 산술 평균을 사용하며 DAC audit에 좌우값과 적용 gain을 남깁니다. 별도 per-channel mixer는 실제 불균형 transaction이 확인될 때 확장합니다.

Task 461의 `Stop()`과 stop-generation reset 경로는 제거합니다. AVOL 0은 완전 무음이지만 decode와 queue 소비는 계속되므로 일시 mute 뒤 같은 곡 위치에서 자연스럽게 다시 들립니다. 곡 끝은 guest MP3 data의 종료와 queue 소진으로 자연스럽게 무음이 됩니다. pause는 여전히 구현하지 않습니다.

### 검증 전략

- AVOL 0, 1, 8, 44와 상한 clamp의 gain 값을 probe에서 검사합니다.
- DAC callback이 stop을 호출하지 않고 stream gain만 변경하는지 정적 검토합니다.
- Win32 Debug build와 PIU10 probe를 실행합니다.
- 실제 `pumpito`에서 stop generation 로그가 사라지고 동일 곡이 저음량 전이 뒤에도 이어지는지 확인합니다.

참고: [MAME DAC3350A](https://github.com/mamedev/mame/blob/master/src/devices/sound/dac3350a.cpp), [SDL3 audio stream gain](https://wiki.libsdl.org/SDL3/SDL_SetAudioStreamGain).

## English

### Problem and findings

Task 461 interpreted an audible-to-low DAC3350A `AVOL` transition as MP3 stop. A gameplay log showed this transition repeating within one song, clearing the SDL queue, compressed FIFO, and minimp3 state and cutting the music. `AVOL` controls DAC output, not decoder lifetime.

MAME's BSD-3-Clause DAC3350A implementation maps the six-bit AVOL value as follows: zero is mute; 1–7 cover -75 dB through -57 dB in 3 dB steps; values from 8 use 1.5 dB steps starting at -54 dB; the result is clamped to -75 dB through +18 dB and converted with `10^(dB/20)`. Thus guest value `0x2C` (44) is 0 dB and `0x01` is -75 dB.

### Design

Expose the MAME-equivalent AVOL-to-linear-gain conversion from platform-neutral `Dac3350aControl`. The Win32 backend uses `SDL_SetAudioStreamGain`. SDL serializes the call with the stream mutex and applies gain as queued PCM is consumed, leaving the decoder, compressed FIFO, and frame-sync untouched.

An SDL audio stream exposes one stream gain rather than independent left/right gains. All observed `pumpito` commands have equal channels and are therefore exact. For an unequal command, use the arithmetic mean of the two linear gains and include both values and the applied gain in DAC audit output. Add a per-channel mixer only if an unequal transaction is observed.

Remove the Task 461 `Stop()` and stop-generation reset path. AVOL zero produces silence while decoding and queue consumption continue, so unmuting resumes at the natural position. Song end naturally becomes silent when guest MP3 input and queued PCM are exhausted. Pause remains unimplemented.

### Verification strategy

- Probe gain values for AVOL 0, 1, 8, 44, and upper clamping.
- Statically verify that the DAC callback changes only stream gain and never calls stop.
- Build Win32 Debug and run the PIU10 probe.
- In a live `pumpito` run, confirm there are no stop-generation messages and that a song continues through a low-volume transition.

References: [MAME DAC3350A](https://github.com/mamedev/mame/blob/master/src/devices/sound/dac3350a.cpp), [SDL3 audio stream gain](https://wiki.libsdl.org/SDL3/SDL_SetAudioStreamGain).
