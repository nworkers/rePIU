# 작업 지시: 버려진 옵트인 실험의 기능과 스위치 삭제 (issue #22)

설계: `docs/design/20261008-i022-delete-abandoned-opt-in-experiments.md`

1. 이전 동작 복원 스위치 5개 — `aot_dbt_dispatch.cpp`(segment write),
   `execution_trampoline.cpp`(first write), `aot_runtime_dispatch.cpp`·
   `aot_generation_failure_policy.h`(generation failure), `aot_code_cache.cpp`(strict
   spanning, patch wide protect).
2. post-HLE 번역 opt-in — `aot_dbt_dispatch.*`, `selector_guard_probe.cpp`.
3. segment-override hybrid 디스패치 — loader, `include/repiu/runtime/aot_code_cache.h`,
   `include/repiu/engine/aot_code_cache.h`, `src/runtime/aot_code_cache.cpp`,
   `src/runtime/aot_segment_patch.cpp`, `src/engine/aot_code_cache.cpp`,
   `selector_guard_probe.cpp`.
4. 간접 디스패치와 call-step 진단 — loader, AOT 헤더·방출·배치, `aot_dbt_indirect_dispatch.*`,
   `aot_dbt_call_step_probe.*`, thunk(direct·cache·Win32·Linux x86 `.S`), `thread_context.h`,
   `execution_trampoline.h/.cpp`, `veh_exit_site.h`, live snapshot, CMake, probe 3종.
5. native region — `execution_trampoline.cpp`, `verified_region_analyzer.*`,
   `native_fast_path.h`, region 상태를 읽던 곳, `execution_trampoline.h`, `veh_exit_site.h`,
   live snapshot.
6. span 캐시·쓰기·점프·retired reentry — `native_linear_span.*`, `verified_region_analyzer.*`,
   `native_fast_path.h`, `aot_runtime_dispatch.cpp`, `thread_context.h`,
   `execution_trampoline.h`, `execution_time_profile.h`, `live_telemetry.h`(버전 24),
   live snapshot, loader 로그, supervisor, `native_linear_span_probe.cpp`.
7. 스크립트 7개 삭제, `task347`에서 지워진 변수 한 줄 제거.
8. ARCHITECTURE, 가이드, analysis 목록, TODO, README 색인 갱신.
9. 검증: Linux x64·i386 Release 빌드, core probe, 변수 읽기 `grep`, 롬셋 스모크와 pumpitea를
   변경 전 빌드와 비교. Win32 빌드는 사용자 확인으로 넘긴다.

완료 기준: 변수 없이 실행했을 때의 동작이 변경 전과 같고, 코드가 13개 변수와
`REPIU_AOT_DBT_CALL_STEP`을 더 이상 읽지 않는다.

---

# Work order: delete the abandoned opt-in experiments and their switches (issue #22)

(1) The five rollback switches. (2) The post-HLE translation opt-in and its probe policy table.
(3) Segment-override hybrid dispatch (loader, both AOT headers, emission, segment patch,
placement, probe). (4) Indirect dispatch and the call-step diagnostic (loader, AOT headers,
emission and placement, handler and probe files, thunks for direct, cache, Win32 and Linux x86,
context and report fields, `VehExitSite`, snapshot, CMake, three probes). (5) Native region.
(6) Span cache, writes, jumps and retired reentry (span and analyzer code, state, counters,
time-profile bucket, shared telemetry version 24, snapshot, loader log, supervisor, span probe).
(7) Delete seven scripts; drop the deleted variable from `task347`. (8) ARCHITECTURE, guide,
analysis inventory, TODO, index updates. (9) Verify: Linux x64 and i386 Release builds, core
probe, `grep` for reads, romset smoke and pumpitea against the pre-change build; Win32 build left
to the user. Done when behavior with nothing set matches the pre-change build and the code no
longer reads the 13 variables or `REPIU_AOT_DBT_CALL_STEP`.
