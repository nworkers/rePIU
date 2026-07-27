# 20260727-327 작업 지시: 번역 워커 타이밍 계측 / Work order

설계: [docs/design/20260727-327-translation-worker-timing.md](../design/20260727-327-translation-worker-timing.md)

## 한국어

### 목표

동적 번역 rendezvous 한 번을 스케줄링 지연과 워커 CPU 작업으로 나눕니다. Task 326이
확인한 호출당 175ms의 정체를 확정합니다. 관측 전용입니다.

### 구현 항목

1. `include/repiu/platform/win32/aot_worker_timing.h`,
   `src/platform/win32/aot/aot_worker_timing.cpp` 신규 모듈
   - `Win32AotWorkerTimingProfile`: `request_signal_cycles`,
     `complete_signal_cycles`, `translate_count`, `wake_latency_cycles`,
     `segment_table_cycles`, `append_cycles`, `complete_latency_cycles`,
     `guest_total_cycles`, `max_*`, `clamped_sample_count`,
     `other_operation_count`, `request_gap_cycles`, `last_request_cycles`.
   - 스냅샷 함수와 누적 helper. **원자 연산과 잠금을 쓰지 않습니다.**
2. `src/platform/win32/execution/thread_context.h`
   - `std::unique_ptr<Win32AotWorkerTimingProfile> aot_worker_timing` 추가.
3. `src/platform/win32/execution/execution_trampoline.cpp`
   - `REPIU_EXECUTION_TIME_PROFILE` opt-in에서 함께 할당합니다.
4. `src/platform/win32/aot/aot_runtime_dispatch.cpp`
   - `RequestAotDynamicTranslation`: `SetEvent` 직전 `T0` 기록,
     `WaitForSingleObject` 복귀 직후 `T3` 기록 후 guest 구간 누적.
   - `AotTranslationWorkerProc`: 기상 직후 `T1` 기록, translate 분기에서
     `BuildWin32AotSegmentTable`과 `AppendWin32DynamicAotTranslation`을 각각 계측,
     `SetEvent(complete)` 직전 `T2` 기록. 비translate 작업은 횟수만 셉니다.
5. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`,
   `include/repiu/platform/win32/execution_trampoline.h`
   - 스냅샷을 attempt 결과로 복사.
6. `src/host/win32/main.cpp`
   - 구간별 tick/평균/최댓값과 파생 잔여, clamp 횟수, 기타 작업 횟수를 출력.
7. `src/tools/aot_probe/aot_worker_timing_probe.{h,cpp}` 신규 probe.
8. CMake와 `aot_probe/main.cpp`에 신규 파일 등록.

### 안전 조건

- guest에게 보이는 실행 순서, EIP, EFLAGS, 반환값을 바꾸지 않습니다.
- **계측에 원자 연산이나 잠금을 추가하지 않습니다.** 측정 대상이 지연이므로 계측이
  지연을 바꾸면 안 됩니다. `SetEvent`/`WaitForSingleObject`의 happens-before에만
  의존합니다.
- `T0` 기록은 `SetEvent(request)` **직전**, `T2` 기록은 `SetEvent(complete)`
  **직전**이어야 합니다. 순서가 바뀌면 지연이 작업 시간에 섞입니다.
- TSC 차분이 음수면 0으로 clamp하고 `clamped_sample_count`를 증가시킵니다.
- profile 기본값 OFF를 유지하고, 비활성 시 비용은 분기 하나입니다.
- 워커 shutdown 경로에서 누적하지 않습니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과
3. `REPIU_EXECUTION_TIME_PROFILE=1` 60초 `aot-dbt` 실행으로 분포 확보
4. `guest_total`을 Task 326의 `kAotDynamicTranslate`와 대조
5. profile OFF 대조 실행과 EEPROM hash 일치, fatal 0, malformed 0

---

## English

### Goal

Split one dynamic-translation rendezvous into scheduling latency and worker CPU work,
resolving what Task 326's 175ms per call consists of. Observation only.

### Implementation

Add an `aot_worker_timing` module with `Win32AotWorkerTimingProfile` covering the request and
complete signal timestamps, translate count, wake latency, segment table and append intervals,
complete latency, guest total, maxima, clamped-sample count, other-operation count, and request
spacing, plus snapshot and accumulation helpers using no atomics or locks. Hold it in
`ThreadContext`, allocated under the existing `REPIU_EXECUTION_TIME_PROFILE` opt-in. Record
`T0` immediately before `SetEvent(request)` and `T3` after the guest resumes in
`RequestAotDynamicTranslation`; record `T1` on worker wake, time
`BuildWin32AotSegmentTable` and `AppendWin32DynamicAotTranslation` separately, and record `T2`
immediately before `SetEvent(complete)` in `AotTranslationWorkerProc`, counting non-translate
operations only. Copy the snapshot into the attempt result, report intervals with means,
maxima, residual, clamp count, and other-operation count in the loader summary, and add a
probe, all registered in CMake and the probe main.

### Safety

Guest-visible ordering, EIP, EFLAGS, and return values stay unchanged. No atomics or locks are
added, because latency is the quantity being measured and instrumentation must not perturb it;
correctness rests on the happens-before supplied by the event pair. `T0` must be taken
immediately before `SetEvent(request)` and `T2` immediately before `SetEvent(complete)`, or
latency leaks into work time. Negative TSC differences clamp to zero and increment
`clamped_sample_count`. The profile stays off by default and costs one branch when inactive.
The worker shutdown path accumulates nothing.

### Verification

Build, pass `repiu_aot_probe`, capture the distribution from a 60-second `aot-dbt` run,
cross-check `guest_total` against Task 326's `kAotDynamicTranslate`, and confirm a matching
EEPROM hash with zero fatal and malformed dispatch against a profile-off control.
