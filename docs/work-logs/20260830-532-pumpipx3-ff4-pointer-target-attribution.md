# 20260830-532 pumpipx3 FF /4 pointer·target 변경 분리 계측 작업 로그

## 결과 요약

Task 531의 target 변화가 jump-table entry 자체의 변경인지 index/register 변화인지 구분하기
위해 resolved pointer 변경 count와 target 변화 분류 count를 추가했습니다. 새 60초 측정에서
두 타이틀의 주요 FF /4 site는 모두 pointer가 유효했고, 관측된 target 변화는 모두 pointer
변경을 동반했습니다. 동일 pointer에서 target만 변한 표본은 0회였습니다.

따라서 Task 531에서 `pumpipx3`에 대해 제시했던 “target-table mutation” 해석은 이번 측정으로
지지되지 않으며, 현재 증거는 index/register 경로 또는 그에 준하는 operand pointer 변화 쪽을
가리킵니다. 다만 pointer 변경의 실제 writer와 late drop의 인과관계는 아직 확인하지 않았습니다.

## 변경 내용

* `AotFfBoundarySite`와 hotspot에 `pointer_change_count`, pointer validity,
  `target_change_with_same_pointer_count`, `target_change_with_pointer_change_count`를 추가했습니다.
* 기존 `target_change_count`와 resolved/unresolved 의미는 유지했습니다.
* register-form은 pointer 분류에서 제외하고, unresolved sample은 마지막 resolved 상태를
  지우지 않도록 했습니다.
* live site line에 `pv`, `pc`, `dc`, `tc`, `spc`, `ppc`를 추가했습니다.
* synthetic SIB probe에서 같은 pointer의 dword 변경과 다른 index 선택을 별도로 검증했습니다.

## 검증

### Probe/build

* Win32 x86 Debug `repiu_aot_probe`: 성공
* Probe 결과: `aot_ff_boundary_target_attribution=true`,
  `aot_boundary_opcode_census_all=true`, exit 0
* Linux i386 Release build: 성공, `repiu` 링크 완료
* 기존 코드 페이지 및 매크로 관련 경고는 있었으나 새 변경으로 인한 오류는 없었습니다.

### 실제 측정 조건

두 실행 모두 다음 환경을 사용했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

실행은 저장소 루트에서 각각 `./build/linux_i386/repiu pumpipx3`와
`./build/linux_i386/repiu pumpit1`을 사용했으며 stdout은 버리고 stderr를 임시 로그로
수집했습니다. 두 실행 모두 timeout shutdown으로 exit 1이 되었습니다.

## 관측 결과

`pc`는 pointer 변경 count, `tc`는 전체 target 변경 count, `spc`는 동일 pointer target 변경,
`ppc`는 pointer 변경 동반 target 변경입니다. 두 site의 `pv=1`, `dc=0`, target read 실패는
0이었습니다.

| snapshot | `pumpipx3` dominant `0x010EF6DE` | `pumpit1` `0x010F1DD7` |
| --- | --- | --- |
| #1 | `/4=2849`, site `2827`, `p=0x010EF678`, `t=0x010EF8E9`, `pc=21`, `tc=21`, `spc=0`, `ppc=21` | `/4=58`, site `58`, `p=0x010F1D9B`, `t=0x010F1CFD`, `pc=8`, `tc=8`, `spc=0`, `ppc=8` |
| #3 | `/4=3626`, site `3598`, same `p/t`, `pc=27`, `tc=27`, `spc=0`, `ppc=27` | `/4=58`, site `58`, same `p/t`, `pc=8`, `tc=8`, `spc=0`, `ppc=8` |
| #4/#5 | `/4=15808`, site `15686`, same `p/t`, `pc=121`, `tc=121`, `spc=0`, `ppc=121` | `/4=90`, site `90`, `p=0x010F1D97`, `t=0x010F1CF4`, `pc=13`, `tc=13`, `spc=0`, `ppc=13` |

전체 live line에서 `resolved=sample_count`, `unresolved=0`, `target_truncated=0`,
`target_unsupported=0`, `target_unreadable=0`이 유지되었습니다.

shutdown은 `pumpipx3`가 `frames=1482`, `span_ms=56576`, `pumpit1`이 `frames=1827`,
`span_ms=57075`였습니다. 이 값은 timeout 경계에서의 전체 실행량이며 FF /4 target 자체의
cycle 비용으로 해석하지 않았습니다.

## 판정

### 확인됨

* `pumpipx3` dominant site의 Task 531 target 변화 `tc=121`은 이번 실행에서 `pc=121` 및
  `ppc=121`과 일치했고 `spc=0`이었습니다.
* `pumpit1`도 site target 변화 `tc=13`이 `pc=13`, `ppc=13`과 일치했고 `spc=0`이었습니다.
* 관측된 두 site에서는 pointer가 고정된 상태에서 table dword만 변경된 증거가 없습니다.

### 추정

* 두 타이틀의 FF /4 target 변화는 현재 관측 범위에서 jump-table entry mutation보다는
  register/index 변화에 따른 다른 operand pointer 선택으로 설명하는 편이 타당합니다.
