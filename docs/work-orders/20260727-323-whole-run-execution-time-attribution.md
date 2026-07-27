# 20260727-323 작업 지시: 전체 실행 시간 귀속과 kAotResume 분해 / Work order

설계: [docs/design/20260727-323-whole-run-execution-time-attribution.md](../design/20260727-323-whole-run-execution-time-attribution.md)

## 한국어

### 목표

두 개의 미지수를 동시에 계측합니다. Part A는 `kAotResume` 16.2%의 내역을, Part B는
미계측 wall-clock 78%의 내역을 구합니다. 관측 전용이며 실행 의미를 바꾸지 않습니다.
Task 322의 잘못된 인과 귀속과 그로부터 도출한 로드맵 결론을 문서에서 정정합니다.

### 구현 항목

**Part A — `kAotResume` 4구간 분해**

1. `single_step_hotspot_profile.h`
   - `SingleStepProfileStage`에 `kSegmentWriteProbe`, `kQuarantineCheck`,
     `kCacheLookup`, `kSpanSafety` 추가. 기존 5개 단계 뒤에 append하여 배열 인덱스
     호환을 유지합니다.
   - `SingleStepHotspotStageScope`에 null 허용 포인터 생성자 추가.
2. `thread_context.h`
   - `SingleStepHotspotCycleScope* active_hotspot_scope` 필드 추가.
3. `execution_trampoline.cpp`
   - `HandleSingleStepTrace`에서 활성 scope 포인터를 설정하고 종료 시 해제.
4. `aot_dbt_dispatch.cpp`
   - `TryResumeAotAfterHandledHle`의 네 구간을 하위 단계 scope로 감쌈.

**Part B — guest thread wall-clock 귀속**

5. `include/repiu/platform/win32/execution_time_profile.h`,
   `src/platform/win32/telemetry/execution_time_profile.cpp` 신규 모듈
   - bucket enum(`kGuestRunTotal`, `kVehTotal`, `kGlideGate`, `kPortIoDevice`,
     `kDosService`), 누적기, RAII scope, VEH 깊이 추적, snapshot, opt-in 판정.
   - opt-in은 `REPIU_EXECUTION_TIME_PROFILE`, 기본 OFF.
6. 계측 지점 연결
   - `GuestEntryThreadProc`의 guest 실행 구간 → `kGuestRunTotal`
   - `DispatchGuestException` → `kVehTotal`
   - `HandleGlideGateBoundary` → `kGlideGate`
   - `HandlePortIoInstruction` → `kPortIoDevice`
   - traced DOS/DPMI/mouse interrupt handler → `kDosService`
7. `main.cpp` 종료 summary에 bucket별 tick/count, VEH 안/밖 진입 횟수,
   파생 `kVehExclusive`와 `kUnaccounted` 출력.

**Part B-2 — kernel 예외 전이 교정 probe**

8. `src/tools/aot_probe/exception_transition_calibration_probe.{h,cpp}` 신규
   - 알려진 횟수의 `INT3`와 TF single-step을 사소한 VEH로 처리하고 wall clock을
     횟수로 나눠 전이 1회 비용을 출력합니다.
9. CMake Win32 source 목록에 신규 파일 추가.

**문서 정정**

10. Task 322 설계 문서 상단에 정정 note, `docs/analysis/`의 두 문서에 정정 반영.

### 안전 조건

- guest에게 보이는 실행 순서, EIP, EFLAGS, 반환값을 바꾸지 않습니다.
- 계측 scope는 heap 할당, 문자열 포맷, 파일 I/O를 하지 않습니다.
- 두 profile 모두 기본 OFF이며, 비활성 시 비용은 분기 하나입니다.
- 기존 `SingleStepProfileStage` 인덱스 순서를 바꾸지 않습니다(append만).
- Part A 하위 단계는 `kAotResume` 안에서만 열리며
  `sum(sub-stage) <= kAotResume`을 유지합니다.
- Part B bucket은 중첩을 허용하되 총량과 진입 횟수를 분리 기록하고, 배타성은 보고
  시점의 파생값으로만 표현합니다.
- 교정 probe는 guest 이미지나 실행 상태를 건드리지 않는 독립 합성 코드입니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과
3. `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1 REPIU_EXECUTION_TIME_PROFILE=1`
   60초 `aot-dbt` 실행으로 Part A/B 분포 확보
4. 두 profile OFF 대조 실행과 EEPROM hash 일치, fatal 0, malformed 0 확인

---

## English

### Goal

Measure both open unknowns at once: Part A resolves the composition of the 16.2% held by
`kAotResume`, and Part B attributes the roughly 78% of guest-thread wall clock never yet
measured. Observation only. The task also corrects Task 322's false causal attribution and
the roadmap conclusion drawn from it.

### Implementation

Part A appends `kSegmentWriteProbe`, `kQuarantineCheck`, `kCacheLookup`, and `kSpanSafety`
to `SingleStepProfileStage` (append only, preserving existing array indices), adds a
null-tolerant pointer constructor to `SingleStepHotspotStageScope`, threads the active cycle
scope through `ThreadContext`, and wraps the four regions of
`TryResumeAotAfterHandledHle`.

Part B adds an `execution_time_profile` module with bucket accumulators, a RAII scope, VEH
depth tracking, snapshotting, and a `REPIU_EXECUTION_TIME_PROFILE` opt-in, wired at the
guest run window, `DispatchGuestException`, `HandleGlideGateBoundary`,
`HandlePortIoInstruction`, and the traced DOS/DPMI/mouse handlers, with the loader summary
printing bucket ticks and counts plus derived `kVehExclusive` and `kUnaccounted`.

Part B-2 adds a synthetic calibration probe that prices one `INT3` and one TF single-step
round trip through a trivial VEH. New sources join the Win32 CMake list, and the Task 322
design and both analysis documents receive the correction.

### Safety

Guest-visible ordering, EIP, EFLAGS, and return values are unchanged. Instrumentation scopes
perform no allocation, string formatting, or file I/O. Both profiles default off and cost one
branch when inactive. Existing stage indices are preserved by appending only. Part A
sub-stages open only inside `kAotResume` and maintain `sum(sub-stage) <= kAotResume`. Part B
buckets may nest, recording totals and entry counts separately, with exclusivity expressed
only as a derived reporting value. The calibration probe is self-contained and never touches
guest image or execution state.

### Verification

Run the full Win32 x86 Debug build, pass `repiu_aot_probe`, capture both distributions from a
60-second `aot-dbt` run with both profiles enabled, and confirm a matching EEPROM hash with
zero fatal and zero malformed dispatch against a profiles-off control.
