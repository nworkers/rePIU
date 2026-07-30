# 작업 지시: 커널 예외 전달 비용 계측 / Work order: measure kernel exception-delivery cost

Task 372. 설계: [20260731-372](../design/20260731-372-kernel-exception-delivery-cost.md)

## 한국어

### 목표

VEH 핸들러 퇴출과 다음 진입 사이의 간격을 실측해, `unaccounted` 안의 커널 예외
전달 비용을 예외 종류별로 귀속한다. **새 clock 읽기를 추가하지 않는다.**

### 단계

1. **profile 확장** (`execution_time_profile.{h,cpp}`)
   * 상태: `veh_last_exit_cycles`, `veh_gap_pending_cycles`.
   * 누적: `veh_gap_cycles[3]` / `veh_gap_counts[3]`
     (single-step / breakpoint / other), `veh_gap_min_cycles`,
     `veh_gap_max_cycles`, `veh_gap_unclassified_cycles`,
     `veh_gap_clamped_count`.
   * `RecordVehExceptionGap(profile, class_index)` — pending을 해당 버킷으로 이동.
   * snapshot에 반영.

2. **scope 연결** (`ExecutionTimeScope`)
   * `kVehTotal` 생성자: `veh_last_exit_cycles != 0`이고 `start >= last_exit`이면
     pending에 차이를 넣고 총계·min·max 갱신. 역전이면 `clamped_count` 증가.
   * `kVehTotal` 소멸자(외곽 프레임만): `veh_last_exit_cycles = end_cycles`.
   * 중첩 예외에서 이중 계상되지 않도록 `owns_veh_depth_`를 따른다.

3. **분류 연결** (`execution_trampoline.cpp`)
   * `RecordVehExceptionCensus`에서 code에 따라 `RecordVehExceptionGap` 호출.
   * 그 지점은 record 검증 이후이므로 Task 296의 malformed record 위험이 없다.

4. **요약 출력**
   * `Win32 VEH gap cycles single-step/breakpoint/other/unclassified`
   * `Win32 VEH gap counts single-step/breakpoint/other`
   * `Win32 VEH gap min/max/clamped`
   * `Win32 VEH gap share of wall` (single-step 기준 왕복 추정)

5. **probe 추가**
   * `veh_exception_gap_probe.{h,cpp}` + `main.cpp` + `CMakeLists.txt`.
   * 검증: 분류 누적, min/max 추적, 역전 시 clamped 증가·누적 없음,
     pending 미분류 시 unclassified로 귀속, `nullptr` 무해.

6. **빌드·측정**
   * Debug + Release, probe exit 0.
   * **`REPIU_GLIDE_SWAP_INTERVAL=0` 고정**으로 자동 장면 70초 측정.
   * 합성 캘리브레이션(INT3 21,347 / single-step 25,855)과 교차 검증.

7. **문서**
   * 작업 로그, `docs/analysis/` 갱신(예외 축 재개 여부 판정 포함),
     `current-execution-frontier.md`.

### 완료 조건

* single-step gap의 평균과 최소가 출력되고, 합성 캘리브레이션과 같은 자릿수.
* 왕복 총량이 wall 비중으로 환산돼 `unaccounted` 분해가 가능해짐.
* probe 전 항목 통과, 양 구성 빌드 성공.
* 관측자 비용: hot path에 clock 읽기 추가 0회.

### 비범위

* exception-free dispatch 구현은 하지 않는다(판정 후 별도 작업).
* Task 365 batch 2, rendezvous 왕복 제거는 별건.

---

## English

Measure the interval between one VEH handler's exit and the next one's entry so the
kernel exception-delivery cost hiding inside `unaccounted` can be attributed by
exception class, **without adding a single clock read**. Extend the execution time
profile with the last-exit and pending-gap state, per-class accumulators for single
step, breakpoint, and other, plus minimum, maximum, unclassified, and clamped
counters. Bank the gap in the `kVehTotal` scope constructor and store the exit
timestamp in its destructor, following `owns_veh_depth_` so nested exceptions do not
double count. Classify from `RecordVehExceptionCensus`, which runs after the
exception record has been validated. Add summary lines, a probe covering
accumulation, extremes, clamping, unclassified fallback, and null safety, then build
both configurations and measure a 70-second automated scene with
`REPIU_GLIDE_SWAP_INTERVAL=0` pinned, cross-checking against the synthetic
calibration figures of 21,347 and 25,855 cycles. Implementing exception-free
dispatch is explicitly out of scope until the measurement decides.
