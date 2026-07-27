# 20260727-328 작업 지시: 동적 append 단계 분해 / Work order

설계: [docs/design/20260727-328-dynamic-append-phase-decomposition.md](../design/20260727-328-dynamic-append-phase-decomposition.md)

## 한국어

### 목표

`AppendWin32DynamicAotTranslation`(번역 rendezvous의 101.00%, 회당 약 259ms)을 다섯
단계로 나누고 번역 1회의 규모를 함께 기록합니다. arena 전체 스냅샷이 주범이라는
가설을 확인하거나 기각합니다. 관측 전용입니다.

### 구현 항목

1. `include/repiu/platform/win32/aot_worker_timing.h`
   - `Win32AotWorkerTimingProfile`과 스냅샷에 단계 필드를 추가합니다.
     `arena_snapshot_cycles`, `plan_build_cycles`, `image_emit_cycles`,
     `validate_cycles`, `placement_cycles`, `append_phase_count`.
   - 규모 필드를 추가합니다. `plan_block_total`, `plan_instruction_total`,
     `emitted_byte_total`, `snapshot_byte_total`, `max_plan_instruction_count`.
   - 누적 helper `RecordAotAppendPhases`, `RecordAotAppendScale`.
2. `include/repiu/platform/win32/aot_code_cache_win32.h`,
   `src/platform/win32/aot_code_cache_win32.cpp`
   - `AppendWin32DynamicAotTranslation`에 선택적 마지막 인자
     `Win32AotWorkerTimingProfile* timing = nullptr`를 추가합니다.
   - 다섯 구간을 계측합니다. plan build와 image emit은 한 `if` 안의 단축 평가이므로
     **지역 변수로 분리해 순서를 보존**한 뒤 각각 계측합니다.
   - 성공 여부와 무관하게 도달한 구간까지 누적합니다.
3. `src/platform/win32/aot/aot_runtime_dispatch.cpp`
   - 워커가 `context->aot_worker_timing.get()`을 전달합니다.
4. `src/host/win32/main.cpp`
   - 단계별 tick/비율/파생 잔여와 규모(평균 블록·명령·emit 바이트, 스냅샷 바이트)를
     출력합니다.
5. `src/tools/aot_probe/aot_worker_timing_probe.cpp`
   - 단계·규모 누적과 profile 없는 호출 경로를 검증에 추가합니다.

### 안전 조건

- guest에게 보이는 실행 순서, 반환값, `result` 내용을 바꾸지 않습니다.
- 단축 평가를 지역 변수로 옮길 때 **평가 순서와 부수효과 순서를 보존**합니다.
  `BuildAotTranslationPlanFromEntry`가 실패하면 `BuildAotCodeCacheImage`를 호출하지
  않아야 합니다.
- 이 함수는 워커 스레드 전용이므로 **원자 연산이나 잠금을 추가하지 않습니다.**
  guest는 완료 이벤트 이후에만 읽습니다.
- `repiu_aot_probe`가 이 함수를 호출하므로 `timing == nullptr` 경로가 기존과
  동일하게 동작해야 합니다.
- 기존 호출 지점의 인자 순서를 바꾸지 않습니다(마지막에 기본값 인자 추가).

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과
3. `REPIU_EXECUTION_TIME_PROFILE=1` 60초 `aot-dbt` 실행으로 단계·규모 분포 확보
4. 단계 합계를 Task 327의 `append`와 대조
5. profile OFF 대조 실행과 EEPROM hash 일치, fatal 0, malformed 0

---

## English

### Goal

Split `AppendWin32DynamicAotTranslation` — 101.00% of the translation rendezvous at about
259ms per call — into five phases and record what one translation covers, confirming or
rejecting the hypothesis that the full-arena snapshot dominates. Observation only.

### Implementation

Extend `Win32AotWorkerTimingProfile` and its snapshot with phase fields
(`arena_snapshot_cycles`, `plan_build_cycles`, `image_emit_cycles`, `validate_cycles`,
`placement_cycles`, `append_phase_count`) and scale fields (`plan_block_total`,
`plan_instruction_total`, `emitted_byte_total`, `snapshot_byte_total`,
`max_plan_instruction_count`), plus accumulation helpers. Add an optional trailing
`Win32AotWorkerTimingProfile* timing = nullptr` parameter to
`AppendWin32DynamicAotTranslation` and instrument the five phases, splitting the plan build
and image emit out of their shared short-circuit `if` into sequential locals so ordering is
preserved. Pass the profile from the worker, report phases, residual, and scale in the loader
summary, and extend the probe.

### Safety

Guest-visible ordering, return values, and `result` contents stay unchanged. Moving the
short-circuit into locals must preserve evaluation and side-effect order, so
`BuildAotCodeCacheImage` is not called when the plan build fails. The function runs only on the
worker thread, so no atomics or locks are added; the guest reads only after the completion
event. `repiu_aot_probe` calls this function, so the `timing == nullptr` path must behave
exactly as before, and existing call sites keep their argument order because the new parameter
is trailing and defaulted.

### Verification

Build, pass `repiu_aot_probe`, capture phase and scale distributions from a 60-second
`aot-dbt` run, cross-check the phase total against Task 327's `append`, and confirm a matching
EEPROM hash with zero fatal and malformed dispatch against a profile-off control.
