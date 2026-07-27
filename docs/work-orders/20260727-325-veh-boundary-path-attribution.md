# 20260727-325 작업 지시: VEH boundary 경로 비용 귀속 / Work order

설계: [docs/design/20260727-325-veh-boundary-path-attribution.md](../design/20260727-325-veh-boundary-path-attribution.md)

## 한국어

### 목표

`DispatchGuestException` 본문에서 아직 계측되지 않은 구간을 5개 bucket으로 나눕니다.
Task 324가 원인 가설을 기각한 뒤 남은 전체 wall-clock의 74.34% 블록 내부를 처음으로
귀속합니다. 관측 전용이며 실행 의미를 바꾸지 않습니다.

### 구현 항목

1. `include/repiu/platform/win32/execution_time_profile.h`
   - `ExecutionTimeBucket`에 `kVehPrologue`, `kVehAotTransfer`, `kVehTelemetry`,
     `kVehBoundaryGates`, `kVehHleChain`을 **append**합니다(기존 인덱스 보존).
2. `src/platform/win32/execution/execution_trampoline.cpp`
   - `kVehPrologue`: 포인터/스레드 검증부터 native region 처리까지.
   - `kVehAotTransfer`: `HandleAotGuestCodeWriteCompletion`,
     `HandleAotGuestCodeWriteFault`, `HandleAotReentry`,
     `HandleAotIndirectTransfer`, `HandleAotConditionalTransfer`,
     `HandleAotReturnTransfer`.
   - `kVehTelemetry`: live telemetry `InterlockedExchange` 블록과
     `RecordAllocatorControlFlowException`.
   - `kVehBoundaryGates`: `HandleTimerInterruptChainBoundary`,
     `HandleLinexeFarTransferBoundary`.
   - `kVehHleChain`: `HandleSingleStepTrace` 이후의 HLE 핸들러 체인 전체.
   - 각 구간은 조기 `return`이 많으므로 RAII scope를 블록으로 감싸되, 반환값을
     지역 변수에 받아 scope 종료 후 분기하는 방식으로 구현합니다.
3. `src/host/win32/main.cpp`
   - VEH 하위 bucket별 tick/count와 파생 `kVehResidual`을 출력합니다.
   - 기존 `kVehExclusive`/`kUnaccounted` 계산식은 **바꾸지 않습니다**(새 bucket은
     `kVehTotal`의 분해이지 추가 항목이 아님).
4. `src/tools/aot_probe/execution_time_profile_probe.{h,cpp}` 신규 probe
   - bucket 누적, VEH 깊이 추적, `sum(VEH 하위 bucket) <= kVehTotal` 불변식,
     비활성 profile 무누적을 검증합니다.
5. CMake와 `aot_probe/main.cpp`에 신규 probe 등록.

### 안전 조건

- guest에게 보이는 실행 순서, EIP, EFLAGS, 반환값을 바꾸지 않습니다. 특히 조기
  `return`을 지역 변수로 옮길 때 **단축 평가 순서와 부수효과 순서를 보존**합니다.
- `ExecutionTimeBucket`의 기존 열거 값 순서를 바꾸지 않습니다(append만).
- 계측 scope는 heap 할당, 문자열 포맷, 파일 I/O를 하지 않습니다.
- profile 기본값 OFF를 유지하고, 비활성 시 비용은 분기 하나입니다.
- 새 VEH 하위 bucket을 `kVehExclusive`/`kUnaccounted` 계산에 더하지 않습니다.
- bucket 중첩을 허용하되 총량과 진입 횟수를 분리 기록합니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과
3. `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1 REPIU_EXECUTION_TIME_PROFILE=1`
   60초 `aot-dbt` 실행으로 분포 확보
4. 두 profile OFF 대조 실행과 EEPROM hash 일치, fatal 0, malformed 0
5. progress/heartbeat/phase를 함께 기록하고, 구성비를 같은 작업량 비교로 제시하지
   않습니다.

---

## English

### Goal

Divide the still-unmeasured parts of `DispatchGuestException` into five buckets, attributing
for the first time the 74.34% block that remained after Task 324 rejected its cause
hypothesis. Observation only.

### Implementation

Append `kVehPrologue`, `kVehAotTransfer`, `kVehTelemetry`, `kVehBoundaryGates`, and
`kVehHleChain` to `ExecutionTimeBucket`, preserving existing indices. Wrap the corresponding
regions of `DispatchGuestException`, taking each handler's result into a local so the scope
closes before branching, since these regions return early. Print per-bucket ticks and counts
plus a derived `kVehResidual` in the loader summary without changing the existing
`kVehExclusive` and `kUnaccounted` formulas, because the new buckets decompose `kVehTotal`
rather than adding to it. Add an `execution_time_profile` probe covering bucket accumulation,
VEH depth tracking, the `sum(VEH sub-buckets) <= kVehTotal` invariant, and no accumulation
when disabled, registered in CMake and the probe main.

### Safety

Guest-visible ordering, EIP, EFLAGS, and return values stay unchanged; in particular, moving
early returns into locals must preserve short-circuit and side-effect ordering. Enumeration
order is append-only. Scopes perform no allocation, string formatting, or file I/O. Both
profiles stay off by default and cost one branch when inactive. New sub-buckets do not enter
the `kVehExclusive` or `kUnaccounted` formulas. Buckets may nest, recording totals and entry
counts separately.

### Verification

Build, pass `repiu_aot_probe`, capture the distribution from a 60-second `aot-dbt` run with
both profiles enabled, and confirm a matching EEPROM hash with zero fatal and malformed
dispatch against a profiles-off control. Record progress, heartbeat, and phase alongside, and
do not present composition shares as an equal-work comparison.
