# 20260830-531 pumpipx3 FF /4 displacement·resolved target 귀속 작업 로그

설계: [20260830-531](../design/20260830-531-pumpipx3-ff4-target-attribution.md)
작업 지시서: [20260830-531](../work-orders/20260830-531-pumpipx3-ff4-target-attribution.md)

## 결과 요약

Task 530의 site·SIB 관측을 displacement, operand pointer, raw dword target까지 확장했습니다.
두 타이틀의 새 60초 표본에서 모든 `FF /4` target read가 성공했습니다. `pumpipx3`는
`0x010EF6DE`의 index 7 jump-table entry를 반복해서 읽었고, `pumpit1`은 같은 site에서
EAX index가 3에서 2로 바뀌었습니다.

이 결과는 두 타이틀의 `/4` 차이를 특정 jump-table 사용 패턴으로 좁히지만, target 실행
cycle이나 table-entry writer까지 연결하지 않았으므로 late drop의 원인 또는 최적화 근거로
판정하지 않습니다.

## 구현 변경

* `AotFfBoundarySite`와 live snapshot에 마지막 displacement, operand pointer, target,
  resolved/failure/change count를 추가했습니다.
* 별도 `aot_ff_boundary_target_attribution` 모듈에서 최대 15바이트의 legacy-32
  `FF /4` register 및 ModRM/SIB operand를 bounded decode합니다.
* 16-bit address-size, 지원하지 않는 명시적 segment, truncated instruction,
  unreadable pointer는 fail-closed unresolved로 집계합니다.
* 기존 `HandleAotReentry`의 `kOther` 지점에서만 관측하며, guest register·memory·EIP,
  AOT dispatch 결과를 변경하지 않습니다.
* `[repiu-live-ff-site]`에 target 상태를 추가하고, Windows probe에 register,
  SIB/disp32, target change, truncated, unsupported, unreadable 검사를 추가했습니다.

## 빌드 및 프로브 검증

Windows x86 Debug probe build와 `pumpipx3` probe 실행이 완료되었습니다. 기존 census 검사와
새 target 검사가 모두 다음과 같이 `true`였습니다.

* `aot_ff_boundary_target_attribution=true`
* `aot_boundary_opcode_census_all=true`

Linux i386 Release build도 다음 결과로 완료되었습니다.

* `[100%] Built target repiu`

빌드에는 기존 경고만 있었고 실패는 없었습니다.

## 측정 조건

두 타이틀을 동일한 Linux i386 Release 실행 파일로 trace-free 60초 측정했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

측정 로그는 저장소 외부 temporary 경로에 보관했습니다. `pumpipx3`는 정상적인 timeout
shutdown으로 exit 0이었고, `pumpit1`은 timeout shutdown 후 기존 probe-dump 경로에서 exit 1로
끝났습니다. 이는 target attribution 실패가 아니라 기존 종료 경로의 차이입니다.

## 관측 결과

| live 표본 | `pumpipx3` | `pumpit1` |
| --- | --- | --- |
| #3 | `/4=2849`, resolved `2849`, unresolved `0`; site `0x010EF6DE:2827`, `d=0x010EF65C`, `p=0x010EF678`, `t=0x010EF8E9`, `tc=21` | `/4=58`, resolved `58`, unresolved `0`; site `0x010F1DD7:58`, `d=0x010F1D8F`, `p=0x010F1D9B`, `t=0x010F1CFD`, `tc=8` |
| #4 | `/4=3626`, resolved `3626`, unresolved `0`; site `0x010EF6DE:3598`, 같은 `d/p/t`, `tc=27` | `/4=90`, resolved `90`, unresolved `0`; site `0x010F1DD7:90`, `p=0x010F1D97`, `t=0x010F1CF4`, `tc=13` |
| #5 | `/4=15808`, resolved `15808`, unresolved `0`; site `0x010EF6DE:15686`, 같은 `d/p/t`, `tc=121` | `/4=90`, resolved `90`, unresolved `0`; 같은 `p/t`, `tc=13` |

세 표본 구간에서 두 타이틀 모두 target truncated, unsupported, unreadable counter가
0이었습니다. `pumpipx3`의 shutdown은 `frames=1428`, `span_ms=56158`이었고,
`pumpit1`은 `frames=1975`, `span_ms=57970`이었습니다.

