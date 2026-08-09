# 20260809-461 pumpito MP3 지연 보정과 stop 작업 지시 / Pumpito MP3 Latency Compensation and Stop Work Order

설계: [20260809-461-pumpito-mp3-latency-stop.md](../design/20260809-461-pumpito-mp3-latency-stop.md)

## 한국어

- [x] 현재 MP3 시작, frame-sync, teardown과 PIU10/DAC3350A 배선을 검토합니다.
- [x] target profile 기본값과 환경 변수 override를 갖는 시작 지연을 구현합니다.
- [x] 플랫폼 공용 DAC3350A I²C parser와 합성 probe를 구현합니다.
- [x] 실제 `pumpito`에서 DAC 명령과 MP3 전송 경계를 계측하여 stop 신호를 확정합니다.
- [x] pause 없이 worker 소유 stop/reset 경로를 구현합니다.
- [x] Win32 Debug build, PIU10 probe와 실제 실행으로 검증합니다.
- [x] architecture, 누적 분석과 작업 로그를 갱신하고 커밋합니다.
- [ ] 사용자 환경에서 latency 값과 stop 뒤 잔향 부재를 최종 확인합니다.

## English

- [x] Review current MP3 startup, frame-sync, teardown, and PIU10/DAC3350A wiring.
- [x] Implement startup latency with a target-profile default and environment override.
- [x] Implement a platform-neutral DAC3350A I2C control parser and synthetic probe.
- [x] Instrument live `pumpito` DAC commands and MP3 transfer boundaries to confirm a stop signal.
- [x] Implement a worker-owned stop/reset path without pause.
- [x] Verify with Win32 Debug builds, the PIU10 probe, and live execution.
- [x] Update architecture, cumulative analysis, and the work log, then commit.
- [ ] Confirm latency value and absence of residual audio after stop in the user's environment.
