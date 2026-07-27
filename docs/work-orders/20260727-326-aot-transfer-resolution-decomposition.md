# 20260727-326 작업 지시: AOT transfer 해석부 재분해 / Work order

설계: [docs/design/20260727-326-aot-transfer-resolution-decomposition.md](../design/20260727-326-aot-transfer-resolution-decomposition.md)

## 한국어

### 목표

`kVehAotTransfer`(VEH의 87.50%, 전체의 71.31%)를 handler 축 6개와 function 축 4개로
동시에 분해합니다. 관측 전용이며 실행 의미를 바꾸지 않습니다.

### 구현 항목

1. `include/repiu/platform/win32/execution_time_profile.h`
   - `ExecutionTimeBucket`에 다음을 **append**합니다(기존 인덱스 보존).
     - handler 축: `kAotWriteCompletion`, `kAotWriteFault`, `kAotReentry`,
       `kAotIndirect`, `kAotConditional`, `kAotReturn`
     - function 축: `kAotTransferResolve`, `kAotHleBoundaryScan`,
       `kAotDynamicTranslate`, `kAotResidency`
   - `kFirstAotHandlerBucket`, `kFirstAotFunctionBucket` 상수를 추가합니다.
2. `src/platform/win32/aot/aot_runtime_dispatch.cpp`
   - 여섯 handler 함수 본문 최상단에 각각 scope를 둡니다. 함수 scope이므로 조기
     `return`을 포함한 모든 경로가 닫힙니다.
   - `ResolveAotTransferTarget`, `IsAotHleBoundaryAddress`,
     `AccumulateAotResidency` 정의부에도 각각 scope를 둡니다.
   - `RequestAotDynamicTranslation` 호출 지점을 scope로 감쌉니다(정의부가 다른
     파일이면 호출부 계측으로 대체합니다).
3. `src/host/win32/main.cpp`
   - 두 축을 각각 `kVehAotTransfer` 대비 비율로 출력합니다. **두 축 합계를 서로
     더하지 않습니다.**
   - handler 축은 배타적이므로 파생 residual을 함께 출력합니다.
4. `src/tools/aot_probe/execution_time_profile_probe.cpp`
   - 신규 열거 인덱스 안정성과 두 축 누적을 검증에 추가합니다.

### 안전 조건

- guest에게 보이는 실행 순서, EIP, EFLAGS, 반환값을 바꾸지 않습니다.
- `ExecutionTimeBucket`의 기존 열거 값 순서를 바꾸지 않습니다(append만).
- 계측 scope는 heap 할당, 문자열 포맷, 파일 I/O를 하지 않습니다.
- 두 profile 기본값 OFF를 유지합니다.
- 새 bucket을 기존 `kVehExclusive`/`kUnaccounted`/`kVehResidual` 계산식에 더하지
  않습니다. handler 축과 function 축은 `kVehAotTransfer`의 분해입니다.
- `IsAotHleBoundaryAddress`는 `const ThreadContext*`를 받으므로 profile 접근 시
  const 정합성을 유지합니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1`
2. `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과
3. `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1 REPIU_EXECUTION_TIME_PROFILE=1`
   60초 `aot-dbt` 실행으로 두 축 분포 확보
4. 두 profile OFF 대조 실행과 EEPROM hash 일치, fatal 0, malformed 0
5. progress/heartbeat/phase를 함께 기록하되, 실행 간 편차가 크므로 구성비만
   해석합니다.

---

## English

### Goal

Decompose `kVehAotTransfer` (87.50% of VEH, 71.31% of wall clock) along a six-bucket handler
axis and a four-bucket function axis simultaneously. Observation only.

### Implementation

Append the handler buckets (`kAotWriteCompletion`, `kAotWriteFault`, `kAotReentry`,
`kAotIndirect`, `kAotConditional`, `kAotReturn`) and function buckets
(`kAotTransferResolve`, `kAotHleBoundaryScan`, `kAotDynamicTranslate`, `kAotResidency`) to
`ExecutionTimeBucket` with `kFirstAotHandlerBucket` and `kFirstAotFunctionBucket` constants,
preserving existing indices. Place a function-scope timer at the top of each of the six
handlers, of `ResolveAotTransferTarget`, `IsAotHleBoundaryAddress`, and
`AccumulateAotResidency`, and around the `RequestAotDynamicTranslation` call. Report each axis
as a share of `kVehAotTransfer` without adding the axes together, including a derived residual
for the mutually exclusive handler axis. Extend the profile probe with the new index-stability
and accumulation checks.

### Safety

Guest-visible ordering, EIP, EFLAGS, and return values stay unchanged. Enumeration order is
append-only. Scopes perform no allocation, string formatting, or file I/O. Both profiles stay
off by default. New buckets do not enter the existing `kVehExclusive`, `kUnaccounted`, or
`kVehResidual` formulas, since both axes decompose `kVehAotTransfer`. `IsAotHleBoundaryAddress`
takes a `const ThreadContext*`, so const correctness is preserved when reaching the profile.

### Verification

Build, pass `repiu_aot_probe`, capture both axes from a 60-second `aot-dbt` run with both
profiles enabled, and confirm a matching EEPROM hash with zero fatal and malformed dispatch
against a profiles-off control. Record progress, heartbeat, and phase alongside, but interpret
only composition given the observed run-to-run variance.
