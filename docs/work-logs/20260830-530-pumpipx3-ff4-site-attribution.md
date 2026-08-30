# 20260830-530 pumpipx3 FF /4 site·addressing mode 귀속 작업 로그

설계: [20260830-530](../design/20260830-530-pumpipx3-ff4-site-attribution.md)
작업 지시서: [20260830-530](../work-orders/20260830-530-pumpipx3-ff4-site-attribution.md)

## 결과 요약

Task 529에서 확인한 AOT `FF /4` 증가를 guest EIP/site와 ModRM addressing mode로
분리했습니다. 두 타이틀의 표본은 모두 SIB 형식이었고, `pumpipx3`의 증가를 주도한 site는
`0x010EF6DE`였습니다. `pumpit1`은 다른 site `0x010F1DD7` 하나만 관측되었습니다.
Displacement와 resolved target은 아직 수집하지 않았으므로 인과관계나 비용은 확정하지
않았습니다.

## 변경 사항

* 고정 32-slot `AotFfBoundaryAttribution`과 상위 8개 site ranking을 추가했습니다.
* 기존 최대 4바이트 boundary window에서 `FF /4`를 찾아 register, absolute, base, SIB,
  16-bit address-size mode로 분류합니다.
* site별 마지막 packed bytes, byte 변경 횟수, mode 변경 횟수와 overflow/truncation을
  누적합니다.
* live reporter에 `[repiu-live-ff-site]`를 추가했습니다. 표본마다 formatted logging은
  하지 않습니다.
* 원본 guest code, AOT cache, register/memory semantics, indirect target 계산은 변경하지
  않았습니다.

## 빌드 및 프로브 검증

Linux i386 Release:

```text
wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2
[100%] Built target repiu
```

Windows AOT probe:

```text
cmake --build build/win32_x86_debug --config Debug --target repiu_aot_probe --parallel 1
build/win32_x86_debug/Debug/repiu_aot_probe.exe roms/pumpipx3/PIU/PIU.EXE
```

관련 검사는 모두 `true`였습니다.

```text
aot_ff_boundary_attribution_modes=true
aot_ff_boundary_attribution_sites=true
aot_ff_boundary_attribution_overflow=true
aot_ff_boundary_attribution_truncation=true
aot_boundary_opcode_census_all=true
```

## 측정 조건

두 타이틀을 같은 Linux i386 Release 실행 파일로 trace-free 60초 측정했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

측정 종료 표본은 `pumpipx3`가 `frames=1401`, `span_ms=56157`, `pumpit1`이 `frames=1928`,
`span_ms=57610`이었습니다. 두 실행 모두 `reason=timeout`, `answered=1`로 종료되었습니다.

## 관측 결과

| live 표본 | `pumpipx3` | `pumpit1` |
| --- | --- | --- |
| #3 | `/4=2850`, `sib=2850`; `0x010EF6DE:2828`, `0x0102A963:11`, `0x010E785C:11`; top packed `0x9524FF2E` | `/4=58`, `sib=58`; `0x010F1DD7:58`; packed `0x8524FF2E` |
| #4 | `/4=2850`, `sib=2850`; `0x010EF6DE:2828` | `/4=91`, `sib=91`; `0x010F1DD7:91` |
| #5 | `/4=3628`, `sib=3628`; `0x010EF6DE:3600`, `0x0102A963:14`, `0x010E785C:14` | `/4=91`, `sib=91`; `0x010F1DD7:91` |

모든 표본에서 `truncated=0`, `overflow=0`, site `byte_change=0`, `mode_change=0`이었습니다.
`packed` 정수는 little-endian으로 출력되므로 `0x9524FF2E`는 captured bytes `2E FF 24 95`,
`0x8524FF2E`는 `2E FF 24 85`입니다. 두 값은 동일한 `CS:FF /4`, ModRM `0x24` SIB 형식이며
SIB index byte가 다릅니다.

## 판정

**확인됨**

* 두 타이틀 모두 관측된 `FF /4`가 SIB mode입니다.
* `pumpipx3`의 표본은 사실상 `0x010EF6DE` 한 site가 지배하며, #5에서 `2828 -> 3600`으로
  증가했습니다.
* `pumpit1`은 `0x010F1DD7` 하나만 관측되었고 `58 -> 91`로 증가했습니다.
* site table capacity와 ModRM capture는 이번 측정에서 부족하지 않았습니다.

**추정**

