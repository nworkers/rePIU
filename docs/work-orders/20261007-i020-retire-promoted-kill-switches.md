# 작업 지시: 오래 승격된 기능의 끄기 스위치 제거 (issue #20)

설계: `docs/design/20261007-i020-retire-promoted-kill-switches.md`

1. `src/host/loader/main.cpp` — AOT 빌드 토글 5개의 읽기 제거(`use_dynamic_backend`로).
2. `src/engine/aot/aot_runtime_dispatch.*`, `src/engine/execution/thread_context.h`,
   `include/repiu/engine/execution_trampoline.h`, `live_telemetry_snapshot.cpp`, loader 로그 —
   inline cache 패치 토글과 worker 패치 경로 제거.
3. `glide_setter_state_cache.*`, `linexe_glide_boundary.cpp` — setter 생략 토글 4개 제거.
4. `glide_draw_batch.*`, `linexe_glide_boundary.cpp` — draw batch 토글 제거.
5. `live_telemetry_snapshot.cpp` — host wait 토글 제거.
6. `linexe_glide_boundary.cpp` — gate pump 토글과 옛 `PumpEvents()` 호출 제거.
7. `port_io_delay_loop.*`, `port_io_emulator.cpp` — delay loop 토글 제거.
8. `native_linear_span.*` — reject cache 읽기 제거, 기존 기본값으로 고정.
9. probe 단언 갱신(setter cache·draw batch·native span).
10. README·ARCHITECTURE·가이드 3종·스크립트 2종 갱신.
11. 검증: 빌드, probe, 16개 롬셋 스모크, pumpitea 90초.

완료 기준: 변수 없이 실행했을 때의 동작이 변경 전과 같고(스모크·pumpitea), 15개 변수를
코드가 더 이상 읽지 않는다.

---

# Work order: retire the kill switches of long-promoted features (issue #20)

(1) Loader: drop the reads of the five AOT build toggles (`use_dynamic_backend`). (2) Inline
cache patch toggle and the worker patch path. (3) The four setter-elision toggles. (4) The draw
batch toggle. (5) The host wait toggle. (6) The gate pump toggle and the old `PumpEvents()`
call. (7) The delay loop toggle. (8) The reject cache read, fixed at its previous default.
(9) Probe assertions (setter cache, draw batch, native span). (10) README, ARCHITECTURE, three
guides and two scripts. (11) Verify: build, probes, 16-romset smoke, 90 s pumpitea. Done when
behavior with nothing set matches the pre-change results and the code no longer reads any of
the 15 variables.
