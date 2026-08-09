# 20260809-460 pumpito MP3 stream 연속성 작업 지시 / Pumpito MP3 Stream Continuity Work Order

설계: [20260809-460-pumpito-mp3-stream-continuity.md](../design/20260809-460-pumpito-mp3-stream-continuity.md)

## 한국어

- [x] 최신 사용자 로그에서 전체 입력 대비 8-frame decode와 정상 SDL 종료를 확인합니다.
- [x] 환경변수로 제한된 producer/consumer 4 KiB rolling-hash 진단을 구현합니다.
- [x] 호출 분할과 무관한 hash 결과를 공용 probe로 검증합니다.
- [x] 일반 batch 실행에서 producer/consumer stream을 비교합니다.
- [x] 필요하면 원본 byte-loop와 batch producer stream을 비교합니다.
- [x] 최초 불일치 계층을 교정하고 build, probe와 실제 실행으로 재검증합니다.
- [x] 누적 분석, architecture와 작업 로그를 갱신하고 커밋합니다.
- [ ] 사용자 환경에서 첫 음악 음질과 화면 동시 진행을 확인합니다.

## English

- [x] Confirm the latest user's full-input/eight-frame result and normal SDL exit.
- [x] Implement environment-gated producer/consumer 4 KiB rolling-hash diagnostics.
- [x] Verify hash independence from call segmentation in the shared probe.
- [x] Compare producer and consumer streams in a normal batch run.
- [x] If needed, compare producer streams between original byte-loop and batch runs.
- [x] Correct the first mismatching layer and revalidate with build, probe, and live execution.
- [x] Update cumulative analysis, architecture, and the work log, then commit.
- [ ] Confirm first-track quality and concurrent rendering in the user's environment.
