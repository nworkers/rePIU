# 20260809-460 pumpito MP3 stream 연속성 작업 로그 / Pumpito MP3 Stream Continuity Work Log

설계: [20260809-460-pumpito-mp3-stream-continuity.md](../design/20260809-460-pumpito-mp3-stream-continuity.md)

작업 지시: [20260809-460-pumpito-mp3-stream-continuity.md](../work-orders/20260809-460-pumpito-mp3-stream-continuity.md)

## 한국어

### 결과

- `StreamChunkAudit`를 추가하여 호출 분할과 무관한 4 KiB FNV-1a 구간 hash를 계산합니다.
- `REPIU_PIU10_MP3_STREAM_AUDIT=1`일 때 pumpito MP3 producer와 worker consumer가 실제
  처리한 stream hash를 각각 기록합니다. 기본 실행에는 영향을 주지 않습니다.
- 일반 batch, 원본 byte-loop 감사, frame-sync 교정 실행을 비교했습니다. SPSC의
  producer/consumer hash는 일치했으며, 동일한 producer stream도 worker timing에 따라
  4 frame 또는 127 frame을 decode했습니다.
- minimp3에 FIFO의 남은 전체 buffer 대신 공용 MPEG parser가 확정한 정확한 frame 하나만
  전달하도록 교정했습니다. 불완전한 다음 header 때문에 현재 frame이 폐기되는 입력 분할
  의존성을 제거했습니다.
- decode된 PCM frame의 시작 offset을 기록하고 SDL queue가 실제로 해당 지점을 소비할 때
  PIU10 frame-sync를 전이하도록 변경했습니다.

### 검증

- Win32 Debug `repiu`, `repiu_aot_probe` 빌드 성공.
- `repiu_aot_probe --piu10` 성공:
  `piu10_mp3_stream_chunk_audit=true,chunks=2`,
  `piu10_mp3_frame_audit=true,frames=1`.
- 수정 후 독립적인 `pumpito` 실행 두 번 모두 첫 64 KiB에서 127 frame을 decode했습니다.
  체크포인트는 `66408/127/65566`, `66409/127/65567`이었습니다.
- 각 실행에서 producer와 consumer의 완성된 4 KiB 구간 62개가 모두 일치했고, 두 번째
  실행의 hash 불일치는 0개였습니다.
- 숨김 실행이므로 실제 음질과 화면·입력 동시 진행은 사용자 확인 항목으로 남겼습니다.

## English

### Result

- Added `StreamChunkAudit` to compute call-segmentation-independent 4 KiB FNV-1a chunk hashes.
- With `REPIU_PIU10_MP3_STREAM_AUDIT=1`, the pumpito MP3 producer and worker consumer independently
  log hashes of bytes they actually process. The diagnostic is off by default.
- Compared normal batch, original byte-loop audit, and frame-sync correction runs. SPSC producer
  and consumer hashes matched, while an identical producer stream decoded either four or 127
  frames depending on worker timing.
- Corrected the decoder to pass minimp3 exactly one frame established by the shared MPEG parser,
  rather than all remaining FIFO data. This removes input-segmentation dependence caused by an
  incomplete next header invalidating the current frame.
- Recorded decoded PCM frame start offsets and changed PIU10 frame-sync when SDL queue playback
  actually consumes each offset.

### Verification

- Built Win32 Debug `repiu` and `repiu_aot_probe` successfully.
- `repiu_aot_probe --piu10` passed with
  `piu10_mp3_stream_chunk_audit=true,chunks=2` and
  `piu10_mp3_frame_audit=true,frames=1`.
- Two independent post-fix `pumpito` runs both decoded 127 frames at the first 64 KiB checkpoint,
  reporting `66408/127/65566` and `66409/127/65567`.
- All 62 complete 4 KiB producer and consumer chunks matched in each run; the second run had zero
  hash mismatches.
- Subjective sound quality and concurrent rendering/input remain for user validation because the
  automated executions were hidden.
