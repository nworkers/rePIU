# 20260830-529 pumpipx3 FF 경계 귀속 작업 로그

## 결과 요약

Task 529의 목적이었던 AOT `FF` boundary 표본의 ModRM 그룹 분리는 완료되었습니다. 두 타이틀의
동일 조건 60초 실행에서 관측된 그룹은 모두 near indirect jump인 `FF /4`였습니다. `pumpipx3`는
late drop 직전부터 약 40초 표본 사이에 누적 `/4`가 `3628 -> 15811`로 급증했고, 같은 시점에
FPS가 약 38.1에서 3.9로 하락했습니다. `pumpit1`의 대응 변화는 `58 -> 90`뿐이었습니다.

이는 다음 주소별 귀속 분석을 진행할 충분한 상관관계이지만, 원인 확정은 아닙니다. 이번 작업에서는
최적화나 실행 경로 변경을 적용하지 않았습니다.

## 변경 사항

* `AotBoundaryOpcodeCensus`에 `ff_group_counts[8]`와 `ff_modrm_truncated_count`를 추가했습니다.
* 기존에 dispatcher가 캡처한 byte window만 사용해 effective `FF` 뒤의 ModRM reg field를 분류했습니다.
* live reporter에 누적 `[repiu-live-ff]` 출력을 추가했습니다.
* probe에 `FF /2`, `FF /4`, prefix, truncated 입력과 8칸 합계 검사를 추가했습니다.

## 검증

Windows probe:

```text
cmake --build build\win32_x86_debug --config Debug --target repiu_aot_probe --parallel 1 --verbose
.\build\win32_x86_debug\Debug\repiu_aot_probe.exe .\roms\pumpipx3\PIU\PIU.EXE
```

probe 결과의 관련 항목:

```text
aot_boundary_opcode_census_ff_groups=true
aot_boundary_opcode_census_ff_truncation=true
aot_boundary_opcode_census_all=true
```

Linux i386 Release 빌드:

```text
wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2
```

빌드는 `repiu` target까지 성공했습니다.

## 측정 조건

두 실행 모두 다음 정책을 사용했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

trace는 활성화하지 않았으며, 각 실행은 timeout으로 종료했습니다.

## 측정 결과

| 표본 | `pumpipx3` | `pumpit1` |
| --- | --- | --- |
| #3 | `/4=3628`, `cycles/frame=118270439`, late drop 직전 FPS 약 38.1 | `/4=58`, `cycles/frame=118027477`, FPS 약 28~33 |
| #4 | `/4=15811`, `cycles/frame=79325953`, 약 40초 FPS 3.9 | `/4=90`, `cycles/frame=107165901`, 약 40초 FPS 30.8~33.9 |
| #5 | `/4=15811`, `cycles/frame=945417753`, 약 50초 FPS 4.0 | `/4=90`, `cycles/frame=133918358`, 약 50초 FPS 22~26 |

두 타이틀 모두 #1~#5에서 `/2=0`, `truncated=0`이었습니다.

종료 요약:

* `pumpipx3`: `reason=timeout`, `frames=1454`, `span_ms=56370`
* `pumpit1`: `reason=timeout`, `frames=2017`, `span_ms=57547`

`pumpipx3`의 #3→#4 구간에는 `/4`가 `12183` 증가했고, frame-rate가 약 38.1 FPS에서 3.9 FPS로
떨어졌습니다. #4→#5에서는 `/4`가 더 증가하지 않고 저 FPS가 유지되었습니다. `AH2C`는
`pumpipx3`에서 #3~#5 모두 `434544`였습니다.

## 판단

### 확인됨

* 현재 boundary `FF` 표본의 실제 그룹은 두 타이틀 모두 `/4`입니다.
* `pumpipx3`에서 `/4` 누적 증가와 late drop이 같은 live interval에 관측되었습니다.
* `pumpit1`에서는 같은 크기의 증가와 고정 5 FPS drop이 관측되지 않았습니다.
* FF 계측은 기존 histogram partition과 probe 동작을 깨뜨리지 않았습니다.

### 추정

