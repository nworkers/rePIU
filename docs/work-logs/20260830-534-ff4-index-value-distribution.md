# 20260830-534 FF /4 index 값 분포 관측 작업 로그

## 작업 결과

Task 533의 마지막 index 관측을 고정 8-slot histogram으로 확장했습니다. Resolved SIB
index register/value pair를 site와 hotspot에 누적하고, live reporter에 observation sample,
slot 수, overflow sample, 각 slot의 register/value/count를 추가했습니다. 기존 pointer/target
change count와 failure 분류는 유지했습니다.

## 검증

다음 검증이 통과했습니다.

* Win32 x86 Debug `repiu_aot_probe` build
* Synthetic full probe의 `aot_boundary_opcode_census_all=true`
* Synthetic index histogram assertions
* Linux i386 Release `repiu` build

새 binary로 다음 Task 533 동일 조건을 사용했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

## 측정 결과

| title/site | live sample | index histogram | pointer/target change | overflow |
| --- | --- | --- | --- | --- |
| `pumpipx3` / `0x010EF6DE` | #1 | `EDX=0:11`, `EDX=7:2818`, total `ix=2829` | `pc=21`, `tc=21` | `is=2`, `io=0` |
| `pumpipx3` / `0x010EF6DE` | #5 | `EDX=0:14`, `EDX=7:3587`, total `ix=3601` | `pc=27`, `tc=27` | `is=2`, `io=0` |
| `pumpit1` / `0x010F1DD7` | #1 | `EAX=3:28`, `EAX=2:30`, total `ix=58` | `pc=8`, `tc=8` | `is=2`, `io=0` |
| `pumpit1` / `0x010F1DD7` | #5 | `EAX=3:41`, `EAX=2:49`, total `ix=90` | `pc=13`, `tc=13` | `is=2`, `io=0` |

두 dominant site의 target read는 모두 resolved였고, unresolved/truncated/unsupported/
unreadable count는 0이었습니다. Histogram slot count의 합은 각 site의 index observation
sample count와 일치했습니다.

`pumpipx3`는 `reason=timeout`, `answered=1`, `recovered=1`, `stopped=1`, `failure=0`,
`frames=1394`, `span_ms=55709`로 종료했습니다. `pumpit1`은 #5 live sample까지 유효한
FF 결과를 남겼지만 `reason=timeout`, `answered=1`, `recovered=0`, `stopped=0`, `failure=0`,
`frames=1509`, `span_ms=56296` 후 process exit 1로 종료했습니다. 이는 target attribution
실패가 아니라 timeout cleanup 미완료로 구분합니다.

## Static correlation

Pumpipx3 site의 정적 instruction과 table은 다음과 같습니다.

```text
0x010EF6DE  jmp cs:[edx*4+0x010EF65C]
table index 0 -> target 0x010EF6E6
table index 7 -> target 0x010EF8E9 (mov ebx, esi)
```

따라서 histogram에서 확인된 `EDX=0`은 index 7 pointer의 table dword가 바뀐 것이 아니라
index 0의 별도 jump-table entry가 선택된 증거입니다. `EDX=7`은 계속 대부분의 sample을
차지합니다. Pumpit1의 `EAX=3`과 `EAX=2`는 이전 Task 532에서 확인된 두 pointer/target과
대응합니다.

## 결론과 미해결 사항

이번 결과로 Task 531의 stronger same-pointer target-table mutation 해석은 해당 window에서
지지되지 않습니다. 두 title 모두 실제로 두 index 값을 사용하며, pointer change와 target
change는 계속 함께 발생했습니다.

Histogram은 분포만 기록하므로 index 전환 순서, pumpit1 EAX producer, guest writer, late drop
인과관계는 미해결입니다. 순수 target instruction cycle도 현재 FF-boundary VEH hook의
오염을 피하기 위해 측정하지 않았으며 별도 timing boundary 작업으로 남겼습니다.

---

# 20260830-534 Work Log: FF /4 Index-Value Distribution Observation

## Result

Task 533's last-index observation was extended with a fixed eight-slot histogram. Resolved SIB
index register/value pairs are accumulated per site and hotspot, and the live reporter now exposes
observation samples, slot count, overflow samples, and each slot's register/value/count. Existing
pointer/target change and failure classifications were preserved.

## Verification

The following checks passed:

* Win32 x86 Debug `repiu_aot_probe` build
* Synthetic full probe with `aot_boundary_opcode_census_all=true`
* Synthetic index histogram assertions
* Linux i386 Release `repiu` build

Both titles used Task 533's identical conditions:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

## Measurements

| title/site | live sample | index histogram | pointer/target changes | overflow |
| --- | --- | --- | --- | --- |
| `pumpipx3` / `0x010EF6DE` | #1 | `EDX=0:11`, `EDX=7:2818`, total `ix=2829` | `pc=21`, `tc=21` | `is=2`, `io=0` |
| `pumpipx3` / `0x010EF6DE` | #5 | `EDX=0:14`, `EDX=7:3587`, total `ix=3601` | `pc=27`, `tc=27` | `is=2`, `io=0` |
| `pumpit1` / `0x010F1DD7` | #1 | `EAX=3:28`, `EAX=2:30`, total `ix=58` | `pc=8`, `tc=8` | `is=2`, `io=0` |
| `pumpit1` / `0x010F1DD7` | #5 | `EAX=3:41`, `EAX=2:49`, total `ix=90` | `pc=13`, `tc=13` | `is=2`, `io=0` |

All target reads at both dominant sites resolved, with zero unresolved, truncated, unsupported,
or unreadable samples. Slot-count sums matched each site's index observation sample count.

Pumpipx3 ended with `reason=timeout`, `answered=1`, `recovered=1`, `stopped=1`, `failure=0`,
`frames=1394`, and `span_ms=55709`. Pumpit1 produced valid FF data through live sample #5 but
ended with `reason=timeout`, `answered=1`, `recovered=0`, `stopped=0`, `failure=0`, `frames=1509`,
and `span_ms=56296`, followed by process exit 1. This is classified as incomplete timeout cleanup,
not target-attribution failure.

## Static correlation

The pumpipx3 site and table are:

```text
0x010EF6DE  jmp cs:[edx*4+0x010EF65C]
table index 0 -> target 0x010EF6E6
table index 7 -> target 0x010EF8E9 (mov ebx, esi)
```

The observed `EDX=0` is therefore a second jump-table entry, not a dword mutation at the index-7
pointer. EDX=7 still dominates the sample population. Pumpit1's EAX=3 and EAX=2 match its earlier
resolved pointer/target observations.

## Conclusion and unresolved items

Task 531's stronger same-pointer target-table mutation interpretation is not supported in this
window. Both titles use two actual index values, and pointer changes continue to accompany target
changes.

The histogram records distribution rather than transition order, so transition sequence, the
pumpit1 EAX producer, guest writer, and late-drop causality remain unresolved. Pure target-instruction
cycles were also deferred because the current FF-boundary VEH hook would contaminate the timing;
they remain a separate timing-boundary task.
