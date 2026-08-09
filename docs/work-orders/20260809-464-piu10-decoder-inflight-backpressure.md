# 20260809-464 PIU10 decoder inflight backpressure 작업 지시 / PIU10 Decoder Inflight Backpressure Work Order

설계: [20260809-464-piu10-decoder-inflight-backpressure.md](../design/20260809-464-piu10-decoder-inflight-backpressure.md)

## 한국어

- [x] Task 463 사용자 로그에서 약 18초 compressed backlog를 확인합니다.
- [x] ring 이동과 decoder 소비를 분리하는 inflight 계약을 설계합니다.
- [x] 공용 `DecoderInputFifo`와 synthetic probe를 구현합니다.
- [x] PIU10 MP3 byte/batch 입력, worker consume와 DEMAND를 inflight에 연결합니다.
- [x] DAC audit와 종료 통계에 inflight를 추가합니다.
- [x] Win32 Debug build와 PIU10 probe를 수행합니다.
- [x] architecture, 누적 분석과 작업 로그를 갱신합니다.
- [x] 변경을 커밋합니다.
- [x] 사용자 환경에서 중도 끊김 재현 여부와 음악 연속성을 재검증합니다.

## English

- [x] Confirm the roughly 18-second compressed backlog in the Task 463 user log.
- [x] Design an inflight contract separating ring movement from decoder consumption.
- [x] Implement shared `DecoderInputFifo` and its synthetic probe.
- [x] Connect PIU10 MP3 byte/batch input, worker consumption, and DEMAND to inflight occupancy.
- [x] Add inflight to DAC audit and shutdown statistics.
- [x] Run Win32 Debug builds and the PIU10 probe.
- [x] Update architecture, cumulative analysis, and the work log.
- [x] Commit the changes.
- [x] Revalidate interruption reproduction and music continuity in the user's environment.
