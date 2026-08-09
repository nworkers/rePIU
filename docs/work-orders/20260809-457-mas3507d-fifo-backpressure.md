# 20260809-457 MAS3507D FIFO backpressure 작업 지시 / MAS3507D FIFO Backpressure Work Order

설계: [20260809-457-mas3507d-fifo-backpressure.md](../design/20260809-457-mas3507d-fifo-backpressure.md)

## 한국어

- [x] 사용자 실행 로그와 Task 456 구현에서 guest 공급 loop 점유 원인을 확인합니다.
- [x] MAME의 `0xE00` buffer는 계약 참고값으로만 사용하고 코드 비포함 원칙을 기록합니다.
- [x] 압축 SPSC ring을 4 KiB 물리 용량과 `0xE00` 논리 demand 한계로 변경합니다.
- [x] decoder worker가 PCM queue 상한에 도달하면 압축 FIFO를 비우지 않는지 확인합니다.
- [x] `pumpito`의 `0x212FD` frame 공급 loop와 count/source 상태 갱신을 분석합니다.
- [x] 검증된 loop에서 현재 frame tail을 한 번에 enqueue하는 fail-closed block HLE를 구현합니다.
- [x] block HLE가 source cursor, frame count와 `ECX`를 원본 byte loop 결과와 같게 갱신하는
  probe를 추가합니다.
- [x] probe에 demand deassert/reassert와 headroom 검증을 추가합니다.
- [x] Win32 x86 Debug build와 probe를 실행합니다.
- [x] `pumpito` 제한 실행을 시도하고 MP3 구간 도달 여부와 한계를 기록합니다.
- [x] 누적 분석, architecture와 작업 로그를 갱신하고 커밋합니다.
- [ ] 사용자 환경에서 음악과 화면의 동시 진행을 검증합니다.

## English

- [x] Confirm the guest feeder-loop occupancy from the user's runtime log and Task 456 code.
- [x] Record that MAME's `0xE00` buffer is contract reference only and no code is incorporated.
- [x] Change the compressed SPSC ring to 4 KiB physical capacity with a `0xE00` logical demand limit.
- [x] Confirm the decoder worker does not drain compressed input while the PCM queue is capped.
- [x] Analyze pumpito's frame feeder at `0x212FD` and its count/source state updates.
- [x] Implement fail-closed block HLE that enqueues the current frame tail for the verified loop.
- [x] Probe that block HLE updates the source cursor, frame count, and `ECX` exactly like the
  original byte loop.
- [x] Extend probes for demand deassertion/reassertion and race headroom.
- [x] Run the Win32 x86 Debug build and probes.
- [x] Attempt a bounded `pumpito` run and record whether it reaches the MP3 section and its limits.
- [x] Update cumulative analysis, architecture, and the work log, then commit.
- [ ] Validate concurrent music and rendering in the user's environment.
