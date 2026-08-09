# 20260809-460 pumpito MP3 stream 연속성 진단 설계 / Pumpito MP3 Stream Continuity Audit Design

## 한국어

### 문제

Task 459 이후 사용자 실행은 `01.AUD` 전체 255,625 byte를 drop 없이 받았지만 MPEG frame은
8개만 decode했습니다. 최초 64 KiB에서도 4 frame만 decode되어 압축 stream sync가 초기에
깨졌습니다. 기존 감사 모드는 계획 구간 내부에서 guest byte와 batch 예측을 비교했지만,
구간 사이 연속성 및 SPSC producer와 worker consumer 사이의 실제 byte 순서는 검증하지
않았습니다.

### 설계

`REPIU_PIU10_MP3_STREAM_AUDIT=1`일 때 MP3 producer와 consumer가 서로 독립된 rolling hash
상태를 유지합니다. 각 상태는 실제로 성공한 enqueue byte와 실제로 pop한 byte만 순서대로
처리합니다. 4 KiB 경계마다 구간 번호, 누적 offset, 구간 hash를 한 줄로 기록합니다.
고정 크기 구간을 사용하므로 producer의 byte/span 호출 분할이나 consumer의 pop 크기가 달라도
같은 stream이면 같은 결과가 나옵니다.

```mermaid
flowchart LR
    G[guest byte / batch span] --> P[producer chunk hash]
    P --> R[SPSC ring]
    R --> C[consumer chunk hash]
    C --> D[minimp3 parser]
    P -. chunk 0..N 비교 .-> C
```

일반 batch 실행에서 producer와 consumer hash가 처음 달라지면 FIFO 구현 또는 enqueue/pop
경계를 교정합니다. 모든 hash가 같으면 동일 자산의 원본 byte-loop 감사 실행과 일반 batch
실행의 producer hash를 비교합니다. 이 비교가 다르면 batch 구간 사이의 누락·중복을,
같으면 decoder/parser 상태 처리를 조사합니다.

진단은 기본값 OFF이며 `pumpito` 전용 MP3 경로 외의 동작을 바꾸지 않습니다. hash는 진단
목적의 FNV-1a 64-bit를 사용하며 보안 성질을 요구하지 않습니다.

### 확인 결과와 frame-sync 교정

일반 batch 실행의 producer와 consumer hash는 62개 완성 구간에서 모두 일치하여 SPSC
전달은 원인이 아니었습니다. 그러나 정상 실행은 첫 구간 hash `7EAF...`와 64 KiB 시점
130 frame을, 실패 실행은 `4B70...`와 4 frame을 보였습니다. 원본 byte-loop 실행도 실패
hash와 같아 batch와 무관하게 guest 출력 자체가 두 상태로 갈립니다. 실패 시점은 약
1,900 byte 이후이며 원본 전송 보조 경계와 일치합니다.

MAME의 PIU10 보드는 MAS3507D frame-sync callback을 status bit 2에 연결하고, MAS3507D는
오디오 출력이 다음 MPEG frame을 실제로 필요로 할 때 sync를 전이합니다. 현재 rePIU는
worker가 SDL queue에 미리 decode한 frame마다 즉시 bit를 토글하여 약 250 ms를 선행하고,
worker scheduling에 따라 guest가 경계에서 읽는 상태가 달라집니다. PCM frame 시작 offset을
기록하고 SDL queue 소비량이 해당 offset에 도달할 때만 frame-sync를 토글하도록 교정합니다.

추가 A/B에서 실패 실행과 교정 실험은 62개 producer 구간이 모두 같았지만 decode 결과가
4 frame과 127 frame으로 갈렸습니다. `minimp3`는 stream sync 신뢰성을 위해 충분한 lookahead를
권장하며, 다음 header가 불완전한 짧은 입력에서는 `frame_bytes=0`을 반환합니다. 기존 worker는
이를 손상으로 해석하여 한 byte를 버리고 decoder를 reset했습니다. 공용 MPEG parser가 이미
현재 frame의 정확한 길이와 연속 header를 검증하므로, `minimp3`의 documented split-stream
계약에 따라 정확히 한 non-free-format frame만 전달합니다. 따라서 ring pop 분할은 decode
결과에 영향을 주지 않습니다.