`pumpipx3`의 title-specific late drop은 `FF /4` near indirect-jump boundary work와 상관관계가
있습니다. 다만 `/4` counter는 실행 site, target, addressing mode, 실제 cycle을 구분하지 않으므로
단일 원인이라고 결론 내릴 수 없습니다.

### 미확정

다음 작업에서 `/4` 표본의 guest EIP/site, ModRM addressing mode, resolved target, 그리고 drop
전후의 window cycle을 연결해야 합니다. 이 귀속이 끝나기 전에는 inline-cache나 indirect-jump
경로 최적화를 적용하지 않습니다.

---

# 20260830-529 Work Log: pumpipx3 FF Boundary Attribution

## Summary

The Task 529 objective is complete: AOT `FF` boundary samples are now split by ModRM group. Under
identical 60-second conditions, every observed group in both titles was the near indirect jump
`FF /4`. In `pumpipx3`, cumulative `/4` rose from `3628` just before the late drop to `15811` in
the sample around 40 seconds, while FPS fell from about 38.1 to 3.9. The corresponding `pumpit1`
change was only `58 -> 90`.

This is strong enough to define the next site-attribution task, but it is not causal proof. No
optimization or execution-path change was made in this unit.

## Changes

* Added `ff_group_counts[8]` and `ff_modrm_truncated_count` to `AotBoundaryOpcodeCensus`.
* Classified the ModRM reg field following effective `FF` using only the byte window already captured
  by the dispatcher.
* Added cumulative `[repiu-live-ff]` output to the live reporter.
* Added probe checks for `FF /2`, `FF /4`, prefixes, truncated input, and the eight-group total.

## Verification

Windows probe:

```text
cmake --build build\win32_x86_debug --config Debug --target repiu_aot_probe --parallel 1 --verbose
.\build\win32_x86_debug\Debug\repiu_aot_probe.exe .\roms\pumpipx3\PIU\PIU.EXE
```

Relevant probe output:

```text
aot_boundary_opcode_census_ff_groups=true
aot_boundary_opcode_census_ff_truncation=true
aot_boundary_opcode_census_all=true
```

Linux i386 Release build:

```text
wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2
```

The `repiu` target completed successfully.

## Measurement conditions

Both runs used:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

Tracing was disabled and each run ended by timeout.

## Measurements

| sample | `pumpipx3` | `pumpit1` |
| --- | --- | --- |
| #3 | `/4=3628`, `cycles/frame=118270439`, about 38.1 FPS immediately before the drop | `/4=58`, `cycles/frame=118027477`, about 28–33 FPS |
| #4 | `/4=15811`, `cycles/frame=79325953`, 3.9 FPS around 40 s | `/4=90`, `cycles/frame=107165901`, about 30.8–33.9 FPS around 40 s |
| #5 | `/4=15811`, `cycles/frame=945417753`, 4.0 FPS around 50 s | `/4=90`, `cycles/frame=133918358`, about 22–26 FPS around 50 s |

Both titles reported `/2=0` and `truncated=0` in #1 through #5.

Shutdown summaries:

* `pumpipx3`: `reason=timeout`, `frames=1454`, `span_ms=56370`
* `pumpit1`: `reason=timeout`, `frames=2017`, `span_ms=57547`

In the `pumpipx3` #3-to-#4 interval, `/4` increased by `12183` and frame rate fell from about 38.1
FPS to 3.9 FPS. From #4 to #5, `/4` did not increase further and the low FPS remained. `AH2C`
remained `434544` at #3, #4, and #5 for `pumpipx3`.

## Assessment

### Confirmed

* The observed boundary `FF` samples are `/4` in both titles.
* `pumpipx3` shows a same-interval cumulative `/4` increase and late drop.
* `pumpit1` does not show a comparable increase or a fixed 5 FPS drop.
* The FF instrumentation preserves the existing histogram partition and probe behavior.

### Inferred

The `pumpipx3` title-specific late drop correlates with `FF /4` near indirect-jump boundary work.
However, the `/4` counter does not identify execution site, target, addressing mode, or cycle cost,
so it cannot be treated as a single proven cause.

### Unresolved

The next task must connect each `/4` sample to guest EIP/site, ModRM addressing mode, resolved target,
and window cycles before and after the drop. No inline-cache or indirect-jump optimization should be
applied before that attribution is complete.