## 정적 상관관계

`pumpipx3` 원본 `PIU.EXE`의 site를 `repiu_aot_probe`로 대조한 결과:

```text
0x010EF6DE: jmp cs:[edx*4+0x010EF65C]
0x010EF678: table index 7 pointer
0x010EF8E9: mov ebx, esi
```

따라서 runtime `p=0x010EF678`은 displacement `0x010EF65C`에서 7번째 dword를 읽은
결과이며, raw target `0x010EF8E9`는 target table 안의 instruction입니다.

`pumpit1`의 materialized CHD image `build/runtime_mounts/pumpit1/PIU/PIU.EXE` 대조 결과:

```text
0x010F1DD7: jmp cs:[eax*4+0x010F1D8F]
0x010F1D9B: table index 3 pointer
0x010F1CFD: mov dword ptr [esp], 0x04
0x010F1D97: table index 2 pointer
0x010F1CF4: mov dword ptr [esp], 0x03
```

runtime #3의 `p=...9B`, `t=...CFD`는 index 3이고, #4/#5의 `p=...97`, `t=...CF4`는
index 2입니다. 정적 image는 CHD에서 기존 EXE analyzer가 materialize한 파일이며, 원본
실행 파일은 수정하지 않았습니다.

## 판정

**확인됨**

* 새 evaluator의 register 및 SIB/disp32 target read가 probe에서 기대값으로 동작합니다.
* 두 타이틀의 runtime `/4` 표본은 모두 resolved이며, failure breakdown은 0입니다.
* `pumpipx3` dominant site는 `0x010EF6DE`, operand는 `edx*4+0x010EF65C`, runtime
  index는 7, 관측 target은 `0x010EF8E9`입니다.
* `pumpit1` site는 `0x010F1DD7`, operand는 `eax*4+0x010F1D8F`이며, index가 3에서
  2로 변합니다.

**추정**

`pumpipx3`의 후반 `/4` 증가는 같은 index 7 entry의 반복 read 증가와 일치합니다.
`target_change_count` 증가는 마지막 snapshot의 target이 달라졌다는 뜻이 아니라, 측정 중간
table read 값이 바뀐 횟수를 누적한 것입니다. 따라서 target-table mutation이 함께 존재할
가능성을 보여 주지만, 성능 저하의 원인이라고 확정하지 않습니다.

**미확정 및 다음 축**

현재 관측은 per-read cycle과 jump-table entry writer를 연결하지 않습니다. 다음 분석은
`0x010EF65C` entry의 writer를 정적으로/동적으로 귀속하고, late drop 직전·직후 target
code의 window cycle을 비교해야 합니다. 원본 코드 수정이나 dispatch 최적화는 적용하지
않았습니다.

---

# 20260830-531 Pumpipx3 FF /4 Displacement and Resolved-Target Attribution Work Log

Design: [20260830-531](../design/20260830-531-pumpipx3-ff4-target-attribution.md)
Work order: [20260830-531](../work-orders/20260830-531-pumpipx3-ff4-target-attribution.md)

## Summary

Task 530's site and SIB observation now includes the displacement, operand pointer, and raw
dword target. Every `/4` target read in the new 60-second samples resolved. Pumpipx3 repeatedly
read jump-table index 7 at `0x010EF6DE`; pumpit1 changed its EAX index from 3 to 2 at its only
observed site.

This narrows the difference to title-specific jump-table behavior, but it does not associate
target execution cycles or the table-entry writer. It therefore does not establish the late-drop
cause or justify an optimization.

## Implementation changes

* Added last displacement, operand pointer, target, resolved/failure/change counts to
  `AotFfBoundarySite` and the live snapshot.
* Added a separate `aot_ff_boundary_target_attribution` module with a maximum 15-byte bounded
  decoder for legacy-32 `FF /4` register and ModRM/SIB operands.
* Address-size 16-bit forms, unsupported explicit segments, truncated instructions, and
  unreadable pointers fail closed as unresolved.
