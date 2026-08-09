# 20260809-456 MAS3507D 지속형 MP3 HLE 작업 지시 / Persistent MAS3507D MP3 HLE Work Order

설계: [20260809-456-mas3507d-streaming-hle.md](../design/20260809-456-mas3507d-streaming-hle.md)

handoff: [20260809-456-mas3507d-streaming-hle-handoff.md](../work-logs/20260809-456-mas3507d-streaming-hle-handoff.md)

## 한국어

- [x] `bf71997`과 Task 454/455 runtime 증거를 재확인합니다.
- [x] upstream `minimp3` commit과 CC0-1.0 license를 고정합니다.
- [x] bounded SPSC 압축 byte ring과 demand/backpressure probe를 추가합니다.
- [x] `Piu10Mp3AudioOut`을 persistent `mp3dec_t` worker + SDL3 PCM stream으로 교체합니다.
- [x] SDL3_mixer FetchContent, link와 PIU10 관련 사용을 제거합니다.
- [x] destination `0x008` `OUT DX,AL` 전용 AOT/arena fast path를 구현합니다.
- [x] fast-path byte count, starvation과 ring high-water telemetry를 추가합니다.
- [x] 공용 board demand status를 ring 여유 공간과 연결합니다.
- [ ] 전체 build/probe와 pumpito 60초 이상 청취·게임 진행을 검증합니다.
- [x] pumpit1/2/3 비활성 범위 회귀를 검증합니다.
- [x] architecture, analysis, work log와 third-party notice를 최종 갱신합니다.
- [x] MAME 구현을 참고 자료로만 사용했고 해당 코드가 포함되지 않았음을 검토합니다.

우선 확인할 코드는 `piu10_mp3_audio_out`, `port_io_emulator`, `src/platform/win32/aot/`,
`piu10_isa_board`, `thread_context`, `CMakeLists.txt`, `THIRD_PARTY_NOTICES.md`입니다.

## English

- [x] Reconfirm `bf71997` and Task 454/455 runtime evidence.
- [x] Pin upstream `minimp3` and its CC0-1.0 terms.
- [x] Add a bounded compressed-byte SPSC ring and demand/backpressure probes.
- [x] Replace `Piu10Mp3AudioOut` with a persistent `mp3dec_t` worker and SDL3 PCM stream.
- [x] Remove SDL3_mixer FetchContent, linking, and PIU10 usage.
- [x] Implement dedicated AOT/arena fast paths for destination-`0x008` `OUT DX,AL`.
- [x] Add fast-path byte totals, starvation, and ring high-water telemetry.
- [x] Connect board demand status to ring capacity.
- [ ] Verify the full build/probes and at least 60 seconds of pumpito listening/game progress.
- [x] Verify pumpit1/2/3 remain outside the capability.
- [x] Finalize architecture, analysis, work log, and third-party notices.
- [x] Review that MAME was reference-only and no MAME code was incorporated.

Inspect `piu10_mp3_audio_out`, `port_io_emulator`, `src/platform/win32/aot/`, `piu10_isa_board`,
`thread_context`, `CMakeLists.txt`, and `THIRD_PARTY_NOTICES.md` first.
