# Task 635 작업 지시: x64 GS override selector 해석 측정

설계: [20260908-635](../design/20260908-635-x64-gs-override-selector-resolution.md)

## 한국어

### 범위

`ReResolveAotSegmentOverrides`에 opt-in segment 해석 추적을 추가하고,
`pumpit2a` 실행에서 GS selector `0x0080`의 실제 descriptor와 policy를
측정합니다. segment override 방출이나 실행 정책은 바꾸지 않습니다.

### 구현 단계

1. `src/engine/aot/aot_runtime_dispatch.cpp`에
   `REPIU_AOT_SEGMENT_RESOLUTION_TRACE` 환경 변수로 제어되는 진단을 추가합니다.
2. 해석이 바뀐 경우에만 ES, CS, SS, DS, FS, GS 각각의 shadow address,
   selector, base, limit, flags, policy를 `stderr`에 출력합니다.
3. 환경 변수가 없거나 값이 `0`이면 출력하지 않습니다.
4. `pumpit2a`를 추적과 함께 실행하여 fault 직전 GS 해석이 설계의 갈래 A와
   B 중 어느 것인지 확정합니다.

### 금지 사항

* `LongModeSegmentOverrideEmittable`의 FS/GS 허용 범위를 바꾸지 않습니다.
* segment override ModRM 형식이나 patch policy를 바꾸지 않습니다.
* i386 경로와 기본 실행 출력을 바꾸지 않습니다.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0` 확인.
3. 환경 변수 없이 짧게 실행하여 새 trace가 출력되지 않는지 확인.
4. `REPIU_AOT_SEGMENT_RESOLUTION_TRACE=1 ./build/linux_x64/repiu pumpit2a`를
   실행하여 GS selector `0x0080`의 base, limit, policy를 기록.

### 문서

* 측정 결과를 `docs/analysis/linux-port-frontier.md`에 Task 635 절로 누적합니다.
* `docs/work-logs/20260908-635-x64-gs-override-selector-resolution.md`를 씁니다.

## English

### Scope

Add an opt-in segment-resolution trace to `ReResolveAotSegmentOverrides` and
measure the actual descriptor and policy of GS selector `0x0080` during a
`pumpit2a` run. Do not change segment-override emission or execution policy.

### Implementation steps

1. Add a diagnostic controlled by `REPIU_AOT_SEGMENT_RESOLUTION_TRACE` to
   `src/engine/aot/aot_runtime_dispatch.cpp`.
2. Only when resolutions change, print the shadow address, selector, base,
   limit, flags, and policy for ES, CS, SS, DS, FS, and GS to `stderr`.
3. Emit nothing when the variable is absent or equals `0`.
4. Run `pumpit2a` with the trace and settle whether the GS resolution follows
   branch A or branch B from the design.

### Prohibited

* Do not admit FS/GS in `LongModeSegmentOverrideEmittable`.
* Do not change segment-override ModRM forms or patch policy.
* Do not change the i386 path or default execution output.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. Run `./build/linux_x64/repiu_core_probe` and confirm
   `core_probe_failures=0`.
3. Run briefly without the environment variable and confirm the new trace is
   absent.
4. Run
   `REPIU_AOT_SEGMENT_RESOLUTION_TRACE=1 ./build/linux_x64/repiu pumpit2a`
   and record GS selector `0x0080`'s base, limit, and policy.

### Documentation

* Accumulate the measurement in a Task 635 section of
  `docs/analysis/linux-port-frontier.md`.
* Write `docs/work-logs/20260908-635-x64-gs-override-selector-resolution.md`.
