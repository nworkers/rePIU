# Task 665 작업 로그 — Linux x64 AOT incoming fixup 추적

## 한국어

### 작업 범위

- 설계: [20260913-665 Linux x64 AOT incoming fixup 추적](../design/20260913-665-linux-x64-aot-incoming-fixup-trace.md)
- 작업 지시: [20260913-665 Linux x64 AOT incoming fixup 추적](../work-orders/20260913-665-linux-x64-aot-incoming-fixup-trace.md)
- 누적 analysis: [Linux port frontier](../analysis/linux-port-frontier.md#3102-task-665--linux-x64-aot-incoming-fixup-trace)

### 구현

`TraceAotGuestMap`에 target-side incoming fixup 진단을 추가했습니다.
`fixup.guest_target == match.guest_address`인 metadata만
`[repiu-aot-map-incoming-fixup]` prefix로 출력하고, trace 1회당 최대 32개로
제한합니다. 기존 source-side fixup 출력과 AOT cache/fixup/guest state 동작은
변경하지 않았습니다.

### 검증

1. Linux x64 Debug 빌드:

   ```text
   [100%] Built target repiu
   [100%] Built target repiu_core_probe
   ```

2. Core probe:

   ```text
   core_probe_total=27
   core_probe_failures=0
   core_probe_all=true
   core_probe_skipped=2 stack_bridge guest_stack_switch
   ```

3. 정적 map trace:

   ```text
   REPIU_AOT_MAP_TRACE=0x0F0232 REPIU_AOT_MAP_CONTEXT=8
   ```

   결과에서 target `0x010F0232`로 들어오는 후보가 다음과 같이 확인되었습니다.

   ```text
   [repiu-aot-map-incoming-fixup] filter=0x010F0232 source=0x010EFF2A target=0x010F0232 kind=direct-jump patch=0x00001936 resolved=1
   ```

   context map은 `0x010EFF2A`의 `E9 03 03 00 00` direct jump와
   `0x010F0232`의 `07` (`POP ES`) entry를 함께 보여주었습니다.

4. 동적 비교:

   Task 664의 `0x010F0232` HLE re-entry trace는 HLE 전 ESP
   `0x0158C84C`, HLE/AOT resume 후 ESP `0x0158C860`을 계속 보여줍니다.
   incoming fixup 출력은 정적 metadata 후보일 뿐이며 source의 동적 실행을
   증명하지 않습니다. 일반 게임 실행은 여전히 `0x010F0237` zero-return과
   fail-closed `SIGTRAP`에서 중단됩니다.

### 결론

`0x010EFF2A → 0x010F0232` direct-jump 연결은 AOT metadata에서 확인되었지만,
4바이트 delta의 동적 원인과 정상 게임 실행은 아직 미확정입니다. 이번 변경은
진단 출력만 추가했으며 guest semantics를 변경하지 않았습니다.

## English

### Scope

- Design: [20260913-665 Linux x64 AOT incoming fixup trace](../design/20260913-665-linux-x64-aot-incoming-fixup-trace.md)
- Work order: [20260913-665 Linux x64 AOT incoming fixup trace](../work-orders/20260913-665-linux-x64-aot-incoming-fixup-trace.md)
- Cumulative analysis: [Linux port frontier](../analysis/linux-port-frontier.md#3102-task-665--linux-x64-aot-incoming-fixup-trace)

### Implementation

`TraceAotGuestMap` now reports target-side incoming fixups. It prints metadata
where `fixup.guest_target == match.guest_address` with the distinct
`[repiu-aot-map-incoming-fixup]` prefix, capped at 32 entries per trace. Existing
source-side output and AOT cache, fixup, and guest-state behavior are unchanged.

### Verification

1. Linux x64 Debug build passed for `repiu` and `repiu_core_probe`.
2. Core probe passed with `core_probe_total=27`, `core_probe_failures=0`,
   `core_probe_all=true`, and two expected skips:
   `stack_bridge guest_stack_switch`.
3. The static map trace
   `REPIU_AOT_MAP_TRACE=0x0F0232 REPIU_AOT_MAP_CONTEXT=8` reported:

   ```text
   [repiu-aot-map-incoming-fixup] filter=0x010F0232 source=0x010EFF2A target=0x010F0232 kind=direct-jump patch=0x00001936 resolved=1
   ```

   The context map also showed the `E9 03 03 00 00` direct jump at
   `0x010EFF2A` and the `07` (`POP ES`) entry at `0x010F0232`.

4. Dynamic comparison with Task 664 remains bounded at the same failure:
   guest ESP is `0x0158C84C` before HLE at `0x010F0232` and `0x0158C860`
   after HLE/AOT resume, followed by the `0x010F0237` zero-return and
   fail-closed `SIGTRAP`. The incoming-fixup line is static metadata, not proof
   that `0x010EFF2A` executed dynamically.

### Conclusion

The `0x010EFF2A → 0x010F0232` direct-jump connection is confirmed in AOT
metadata. The dynamic cause of the four-byte delta and normal game execution
remain unresolved. This task added diagnostics only and changed no guest
semantics.
