# Task 635 작업 로그: x64 GS override selector 해석 측정

설계: [20260908-635](../design/20260908-635-x64-gs-override-selector-resolution.md) ·
작업 지시: [20260908-635](../work-orders/20260908-635-x64-gs-override-selector-resolution.md) ·
분석: [linux-port-frontier 3.72](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`ReResolveAotSegmentOverrides`에 `REPIU_AOT_SEGMENT_RESOLUTION_TRACE` opt-in
진단을 추가했습니다. live table이 달라질 때 ES~GS 여섯 해석의 shadow address,
selector, base, limit, flags, policy를 `stderr`에 출력합니다. 환경 변수가 없거나
값이 `0`이면 출력하지 않습니다.

### 검증

Linux x64 빌드와 core probe가 통과했습니다.

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
[100%] Built target repiu_core_probe
[100%] Built target repiu

core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

환경 변수 없이 실행했을 때 새 trace는 0행이었으며 기존 fault를 재현했습니다.

```text
trace_lines=0
[repiu-fault] unhandled signal=0xb rip=0x10f06d0 eip=0x10f06d0
  access=0x9fdd ... eax=0x9fdd ... edx=0x80
```

추적 실행은 fault 직전에 다음 GS 해석을 기록했습니다.

```text
[repiu-aot-segment-resolution] segment=GS index=5 shadow=0x1F00000A
  selector=0x0080 base=0x095C7000 limit=0x00009FFF
  flags=0x00000000 policy=1
```

`policy=1`은 `kNativeFolded`입니다. 설계의 갈래 A가 확인됐습니다. 다만 설계의
후보 limit `0x914F`와 달리 실제 등록 limit는 `0x9FFF`였습니다. LINEXE code
image `0x9150`바이트를 arena layout이 `0xA000`으로 page-align하고 selector
등록이 `gate_code_size - 1`을 사용하기 때문입니다. 따라서 offset `0x9FDD`도
실제 descriptor 범위 안입니다.

### 판단과 다음 작업

이번 작업은 측정 전용이므로 GS 방출 범위나 실행 정책은 바꾸지 않았습니다.
다음 작업은 `LongModeSegmentOverrideEmittable`이 GS와 `mod=00`
base-register/no-displacement 형식을 받고, 기존 slot이 GS base를 disp32에 접도록
확장하는 것입니다. host GS는 계속 건드리지 않습니다.

## English

### Result

Added the opt-in `REPIU_AOT_SEGMENT_RESOLUTION_TRACE` diagnostic to
`ReResolveAotSegmentOverrides`. Whenever the live table changes, it prints the
shadow address, selector, base, limit, flags, and policy for all six segment
resolutions to `stderr`. An absent value or `0` emits nothing.

### Verification

The Linux x64 build and core probe passed.

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
[100%] Built target repiu_core_probe
[100%] Built target repiu

core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

A run without the environment variable produced zero new trace lines and
reproduced the existing fault.

```text
trace_lines=0
[repiu-fault] unhandled signal=0xb rip=0x10f06d0 eip=0x10f06d0
  access=0x9fdd ... eax=0x9fdd ... edx=0x80
```

The traced run recorded this GS resolution immediately before the fault.

```text
[repiu-aot-segment-resolution] segment=GS index=5 shadow=0x1F00000A
  selector=0x0080 base=0x095C7000 limit=0x00009FFF
  flags=0x00000000 policy=1
```

`policy=1` is `kNativeFolded`, confirming branch A from the design. The live
limit is `0x9FFF`, not the candidate `0x914F` in the design: the arena layout
page-aligns the `0x9150`-byte LINEXE code image to `0xA000`, and selector
registration uses `gate_code_size - 1`. Offset `0x9FDD` is therefore inside the
actual descriptor.

### Assessment and next task

This was a measurement-only task, so segment emission and execution policy are
unchanged. The next task should make `LongModeSegmentOverrideEmittable` admit GS
and the `mod=00` base-register/no-displacement form, letting the existing slot
fold the GS base into a disp32. Host GS remains untouched.
