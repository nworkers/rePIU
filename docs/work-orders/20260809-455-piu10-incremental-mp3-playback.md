# 20260809-455 PIU10 증분 MP3 재생 작업 지시 / PIU10 Incremental MP3 Playback Work Order

설계: [20260809-455-piu10-incremental-mp3-playback.md](../design/20260809-455-piu10-incremental-mp3-playback.md)

## 한국어

- [x] MPEG frame header parser와 단위 probe를 추가합니다.
- [x] idle 전체구간 predecode backend를 작은 완성 frame batch decoder로 교체합니다.
- [x] 하나의 지속적인 PCM stream과 MIX track에 batch 결과를 순서대로 공급합니다.
- [x] underrun silence 정책과 종료 수명 관리를 구현합니다.
- [x] 빌드, probe, pumpito runtime을 검증합니다.
- [x] architecture, 분석, 작업 로그를 갱신하고 commit합니다.

## English

- [x] Add an MPEG frame-header parser and unit probe.
- [x] Replace idle whole-segment predecode with small complete-frame batch decoding.
- [x] Feed batch results in order to one persistent PCM stream and MIX track.
- [x] Implement underrun-silence policy and shutdown lifetime management.
- [x] Verify the build, probes, and pumpito runtime.
- [x] Update architecture, analysis, and the work log, then commit.
