# 20260809-462 pumpito DAC 출력 gain 작업 지시 / Pumpito DAC Output Gain Work Order

설계: [20260809-462-pumpito-dac-output-gain.md](../design/20260809-462-pumpito-dac-output-gain.md)

## 한국어

- [x] gameplay 로그와 Task 461의 잘못된 stop 판정을 분석합니다.
- [x] MAME DAC3350A AVOL 변환과 SDL3 stream gain 계약을 확인합니다.
- [x] 플랫폼 공용 AVOL-to-gain 변환과 probe를 구현합니다.
- [x] Win32 MP3 backend에 thread-safe stream gain 제어를 추가합니다.
- [x] DAC callback의 stop 연결과 불필요한 stop-generation 경로를 제거합니다.
- [x] Win32 Debug build와 PIU10 probe를 실행합니다.
- [x] architecture, 누적 분석과 작업 로그를 갱신합니다.
- [x] 변경을 커밋합니다.
- [ ] 사용자 환경에서 곡 연속성과 실제 음량을 확인합니다.

## English

- [x] Analyze the gameplay log and Task 461's incorrect stop classification.
- [x] Confirm the MAME DAC3350A AVOL conversion and SDL3 stream-gain contract.
- [x] Implement the platform-neutral AVOL-to-gain conversion and probe.
- [x] Add thread-safe stream gain control to the Win32 MP3 backend.
- [x] Remove the DAC stop mapping and unnecessary stop-generation path.
- [x] Run the Win32 Debug build and PIU10 probe.
- [x] Update architecture, cumulative analysis, and the work log.
- [x] Commit the changes.
- [ ] Validate song continuity and actual volume in the user's environment.