`pumpipx3` late drop과 같은 후반 구간에서 늘어난 `/4`는 `0x010EF6DE`의 반복 실행과
시간적으로 정렬됩니다. 그러나 현재 측정은 site와 captured SIB byte까지만 보여 주므로
이 site가 실제 성능 저하의 원인이라고 확정할 수 없습니다.

**미확정 및 다음 작업**

각 site 뒤의 displacement와 실행 시 계산된 indirect target을 관측하고, late-drop 전후의
site별 window cycle과 연결해야 합니다. 그때도 원본 guest 실행 경로는 유지하고, 관측 결과만
추가합니다.

---

# 20260830-530 pumpipx3 FF /4 Site and Addressing-Mode Attribution Work Log

Design: [20260830-530](../design/20260830-530-pumpipx3-ff4-site-attribution.md)
Work order: [20260830-530](../work-orders/20260830-530-pumpipx3-ff4-site-attribution.md)

## Summary

The AOT `FF /4` increase from Task 529 was split by guest EIP/site and ModRM addressing mode.
All samples in both titles used SIB addressing. The `pumpipx3` increase was dominated by
`0x010EF6DE`; `pumpit1` exposed a different single site, `0x010F1DD7`. Displacements and resolved
targets were not collected, so causality and cost remain unconfirmed.

## Changes

* Added a fixed 32-slot `AotFfBoundaryAttribution` and top-eight site ranking.
* Classified `FF /4` from the existing four-byte boundary window as register, absolute, base, SIB,
  or 16-bit address-size mode.
* Accumulated each site's last packed bytes, byte-change count, mode-change count, overflow, and
  truncation.
* Added `[repiu-live-ff-site]` to the live reporter without formatted logging per sample.
* Left original guest code, AOT cache, register/memory semantics, and indirect-target calculation
  unchanged.

## Build and probe verification

The Linux i386 Release build completed with `[100%] Built target repiu`. The Windows
`repiu_aot_probe` completed with all new and existing checks passing:

```text
aot_ff_boundary_attribution_modes=true
aot_ff_boundary_attribution_sites=true
aot_ff_boundary_attribution_overflow=true
aot_ff_boundary_attribution_truncation=true
aot_boundary_opcode_census_all=true
```

## Measurement conditions

Both titles ran with the same Linux i386 Release executable and trace-free 60-second settings:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

The runs ended with `pumpipx3` at `frames=1401`, `span_ms=56157`, and `pumpit1` at `frames=1928`,
`span_ms=57610`; both ended with `reason=timeout`, `answered=1`.

## Observations

| live sample | `pumpipx3` | `pumpit1` |
| --- | --- | --- |
| #3 | `/4=2850`, `sib=2850`; `0x010EF6DE:2828`, `0x0102A963:11`, `0x010E785C:11`; top packed `0x9524FF2E` | `/4=58`, `sib=58`; `0x010F1DD7:58`; packed `0x8524FF2E` |
| #4 | `/4=2850`, `sib=2850`; `0x010EF6DE:2828` | `/4=91`, `sib=91`; `0x010F1DD7:91` |
| #5 | `/4=3628`, `sib=3628`; `0x010EF6DE:3600`, `0x0102A963:14`, `0x010E785C:14` | `/4=91`, `sib=91`; `0x010F1DD7:91` |

All samples reported `truncated=0`, `overflow=0`, and `byte_change=0`, `mode_change=0` at each
site. The packed integer is printed little-endian: `0x9524FF2E` represents captured bytes
`2E FF 24 95`, and `0x8524FF2E` represents `2E FF 24 85`. Both are `CS:FF /4` with ModRM
`0x24` and SIB addressing, but their SIB index bytes differ.

## Assessment

**Confirmed**

* Every observed `FF /4` sample in both titles used SIB addressing.
* `pumpipx3` was dominated by `0x010EF6DE`, which grew from `2828` to `3600` at #5.
* `pumpit1` exposed only `0x010F1DD7`, growing from `58` to `91`.
* Site capacity and ModRM capture were sufficient for this measurement.

**Inferred**

The `/4` increase in the later `pumpipx3` interval is temporally aligned with repeated execution
of `0x010EF6DE`. The measurement only identifies the site and captured SIB bytes, so it does not
establish that this site causes the performance drop.

**Unresolved and next unit**

Observe the displacement after each site, resolve the runtime indirect target, and correlate it
with site-specific window cycles before and after the late drop. Preserve the original guest
execution path and add observation only.
