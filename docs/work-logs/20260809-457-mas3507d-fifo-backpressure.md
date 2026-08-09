# 20260809-457 MAS3507D FIFO backpressure 작업 로그 / MAS3507D FIFO Backpressure Work Log

설계: [20260809-457-mas3507d-fifo-backpressure.md](../design/20260809-457-mas3507d-fifo-backpressure.md)
작업 지시: [20260809-457-mas3507d-fifo-backpressure.md](../work-orders/20260809-457-mas3507d-fifo-backpressure.md)

## 한국어

### 결과

- 사용자 로그의 108-byte ring high-water와 210회 starvation을 근거로, 끊김과 실행 정지의
  직접 원인을 매-byte privileged `OUT` 예외 처리량 부족으로 좁혔습니다.
- SPSC ring을 4 KiB 물리 용량으로 줄이고 `DEMAND` 해제 수위를 `0xE00` byte로 분리했습니다.
- exact `pumpito` frame 공급 loop에서 현재 frame tail을 span으로 enqueue하고 source cursor,
  frame count와 `ECX`를 원본 반복 결과대로 갱신하는 fail-closed block HLE를 추가했습니다.
- 이 최적화는 ROM ZIP stem이 `pumpito`일 때만 활성화되며, signature·relocation·범위·상태가
  하나라도 맞지 않으면 기존 byte 경로를 유지합니다.
- `minimp3` worker는 유지하고 압축 입력 drain 단위를 512 byte로 제한했습니다. MAME 코드는
  포함하지 않았으며 `0xE00` 입력 buffer 계약만 참고했습니다.

### 검증

- `cmake --build build\win32_x86_debug --config Debug --target repiu repiu_aot_probe`: 성공
- `repiu_aot_probe.exe --piu10`: 성공
  - `piu10_mp3_ring=true,demand_low=true,demand_high=true,high_water=3584`
  - `piu10_mp3_frame_batch=true,bytes=4,ecx=103`
- `git diff --check`: 오류 없음. 기존 checkout 정책에 따른 LF→CRLF 경고만 출력되었습니다.
- 60초와 90초 supervisor 실행은 startup 편차로 MP3 본 전송에 도달하지 못했습니다. 90초
  실행은 Glide gate/open/texture/draw/swap을 모두 관찰했지만 block-HLE 활성화 로그 전까지
  제한 시간이 끝났습니다.

### 남은 검증

사용자 환경에서 `pumpito`를 실행하여 음악 중 화면·입력·장면 전환이 계속되는지 확인해야
합니다. 로그에는 `verified frame-tail batch active`, 0이 아닌 `batched`, `dropped=0`, 이전보다
크게 감소한 `starved`가 기대됩니다.

## English

### Result

- The user's 108-byte ring high-water and 210 starvation events identified per-byte privileged
  `OUT` exception throughput as the direct cause of both stutter and stalled guest progress.
- The SPSC ring now has a 4 KiB physical capacity with a separate logical `0xE00`-byte `DEMAND`
  threshold.
- A fail-closed block HLE at the exact `pumpito` frame feeder enqueues the current frame tail as a
  span and updates the source cursor, frame count, and `ECX` like the original iterations.
- The optimization is enabled only when the ROM ZIP stem is `pumpito`; any signature, relocation,
  range, or state mismatch preserves the byte path.
- The persistent `minimp3` worker remains, with compressed input drained in 512-byte chunks. No
  MAME code was incorporated; only the `0xE00` input-buffer contract was referenced.

### Verification

- `cmake --build build\win32_x86_debug --config Debug --target repiu repiu_aot_probe`: passed
- `repiu_aot_probe.exe --piu10`: passed
  - `piu10_mp3_ring=true,demand_low=true,demand_high=true,high_water=3584`
  - `piu10_mp3_frame_batch=true,bytes=4,ecx=103`
- `git diff --check`: no errors; only LF-to-CRLF warnings from the existing checkout policy.
- Supervisor runs bounded to 60 and 90 seconds did not reach the main MP3 transfer because of
  startup variation. The 90-second run observed all Glide gate/open/texture/draw/swap milestones,
  but reached its deadline before the block-HLE activation log.

### Remaining Validation

Run `pumpito` in the user's environment and confirm that rendering, input, and scene transitions
continue during music. Expected evidence is `verified frame-tail batch active`, a nonzero
`batched` count, `dropped=0`, and substantially fewer `starved` events than before.
