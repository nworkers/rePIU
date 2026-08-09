# 20260809-462 pumpito DAC 출력 gain 작업 로그 / Pumpito DAC Output Gain Work Log

설계: [20260809-462-pumpito-dac-output-gain.md](../design/20260809-462-pumpito-dac-output-gain.md)

작업 지시: [20260809-462-pumpito-dac-output-gain.md](../work-orders/20260809-462-pumpito-dac-output-gain.md)

## 한국어

### 원인 정정

사용자 gameplay 로그에서 첫 MP3 재생 뒤 약 25초에 stop generation 1, 재동기화 약 1초 뒤 generation 2, 약 118초에 generation 3이 실행되었습니다. 최종 압축 입력 drop은 0이었습니다. 따라서 AVOL 저음량 전이를 곡 종료로 본 Task 461의 판단을 폐기했습니다.

### 구현 결과

- MAME DAC3350A 참고 구현과 같은 AVOL dB 계단과 linear gain 계산을 `Dac3350aControl`에 추가했습니다.
- DAC event에 좌우 linear gain을 포함했습니다.
- Win32 MP3 backend에 thread-safe `SDL_SetAudioStreamGain` 연결을 추가했습니다.
- 좌우가 다른 경우 두 linear gain의 평균을 적용합니다. 관측된 `pumpito` transaction은 좌우가 같습니다.
- DAC callback의 `Stop()` 호출과 worker stop-generation/FIFO/decoder reset 경로를 제거했습니다.
- DAC audit 로그에 좌우 gain, 적용 gain과 적용 성공 여부를 추가했습니다.

### 검증 결과

- Win32 Debug `repiu`와 `repiu_aot_probe` 빌드가 성공했습니다.
- PIU10 probe가 AVOL 0, 1, 8, 44, 63의 변환을 검사했고 `44 → unity 1.0`을 포함하여 통과했습니다.
- 기존 target profile, latency, DAC I²C, MP3 ring, frame batch와 stream audit probe도 모두 통과했습니다.
- 기존 사용자 `repiu_log.txt`를 보존하기 위해 자동 live 실행은 하지 않았습니다. gameplay 곡 연속성과 실제 음량은 사용자 확인 대상으로 남겼습니다.

## English

### Corrected cause

The user's gameplay log showed stop generation 1 at roughly 25 seconds after initial MP3 playback, generation 2 about one second after reacquisition, and generation 3 at roughly 118 seconds. Final compressed-input drops were zero. This invalidates Task 461's interpretation of low AVOL as song end.

### Implementation result

- Added the MAME-equivalent DAC3350A AVOL dB steps and linear-gain calculation to `Dac3350aControl`.
- Included left and right linear gains in DAC events.
- Connected the Win32 MP3 backend to thread-safe `SDL_SetAudioStreamGain`.
- Unequal channels use the arithmetic mean of their linear gains. Observed `pumpito` transactions have equal channels.
- Removed the DAC callback's `Stop()` call and the worker stop-generation/FIFO/decoder reset path.
- Extended DAC audit output with channel gains, applied gain, and application status.

### Verification result

- Win32 Debug `repiu` and `repiu_aot_probe` builds succeeded.
- The PIU10 probe checked AVOL 0, 1, 8, 44, and 63 and passed, including `44 → unity 1.0`.
- Existing target-profile, latency, DAC I2C, MP3 ring, frame-batch, and stream-audit probes also passed.
- No automated live run was made in order to preserve the user's existing `repiu_log.txt`. Gameplay continuity and actual output level remain for user validation.
