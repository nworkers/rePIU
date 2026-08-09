# 20260809-461 pumpito MP3 지연 보정과 stop 작업 로그 / Pumpito MP3 Latency Compensation and Stop Work Log

> **후속 정정 / Later correction:** Task 462에서 이 작업의 AVOL-to-stop 결론이 gameplay 중 오동작으로 확인되어 제거되었습니다. 지연 보정과 I²C parser는 유지되고 stop mapping은 [Task 462 작업 로그](20260809-462-pumpito-dac-output-gain.md)로 대체됩니다. / Task 462 found the AVOL-to-stop conclusion to be incorrect during gameplay and removed it. Latency compensation and the I2C parser remain; the stop mapping is superseded by the [Task 462 work log](20260809-462-pumpito-dac-output-gain.md).

설계: [20260809-461-pumpito-mp3-latency-stop.md](../design/20260809-461-pumpito-mp3-latency-stop.md)

작업 지시: [20260809-461-pumpito-mp3-latency-stop.md](../work-orders/20260809-461-pumpito-mp3-latency-stop.md)

## 한국어

### 구현 결과

- `pumpito` profile에만 50 ms MP3 시작 지연 기본값을 추가하고 `REPIU_PIU10_MP3_LATENCY_MS=0..500` override를 추가했습니다.
- 첫 PCM 앞에 format에 맞춘 무음을 넣고 stop 뒤 다음 stream에도 다시 적용하게 했습니다.
- 플랫폼 공용 DAC3350A I²C parser를 추가하여 목적지 `0x010`의 AVOL을 해석합니다.
- `pumpito`의 가청→저음량 전이만 stop으로 연결했습니다. pause는 추가하지 않았습니다.
- MP3 worker가 SDL queue, 압축 FIFO, encoded buffer, minimp3/MPEG sync와 frame-sync offset을 함께 reset하는 generation 기반 stop을 구현했습니다.

### 검증 결과

- Win32 Debug의 `repiu`와 `repiu_aot_probe`를 빌드했습니다.
- PIU10 probe에서 profile 50 ms, 8,820 silence byte와 합성 DAC3350A AVOL transaction이 통과했습니다.
- 실제 `pumpito` 실행에서 `0x0101 → 0x2C2C → 0x0101`을 관측했습니다. 마지막 전이에서 stop generation 1이 완료되고 반복 저음량에는 재발행되지 않았으며 이후 다음 MP3 stream이 다시 시작되었습니다.
- 자동 실행으로 판단할 수 없는 최종 음악/노트 체감 sync와 stop 잔향은 사용자 검증 대상으로 남겼습니다.

## English

### Implementation result

- Added a 50 ms MP3 startup default only to the `pumpito` profile and the `REPIU_PIU10_MP3_LATENCY_MS=0..500` override.
- Inserted format-derived silence before the first PCM and reapplied it to a new stream after stop.
- Added a platform-neutral DAC3350A I2C parser for destination-`0x010` AVOL commands.
- Mapped only the `pumpito` audible-to-low-volume transition to stop. Pause was not added.
- Added generation-based worker stop that jointly resets the SDL queue, compressed FIFO, encoded buffer, minimp3/MPEG sync, and frame-sync offsets.

### Verification result

- Built Win32 Debug `repiu` and `repiu_aot_probe`.
- The PIU10 probe passed the 50 ms profile default, 8,820 silence bytes, and a synthetic DAC3350A AVOL transaction.
- A live `pumpito` run observed `0x0101 → 0x2C2C → 0x0101`. The final transition completed stop generation 1 without retriggering on repeated low volume, and the next MP3 stream subsequently started.
- Subjective music/note alignment and residual audio after stop remain for validation in the user's environment.
