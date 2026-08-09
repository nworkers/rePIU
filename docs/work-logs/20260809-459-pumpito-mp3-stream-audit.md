# 20260809-459 pumpito MP3 stream 감사 작업 로그 / Pumpito MP3 Stream Audit Work Log

설계: [20260809-459-pumpito-mp3-stream-audit.md](../design/20260809-459-pumpito-mp3-stream-audit.md)
작업 지시: [20260809-459-pumpito-mp3-stream-audit.md](../work-orders/20260809-459-pumpito-mp3-stream-audit.md)

## 한국어

### 결과

- `REPIU_PIU10_MP3_BATCH_AUDIT=1`에서 일괄 enqueue와 guest commit 없이 예측 tail과 원본
  byte loop의 byte, source cursor, frame count, `ECX`를 같은 실행에서 비교합니다.
- 실제 최초 차이는 압축 byte가 아니라 source cursor `0x76C` 이후 원본 loop가 초기화한
  `ECX`였습니다. 기존 batch가 100-byte 보조 처리·반환 경계를 생략한 것이 stream 손상의
  원인이었습니다.
- batch를 다음 제어 경계까지만 허용하여 원본 비교·분기가 그대로 실행되게 했습니다.
- 64 KiB마다가 아니라 최초 64 KiB에서만 decode 체크포인트를 출력하여 장시간 로그 증가를
  제한했습니다.

### 검증

- Win32 x86 Debug `repiu`, `repiu_aot_probe` 빌드 성공.
- `repiu_aot_probe --piu10`: target/JAMMA/ring/frame batch 및 새 frame audit probe 모두 통과.
- 교정 전 실제 감사: byte/cursor/count는 일치, `ECX=1900/0` 및 이후 `100/0` 불일치 재현.
- 교정 후 실제 감사: mismatch 없이 계획 구간 1,700개 통과, playback은 1,253 byte 뒤 시작.
- 일반 batch 실행: playback은 1,671 byte 뒤 시작했고 64 KiB 체크포인트는
  `received/decoded/batched=66410/127/65567`.

### 남은 검증

숨김 자동 실행은 소리를 직접 청취하거나 화면과 입력을 평가할 수 없습니다. 첫 음악 음질과
음악·화면의 동시 진행은 사용자가 최종 확인해야 합니다.

## English

### Result

- `REPIU_PIU10_MP3_BATCH_AUDIT=1` compares predicted tails with bytes, source cursor, frame count,
  and `ECX` from the original byte loop in the same run without batch enqueue or guest commit.
- The first live difference was not a compressed byte. It was `ECX`, reset by the original loop
  after source cursor `0x76C`. Skipping that 100-byte auxiliary-processing and return boundary caused
  the stream damage.
- Batches now stop at the next control boundary so the original compare and branch still execute.
- A decode checkpoint is logged only at the first 64 KiB to keep long-run logging bounded.

### Verification

- Win32 x86 Debug builds of `repiu` and `repiu_aot_probe` succeeded.
- `repiu_aot_probe --piu10` passed target, JAMMA, ring, frame-batch, and new frame-audit probes.
- Before correction, the live audit matched byte/cursor/count but reproduced `ECX=1900/0` and
  recurring `100/0` mismatches.
- After correction, the live audit passed 1,700 planned segments without mismatch and began playback
  after 1,253 bytes.
- A normal batch run began playback after 1,671 bytes and reported
  `received/decoded/batched=66410/127/65567` at the 64 KiB checkpoint.

### Remaining Verification

A hidden automated run cannot listen to the audio or assess rendering and input. The user still
needs to confirm first-track quality and concurrent music and screen progression.
