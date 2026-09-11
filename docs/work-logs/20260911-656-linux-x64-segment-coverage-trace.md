# 20260911-656 작업 로그: Linux x64 segment-override coverage slot trace

## 한국어

### 결과

Task 655의 Linux x64 frontier를 진단하기 위해 dynamic AOT translation의 opt-in
trace를 확장했습니다. `REPIU_AOT_FALLBACK_TRACE=1`과
`REPIU_AOT_DYNAMIC_CONTAINS=0x010F44E6`가 함께 활성화된 경우에만
`AotAddressMapEntry`의 cache/emitted 정보, bounded emitted bytes, 그리고
`AotSegmentOverrideSite` metadata를 출력합니다. validator, cache emission, guest
semantics, fallback policy는 변경하지 않았습니다.

### 검증

* Linux x64 Debug build: 성공 (`repiu_core_probe`, `repiu`)
* core probe: `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`
* 실제 `pumpit2a` trace: 기존 fail-closed `Trace/breakpoint trap` 재현
* `0x010F44E6`: cache `0x2102`, guest length `4`, emitted length `57`, long mode
* segment site: `segment=2`, guard `0x2111`, selector `0x2115`, displacement `0x2132`,
  original displacement `0`

실행 증거:

```text
[repiu-aot-dynamic] stage=image-entry guest=0x010F44E6 cache=0x00002102 guest_length=4 emitted_length=57 long_mode=1 bytes=9C415E458D7FFC4589376766813C25000000000000740B458B37458D7F0441569DCC458B37458D7F0441569D6667898700000000E900000000
[repiu-aot-dynamic] stage=image-segment-site guest=0x010F44E6 segment=2 slot=0x00002102 guard_address=0x00002111 guard_selector=0x00002115 displacement=0x00002132 dispatch=0x00000000 original_displacement=0 prologue_size=5 prologue=9C415E458D
[repiu-aot-coverage-failure] guest=0x010F44E6 kind=9 length=4 bytes=66 36 89 07 00 00
```

기본 환경 변수 없이 실행했을 때 새 `repiu-aot-dynamic` image trace 레코드는 출력되지
않았습니다. 단, 실제 게임은 기존 frontier에서 계속 fail-closed 되므로 기본 실행의
정상 완료를 의미하지는 않습니다.

### 판단과 다음 단계

실제 slot과 site는 생성되므로 문제는 emission 부재가 아니라 coverage validation
판정 경계로 좁혀졌습니다. 다음 작업에서 validator가 기대하는 byte/layout과 위
실제 값을 대조해야 합니다. 이번 작업에서는 `SS` store 실행 의미를 변경하지 않았으며,
게임 정상 실행도 아직 달성하지 못했습니다.

## English

### Result

The opt-in dynamic AOT trace was extended to diagnose the Linux x64 frontier from
Task 655. With `REPIU_AOT_FALLBACK_TRACE=1` and
`REPIU_AOT_DYNAMIC_CONTAINS=0x010F44E6`, it prints the matching
`AotAddressMapEntry` cache/emitted data, bounded emitted bytes, and
`AotSegmentOverrideSite` metadata. The validator, cache emission, guest semantics,
and fallback policy were unchanged.

### Verification

* Linux x64 Debug build: passed (`repiu_core_probe`, `repiu`)
* Core probe: `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`
* Real `pumpit2a` trace: reproduced the existing fail-closed `Trace/breakpoint trap`
* `0x010F44E6`: cache `0x2102`, guest length `4`, emitted length `57`, long mode
* Segment site: `segment=2`, guard `0x2111`, selector `0x2115`, displacement `0x2132`,
  original displacement `0`

Execution evidence:

```text
[repiu-aot-dynamic] stage=image-entry guest=0x010F44E6 cache=0x00002102 guest_length=4 emitted_length=57 long_mode=1 bytes=9C415E458D7FFC4589376766813C25000000000000740B458B37458D7F0441569DCC458B37458D7F0441569D6667898700000000E900000000
[repiu-aot-dynamic] stage=image-segment-site guest=0x010F44E6 segment=2 slot=0x00002102 guard_address=0x00002111 guard_selector=0x00002115 displacement=0x00002132 dispatch=0x00000000 original_displacement=0 prologue_size=5 prologue=9C415E458D
[repiu-aot-coverage-failure] guest=0x010F44E6 kind=9 length=4 bytes=66 36 89 07 00 00
```

With the diagnostic environment variables unset, no new `repiu-aot-dynamic` image
trace records were printed. The real game still reaches the existing frontier and
fails closed, so this does not mean that the default run completed normally.

### Decision and next step

The slot and site are present, so the issue is narrowed to the coverage-validation
boundary rather than missing emission. The next task must compare the validator's
expected byte/layout with the observed values. This task did not change `SS` store
semantics, and normal game execution has not yet been reached.
