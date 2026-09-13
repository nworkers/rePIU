# 작업 로그 20260913-664 — Linux x64 AOT/HLE 재진입 guest ESP 추적

## 한국어

### 작업 개요

Task 664 설계에 따라 기존 `REPIU_AOT_HLE_REENTRY_TRACE`에 HLE dispatcher
전후와 AOT resume 전후의 guest ESP 기록을 추가했습니다.

- 설계: [20260913-664-linux-x64-aot-hle-esp-trace.md](../design/20260913-664-linux-x64-aot-hle-esp-trace.md)
- 작업 지시: [20260913-664-linux-x64-aot-hle-esp-trace.md](../work-orders/20260913-664-linux-x64-aot-hle-esp-trace.md)
- 누적 frontier: [linux-port-frontier.md](../analysis/linux-port-frontier.md#3101-task-664--linux-x64-aot-hle-reentry-guest-esp-trace)

### 구현 내용

기존 AOT/HLE reentry trace helper에 `guest_esp` 출력을 추가하고,
`HandleSingleStepTrace`에서 다음 선택 trace 지점을 연결했습니다.

- `hle-before`: HLE dispatcher 호출 직전
- `hle-after`: HLE dispatcher 반환 직후
- `entry`: `TryResumeAotAfterHandledHle` 진입
- `resumed`: AOT cache resume 성공 직후
- `reentry-after`: resume 판단 반환 직후

필터와 bounded budget은 기존 `REPIU_AOT_HLE_REENTRY_TRACE`를 그대로
재사용합니다. 환경 변수가 없을 때 guest state나 control flow는 바뀌지
않습니다.

### 검증 결과

Linux x64 Debug 대상 빌드가 성공했습니다.

```text
cmake --build build/linux_x64_debug --config Debug --target repiu repiu_core_probe -j2
[100%] Built target repiu_core_probe
```

core probe 결과는 다음과 같습니다.

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

`REPIU_AOT_HLE_REENTRY_TRACE=0x010F0232` 실행에서 다음 ESP 전이를
확인했습니다.

```text
[repiu-hle-reentry] stage=hle-before handled=0x010F0232 current=0x010F0232 guest_esp=0x0158C84C
[repiu-hle-reentry] stage=hle-after handled=0x010F0232 current=0x010F0237 guest_esp=0x0158C860
[repiu-hle-reentry] stage=entry handled=0x010F0232 current=0x010F0237 guest_esp=0x0158C860
[repiu-hle-reentry] stage=resumed handled=0x010F0232 current=0x010F0237 guest_esp=0x0158C860
[repiu-hle-reentry] stage=reentry-after handled=0x010F0232 current=0x20001917 guest_esp=0x0158C860
```

동일 실행의 provenance trace는 `entry_esp=0x0158C84C`,
`exit_esp=0x0158C860`을 재확인했고, 이후 `0x010F0237 RET`가
`guest_eip=0`인 return frame을 만들어 기존 fail-closed `SIGTRAP`으로
끝났습니다.

`REPIU_AOT_HLE_REENTRY_TRACE=0x010EFEC4` 비교 실행에서는 다음을 확인했습니다.

```text
[repiu-hle-reentry] stage=hle-before handled=0x010EFEC4 current=0x010EFEC4 guest_esp=0x0158C84C
[repiu-segment-hle-watch] eip=0x010EFEC4 opcode=0x06 selector=0x0024 destination=0x0158C848 value=0x00000024 esp=0x0158C84C->0x0158C848 next_eip=0x010EFEC5 size=1
[repiu-hle-reentry] stage=hle-after handled=0x010EFEC4 current=0x010EFEC5 guest_esp=0x0158C848
[repiu-hle-reentry] stage=resumed handled=0x010EFEC4 current=0x010EFEC5 guest_esp=0x0158C848
```

### 결론과 다음 frontier

`0x010EFEC4 PUSH ES` HLE의 stack effect는 기존과 동일하게 `-4`입니다.
`0x010F0232` 경로는 HLE dispatcher 내부에서 `0x0158C84C`에서
`0x0158C860`으로 진행한 뒤, AOT resume과 reentry return에서 ESP를
그대로 유지했습니다. `TryResumeAotAfterHandledHle`는 이 경로에서 EIP와
EFlags/pending 상태만 갱신했으며 guest ESP를 증가시키지 않았습니다.

따라서 Task 661의 4바이트 delta는 AOT resume 자체의 ESP 보정으로
설명되지 않습니다. `0x010F0232`에 들어올 때 이미 ESP가 `0x0158C84C`인
상태가 만들어졌다는 점은 확인되었지만, `0x010EFEC4` 이후 어느 CALL,
return resolver 또는 다른 경계가 그 상태를 만들었는지는 아직 미확정입니다.
게임은 동일한 zero-return fail-closed frontier에서 중단됩니다.

### 작업 상태

- 구현: 완료
- 빌드: 통과
- 코어 프로브: `27/27`, 실패 `0`
- `PUSH ES` ESP delta: 확인됨
- `0x010F0232` HLE/AOT ESP 보존: 확인됨
- 4-byte ESP delta의 upstream 원인: 미확정
- 정상 게임 실행: 미확정

## English

### Summary

Following the Task 664 design, the existing
`REPIU_AOT_HLE_REENTRY_TRACE` now records guest ESP before and after HLE
dispatch and before and after AOT resume.

- Design: [20260913-664-linux-x64-aot-hle-esp-trace.md](../design/20260913-664-linux-x64-aot-hle-esp-trace.md)
- Work order: [20260913-664-linux-x64-aot-hle-esp-trace.md](../work-orders/20260913-664-linux-x64-aot-hle-esp-trace.md)
- Cumulative frontier: [linux-port-frontier.md](../analysis/linux-port-frontier.md#3101-task-664--linux-x64-aot-hle-reentry-guest-esp-trace)

### Implementation

The existing AOT/HLE re-entry trace helper now prints `guest_esp`, and
`HandleSingleStepTrace` emits the following opt-in stages:

- `hle-before`: immediately before HLE dispatch;
- `hle-after`: immediately after HLE dispatch;
- `entry`: at entry to `TryResumeAotAfterHandledHle`;
- `resumed`: after a successful AOT-cache resume;
- `reentry-after`: after the resume decision returns.

The existing `REPIU_AOT_HLE_REENTRY_TRACE` filter and bounded budget are reused.
Without the environment variable, guest state and control flow are unchanged.

### Verification

The Linux x64 Debug targets built successfully:

```text
cmake --build build/linux_x64_debug --config Debug --target repiu repiu_core_probe -j2
[100%] Built target repiu_core_probe
```

The core probe reported:

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

With `REPIU_AOT_HLE_REENTRY_TRACE=0x010F0232`, the ESP transitions were:

```text
[repiu-hle-reentry] stage=hle-before handled=0x010F0232 current=0x010F0232 guest_esp=0x0158C84C
[repiu-hle-reentry] stage=hle-after handled=0x010F0232 current=0x010F0237 guest_esp=0x0158C860
[repiu-hle-reentry] stage=entry handled=0x010F0232 current=0x010F0237 guest_esp=0x0158C860
[repiu-hle-reentry] stage=resumed handled=0x010F0232 current=0x010F0237 guest_esp=0x0158C860
[repiu-hle-reentry] stage=reentry-after handled=0x010F0232 current=0x20001917 guest_esp=0x0158C860
```

The same provenance run confirmed `entry_esp=0x0158C84C` and
`exit_esp=0x0158C860`. It then reached the existing zero-return frame at
`0x010F0237 RET` and stopped at the fail-closed `SIGTRAP`.

A comparison run with `REPIU_AOT_HLE_REENTRY_TRACE=0x010EFEC4` recorded:

```text
[repiu-hle-reentry] stage=hle-before handled=0x010EFEC4 current=0x010EFEC4 guest_esp=0x0158C84C
[repiu-segment-hle-watch] eip=0x010EFEC4 opcode=0x06 selector=0x0024 destination=0x0158C848 value=0x00000024 esp=0x0158C84C->0x0158C848 next_eip=0x010EFEC5 size=1
[repiu-hle-reentry] stage=hle-after handled=0x010EFEC4 current=0x010EFEC5 guest_esp=0x0158C848
[repiu-hle-reentry] stage=resumed handled=0x010EFEC4 current=0x010EFEC5 guest_esp=0x0158C848
```

### Conclusion and next frontier

The `0x010EFEC4 PUSH ES` HLE still has the expected `-4` stack effect. The
`0x010F0232` path advances from `0x0158C84C` to `0x0158C860` inside HLE
dispatch, then preserves `0x0158C860` through AOT resume and the re-entry
return. `TryResumeAotAfterHandledHle` changes EIP and EFlags/pending state on
this path but does not increase guest ESP.

Task 661's four-byte delta therefore is not explained by an ESP adjustment in
AOT resume. It is confirmed that the `0x010F0232` path already enters with
ESP `0x0158C84C`; which CALL, return resolver, or other boundary after
`0x010EFEC4` creates that state remains unresolved. The game still stops at
the same zero-return fail-closed frontier.

### Status

- Implementation: complete
- Build: passed
- Core probe: `27/27`, zero failures
- `PUSH ES` ESP delta: confirmed
- `0x010F0232` HLE/AOT ESP preservation: confirmed
- Upstream cause of the four-byte ESP delta: unresolved
- Normal game execution: unresolved