### 검증 결과

정확한 frame 길이 decode를 적용한 뒤 독립적인 일반 batch 실행 두 번 모두 첫 64 KiB
체크포인트에서 127 frame을 decode했습니다. 각 실행의 received/decoded/batched 값은
`66408/127/65566`, `66409/127/65567`이었습니다. 두 실행 모두 producer와 consumer의
완성된 4 KiB 구간 62개가 전부 일치했고, 두 번째 실행의 hash 불일치는 0개였습니다.
이전 실패 전송에서 decoder 진행이 더 이상 worker FIFO pop 분할에 의존하지 않음을
확인했습니다. 실제 음질과 화면·게임 진행의 동시성은 사용자 환경에서 청감 검증이
남아 있습니다.

참고: MAME [PIU10 board](https://github.com/mamedev/mame/blob/master/src/mame/misc/xtom3d_piu10.cpp),
[MAS3507D device](https://github.com/mamedev/mame/blob/master/src/devices/sound/mas3507d.cpp).

## English

### Problem

After Task 459, the user's run received all 255,625 bytes of `01.AUD` without drops but decoded
only eight MPEG frames. Only four frames had decoded by the first 64 KiB, so compressed-stream sync
was already lost near the beginning. The previous audit compared guest bytes with batch prediction
inside each planned segment, but did not verify continuity between segments or the actual byte order
between the SPSC producer and worker consumer.

### Design

With `REPIU_PIU10_MP3_STREAM_AUDIT=1`, the MP3 producer and consumer maintain independent rolling
hash states. Each processes only bytes successfully enqueued or actually popped, in order. At every
4 KiB boundary it logs the chunk number, cumulative offset, and chunk hash. Fixed-size chunks produce
identical results for an identical stream regardless of producer byte/span call boundaries or
consumer pop sizes.

First compare producer and consumer hashes in a normal batch run. The first mismatch identifies the
FIFO or enqueue/pop boundary. If all hashes match, compare producer hashes between an original
byte-loop audit run and a normal batch run using the same asset. A difference then identifies an
inter-batch omission or duplication; equality directs investigation to decoder/parser state.

The diagnostic is OFF by default and changes no non-`pumpito` MP3 behavior. It uses 64-bit FNV-1a
as a diagnostic checksum and does not require cryptographic properties.

### Confirmed Result and Frame-Sync Correction

Producer and consumer hashes matched for all 62 complete chunks in a normal batch run, excluding
SPSC transport. A good run produced first hash `7EAF...` and 130 decoded frames at 64 KiB, while a
failed run produced `4B70...` and four frames. The original byte-loop run had the same failed hash,
showing that guest output itself forks independently of batching. Failure begins around byte 1,900,
coincident with the original auxiliary transfer boundary.

MAME connects the MAS3507D frame-sync callback to PIU10 status bit 2, and its MAS3507D transitions
sync when audio output actually requests the next MPEG frame. rePIU currently toggles immediately
for every frame the worker decodes ahead into the roughly 250 ms SDL queue. The status observed by
the guest at its boundary therefore depends on worker scheduling. Record each PCM frame's start
offset and toggle frame-sync only when SDL queue consumption reaches that offset.

A further A/B produced identical hashes for all 62 producer chunks but decoded four versus 127
frames. `minimp3` recommends sufficient lookahead for reliable stream sync and returns
`frame_bytes=0` for a short input ending with an incomplete next header. The worker incorrectly
treated that result as corruption, discarded one byte, and reset the decoder. The shared MPEG
parser already validates exact frame length and consecutive headers, so pass exactly one
non-free-format frame under `minimp3`'s documented split-stream contract. Ring-pop segmentation
then cannot affect decode results.

### Verification Result

After exact-frame decode was applied, two independent normal batch runs both decoded 127 frames
at the first 64 KiB checkpoint. The runs reported `66408/127/65566` and `66409/127/65567` for
received/decoded/batched bytes. In both runs, all 62 complete producer and consumer 4 KiB chunks
matched; the second run had zero hash mismatches. This proves that decoder progress no longer
depends on worker FIFO-pop segmentation for the previously failing transfer. Subjective audio
quality and concurrent gameplay still require user validation.