* Called observation only from the existing `kOther` point in `HandleAotReentry`; guest
  registers, memory, EIP, and AOT dispatch results remain unchanged.
* Added target state to `[repiu-live-ff-site]` and register, SIB/disp32, target-change,
  truncated, unsupported, and unreadable checks to the Windows probe.

## Build and probe verification

The Windows x86 Debug `repiu_aot_probe` build and run passed the existing census checks and the
new target checks, including:

* `aot_ff_boundary_target_attribution=true`
* `aot_boundary_opcode_census_all=true`

The Linux i386 Release build also completed with `[100%] Built target repiu`. Only pre-existing
warnings were emitted.

## Measurement conditions

Both titles used the same Linux i386 Release executable and trace-free 60-second settings:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

The logs remain outside the repository in temporary storage. Pumpipx3 exited 0 after its timeout
shutdown. Pumpit1 exited 1 after timeout shutdown and its existing probe-dump path; this was not a
target-attribution failure.

## Observations

| live sample | `pumpipx3` | `pumpit1` |
| --- | --- | --- |
| #3 | `/4=2849`, resolved `2849`, unresolved `0`; `0x010EF6DE:2827`, `d=0x010EF65C`, `p=0x010EF678`, `t=0x010EF8E9`, `tc=21` | `/4=58`, resolved `58`, unresolved `0`; `0x010F1DD7:58`, `d=0x010F1D8F`, `p=0x010F1D9B`, `t=0x010F1CFD`, `tc=8` |
| #4 | `/4=3626`, resolved `3626`, unresolved `0`; `0x010EF6DE:3598`, same `d/p/t`, `tc=27` | `/4=90`, resolved `90`, unresolved `0`; `0x010F1DD7:90`, `p=0x010F1D97`, `t=0x010F1CF4`, `tc=13` |
| #5 | `/4=15808`, resolved `15808`, unresolved `0`; `0x010EF6DE:15686`, same `d/p/t`, `tc=121` | `/4=90`, resolved `90`, unresolved `0`; same `p/t`, `tc=13` |

Both titles reported zero target-truncated, unsupported, and unreadable counters in these
samples. Shutdown values were `frames=1428`, `span_ms=56158` for pumpipx3 and `frames=1975`,
`span_ms=57970` for pumpit1.

## Static correlation

The pumpipx3 original image correlates as:

```text
0x010EF6DE: jmp cs:[edx*4+0x010EF65C]
0x010EF678: table index 7 pointer
0x010EF8E9: mov ebx, esi
```

The pumpit1 image materialized from the CHD at `build/runtime_mounts/pumpit1/PIU/PIU.EXE`
correlates as:

```text
0x010F1DD7: jmp cs:[eax*4+0x010F1D8F]
0x010F1D9B: table index 3 pointer
0x010F1CFD: mov dword ptr [esp], 0x04
0x010F1D97: table index 2 pointer
0x010F1CF4: mov dword ptr [esp], 0x03
```

At runtime, `p=...9B`, `t=...CFD` is index 3 at sample #3, while `p=...97`, `t=...CF4`
is index 2 at samples #4/#5. The static image was materialized by the existing EXE analyzer;
the original executable was not modified.

## Assessment

**Confirmed**

* The probe resolves register and SIB/disp32 target values as expected.
* Every runtime `/4` sample in both titles resolved, with zero failure-breakdown counts.
* Pumpipx3's dominant site is `0x010EF6DE`, with operand `edx*4+0x010EF65C`, runtime index 7,
  and observed target `0x010EF8E9`.
* Pumpit1's site is `0x010F1DD7`, with operand `eax*4+0x010F1D8F`; its index changes from 3
  to 2.

**Inferred**

The later pumpipx3 `/4` increase is consistent with repeated reads of the same index-7 entry.
`target_change_count` is cumulative across intermediate reads; it does not mean the last target
in the snapshot changed. It indicates possible target-table mutation, not a proven cause of the
performance drop.

**Unresolved and next axis**

The current observation does not connect per-read cycles to the jump-table entry writer. The next
analysis should attribute the writer of entry `0x010EF65C` statically/dynamically and compare
target-code window cycles immediately before and after the late drop. No original-code change or
dispatch optimization was applied.
