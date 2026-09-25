# Task 729 작업 지시: LFB lock 구간 구성 census와 buffer별 shadow 쌍

설계: [20260922-729](../design/20260922-729-lfb-lock-interval-and-buffer-pair-shadow.md)

## 한국어

### 1단계 (이 지시의 범위)

1. `include/repiu/engine/glide_lfb_lock_interval_census.h` 신설
   * `GlideLfbLockIntervalCensus`: 구간 누적기(`swap_count`, `clear_count`, `draw_count`,
     `region_count`, `other_count`, `interval_open`)와 분류 결과
     (`clean_interval_count`, `cleared_interval_count`, `drawn_interval_count`,
     `region_interval_count`, `first_lock_count`), swap 분포
     (`swap_total`, `swap_minimum`, `swap_maximum`, `single_swap_interval_count`,
     `multi_swap_interval_count`, `zero_swap_interval_count`).
   * 정책 `ResolveGlideLfbLockIntervalCensusSetting`,
     `GlideLfbLockIntervalCensusEnabled` (`REPIU_GLIDE_LFB_LOCK_INTERVAL_CENSUS`).
   * `NoteGlideLfbLockIntervalGate(census, gate_id)` — 구간이 열려 있을 때만 계수.
   * `ClassifyGlideLfbLockInterval(census)` — lock 시점에 호출, 분류 후 누적기 리셋.
   * `OpenGlideLfbLockInterval(census)` — unlock에서 구간 시작.
   * snapshot과 분류 이름.
2. `src/engine/glide_lfb_lock_interval_census.cpp` 신설. 동적 할당 없음.
3. `ThreadContext`에 census 상태 추가.
4. `linexe_glide_boundary.cpp` 통합
   * gate dispatch 지점에서 `NoteGlideLfbLockIntervalGate` 호출.
   * `kGrLfbLock` write 경로에서 `ClassifyGlideLfbLockInterval`.
   * `kGrLfbUnlock` write 경로의 present 성공 뒤 `OpenGlideLfbLockInterval`.
5. snapshot 필드와 `live_telemetry_snapshot.cpp`, `src/host/win32/main.cpp` 보고 추가.
6. `repiu_aot_probe --glide-lfb-lock-interval` probe와 `CMakeLists.txt` 갱신.
7. 문서: `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`, 작업 로그.

### 1단계 검증

* Win32 x86 Debug 전체 빌드, 전용 probe, core probe.
* WSL Linux x64 빌드, core probe, 30초 bounded `pumpit2a` census 관찰
  (`REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=30000`).

### 2단계 진입 조건

1단계 관찰에서 `clean` 구간이 0이면 **2단계를 구현하지 않습니다.** 그 경우 shadow 쌍도
반증된 것으로 기록하고, lock의 readback이 제거 대상이 아니라 **비용 절감 대상**임을 결론으로
남깁니다.

### 하지 않을 것

* 어떤 기본값도 켜지 않습니다.
* Task 728의 shadow 상태와 동작을 바꾸지 않습니다.
* `grBufferSwap` 비용 분해와 `ReadbackFramebuffer` resample 수정은 별도 작업입니다.

---

## English

### Stage 1 (the scope of this order)

1. New `include/repiu/engine/glide_lfb_lock_interval_census.h`
   * `GlideLfbLockIntervalCensus`: the interval accumulator (`swap_count`, `clear_count`,
     `draw_count`, `region_count`, `other_count`, `interval_open`), classification results
     (`clean_interval_count`, `cleared_interval_count`, `drawn_interval_count`,
     `region_interval_count`, `first_lock_count`) and the swap distribution (`swap_total`,
     `swap_minimum`, `swap_maximum`, `single_swap_interval_count`, `multi_swap_interval_count`,
     `zero_swap_interval_count`).
   * Policy `ResolveGlideLfbLockIntervalCensusSetting` and
     `GlideLfbLockIntervalCensusEnabled` (`REPIU_GLIDE_LFB_LOCK_INTERVAL_CENSUS`).
   * `NoteGlideLfbLockIntervalGate(census, gate_id)`, counting only while an interval is open.
   * `ClassifyGlideLfbLockInterval(census)` at a lock, classifying and then resetting.
   * `OpenGlideLfbLockInterval(census)` at an unlock.
   * A snapshot and classification names.
2. New `src/engine/glide_lfb_lock_interval_census.cpp`, with no dynamic allocation.
3. Add the census state to `ThreadContext`.
4. Integrate in `linexe_glide_boundary.cpp`: note each gate at dispatch, classify at the write
   lock, and open the interval after a successful write unlock present.
5. Add the snapshot field, fill it in `live_telemetry_snapshot.cpp`, and report it in
   `src/host/win32/main.cpp`.
6. Add the `repiu_aot_probe --glide-lfb-lock-interval` probe and update `CMakeLists.txt`.
7. Documentation: `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`, and the work log.

### Stage 1 verification

* Full Win32 x86 Debug build, the dedicated probe, and the core probe.
* WSL Linux x64 build, core probe, and a 30-second bounded `pumpit2a` census observation
  (`REPIU_STALL_TIMEOUT_MS=0 REPIU_EXECUTION_TIMEOUT_MS=30000`).

### Entry condition for stage 2

If the stage 1 observation reports zero `clean` intervals, **stage 2 is not implemented.** The
shadow pair is then recorded as refuted too, with the conclusion that a lock's readback is not
something to remove but something to make cheaper.

### Not to be done

* Do not turn on any default.
* Do not change Task 728's shadow state or behavior.
* Decomposing `grBufferSwap` cost and changing `ReadbackFramebuffer` resampling are separate
  tasks.