* Task 531 정적 대조에서 확인한 `pumpipx3`의 `edx` 기반 index 7 site와 `pumpit1`의
  EAX index 3→2 변화는 이 runtime 분류와 일관됩니다.

### 미확정

* pointer를 바꾼 guest writer 또는 register producer
* pointer/target 변경이 late drop을 일으키는지 여부
* resolved target instruction의 per-read cycle 비용

다음 작업은 writer/register producer와 drop 전후의 target 실행 비용을 관측하는 방향으로
이어가며, 원본 guest code와 dispatch 의미는 계속 변경하지 않습니다.

---

# 20260830-532 Work Log: Separating FF /4 Pointer and Target Changes in Pumpipx3

## Summary

This unit added resolved pointer-change counts and target-change classification to distinguish a
jump-table entry mutation from an index/register change. In the new 60-second runs, the dominant
FF /4 site in each title had a valid pointer, and every observed target change was accompanied by
a pointer change. Same-pointer target changes were zero.

Therefore Task 531's `target-table mutation` interpretation for `pumpipx3` is not supported by this
measurement. The current evidence favors an index/register path, or an equivalent operand-pointer
change. The actual writer and causality of the late drop remain unresolved.

## Changes

* Added `pointer_change_count`, pointer validity, `target_change_with_same_pointer_count`, and
  `target_change_with_pointer_change_count` to the site and hotspot state.
* Preserved existing total target-change and resolved/unresolved meanings.
* Excluded register-form targets from pointer classification and preserved the last resolved state
  across unresolved samples.
* Added `pv`, `pc`, `dc`, `tc`, `spc`, and `ppc` to the live site line.
* Extended the synthetic SIB probe to verify same-pointer dword mutation and alternate-index
  selection independently.

## Verification

* Win32 x86 Debug `repiu_aot_probe`: passed
* Probe output: `aot_ff_boundary_target_attribution=true`,
  `aot_boundary_opcode_census_all=true`, exit 0
* Linux i386 Release build: passed and linked `repiu`
* Existing code-page and macro-redefinition warnings remained; no new build errors occurred.

Both title runs used:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

Each run used `./build/linux_i386/repiu pumpipx3` or `pumpit1` from the repository root, with
stdout discarded and stderr captured outside the repository. Both ended with timeout shutdown and
exit code 1.

## Observations

Here `pc` is pointer-change count, `tc` is total target-change count, `spc` is same-pointer target
change count, and `ppc` is pointer-accompanied target-change count. Both sites had `pv=1`, `dc=0`,
and zero target-read failures.

| snapshot | `pumpipx3` dominant `0x010EF6DE` | `pumpit1` `0x010F1DD7` |
| --- | --- | --- |
| #1 | `/4=2849`, site `2827`, `p=0x010EF678`, `t=0x010EF8E9`, `pc=21`, `tc=21`, `spc=0`, `ppc=21` | `/4=58`, site `58`, `p=0x010F1D9B`, `t=0x010F1CFD`, `pc=8`, `tc=8`, `spc=0`, `ppc=8` |
| #3 | `/4=3626`, site `3598`, same `p/t`, `pc=27`, `tc=27`, `spc=0`, `ppc=27` | `/4=58`, site `58`, same `p/t`, `pc=8`, `tc=8`, `spc=0`, `ppc=8` |
| #4/#5 | `/4=15808`, site `15686`, same `p/t`, `pc=121`, `tc=121`, `spc=0`, `ppc=121` | `/4=90`, site `90`, `p=0x010F1D97`, `t=0x010F1CF4`, `pc=13`, `tc=13`, `spc=0`, `ppc=13` |

Across the live lines, `resolved=sample_count`, `unresolved=0`, `target_truncated=0`,
`target_unsupported=0`, and `target_unreadable=0` remained true.

Shutdown was `frames=1482`, `span_ms=56576` for `pumpipx3` and `frames=1827`, `span_ms=57075`
for `pumpit1`. These are whole-run amounts at the timeout boundary and are not interpreted as
per-target FF /4 cycle costs.

## Assessment

### Confirmed

* For `pumpipx3`, target-change `tc=121` matched `pc=121` and `ppc=121`, with `spc=0`.
* For `pumpit1`, target-change `tc=13` matched `pc=13` and `ppc=13`, with `spc=0`.
* Neither site showed evidence of a table dword changing while its pointer stayed fixed.

### Inferred

* Within this observation window, FF /4 target changes are better explained by selecting a
  different operand pointer through register/index changes than by jump-table entry mutation.
* Task 531's static correlation—pumpipx3's EDX-based index-7 site and pumpit1's EAX index 3→2
  change—is consistent with this runtime classification.

### Unresolved

* The guest writer or register producer responsible for pointer changes
* Whether pointer/target changes cause the late drop
* Per-read cycle cost of the resolved target instruction

The next axis is writer/register-producer and before/after target-cost observation, while keeping
the original guest code and dispatch semantics unchanged.
