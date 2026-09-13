# Task 662 작업 로그: Linux x64 guest 진입 provenance 계측

## 한국어

### 작업 개요

Task 662 설계에 따라 Linux x64 fault dispatcher의 실제 guest 진입/종료
경계를 관찰하는 선택적 provenance trace를 구현하고, `0x010F0232`에 대한
bounded 실행을 재검증했습니다.

- 설계: [20260912-662-linux-x64-guest-entry-provenance.md](../design/20260912-662-linux-x64-guest-entry-provenance.md)
- 작업 지시: [20260912-662-linux-x64-guest-entry-provenance.md](../work-orders/20260912-662-linux-x64-guest-entry-provenance.md)
- 누적 frontier: [linux-port-frontier.md](../analysis/linux-port-frontier.md#399-task-662--linux-x64-guest-entry-provenance)

### 구현 내용

`REPIU_LINUX_X64_GUEST_ENTRY_TRACE=<guest-address>`를 추가했습니다.
`VehExitRecorder`가 fault EIP와 fault 종류, dispatcher 진입/종료 EIP·ESP를
snapshot하고, AOT cache 주소는 guest 주소로 reverse-map한 뒤 필터와 일치할
때만 최대 32개의 trace line을 출력합니다. 새 계측은 `__x86_64__` 경로에만
연결했으며 guest state, stack semantics, RET target, resolver policy는
변경하지 않았습니다.

### 검증 결과

Linux x64 Debug 대상 재빌드가 성공했습니다.

```text
cmake --build build/linux_x64_debug --config Debug --target repiu repiu_core_probe -j2
```

기존 `g_repiu_active_thread_context` 선언 관련 warning 1건 외의 새 빌드
오류는 없었습니다. `repiu_core_probe` 결과:

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

`REPIU_LINUX_X64_GUEST_ENTRY_TRACE=0x010F0232`를 포함한 bounded runtime에서
다음 경로를 확인했습니다.

```text
fault_kind=breakpoint fault_eip=0x200018E7
entry_eip=0x200018E7 entry_guest=0x010F0232
exit_eip=0x20001917 exit_guest=0x010F0237
entry_esp=0x0158C84C exit_esp=0x0158C860
pending=0 legacy=0 exit_site=step-trace-hle-resumed
```

정적 map의 entry start `0x200018C3`와 동적 HLE-boundary fixup 위치
`0x200018E7`가 다르다는 점도 함께 확인했습니다. 이후 zero-return frame은
기존처럼 `status=0x010F0237`, `guest_eip=0`을 기록했고 fail-closed
`SIGTRAP`으로 종료했습니다.

새 필터를 지정하지 않은 비교 실행에서는 새
`[repiu-x64-guest-entry]` line이 출력되지 않았고, 기존 zero-return frame과
fail-closed fault만 관찰되었습니다.

### 결론과 미확정 사항

`0x010F0232`가 cache breakpoint/HLE dispatcher 경계를 통해 실제 관찰되며,
처리 후 `0x010F0237 RET`로 진행한다는 것은 확인되었습니다. 다만 이
계측만으로 breakpoint를 만든 upstream guest instruction이나
`0x010F022C` guest INT3의 실행 여부를 증명할 수는 없습니다. Task 661에서
남은 4-byte delta의 원인도 아직 미확정입니다.

### 작업 상태

- 구현: 완료
- 빌드: 통과
- 코어 프로브: `27/27`, 실패 `0`
- bounded runtime: 대상 entry 관찰, 기존 fail-closed frontier 유지
- 문서: 설계·작업 지시·analysis·작업 로그 갱신

## English

### Summary

Following the Task 662 design, this task implemented an opt-in provenance trace
for the Linux x64 fault-dispatch entry/exit boundary and reran a bounded
capture for `0x010F0232`.

- Design: [20260912-662-linux-x64-guest-entry-provenance.md](../design/20260912-662-linux-x64-guest-entry-provenance.md)
- Work order: [20260912-662-linux-x64-guest-entry-provenance.md](../work-orders/20260912-662-linux-x64-guest-entry-provenance.md)
- Cumulative frontier: [linux-port-frontier.md](../analysis/linux-port-frontier.md#399-task-662--linux-x64-guest-entry-provenance)

### Implementation

Added `REPIU_LINUX_X64_GUEST_ENTRY_TRACE=<guest-address>`. `VehExitRecorder`
snapshots the fault EIP and kind, dispatcher entry/exit EIP, and entry/exit
ESP. AOT cache addresses are reverse-mapped to guest addresses before matching
the filter, and output is capped at 32 records. The new capture is connected
only on `__x86_64__`; guest state, stack semantics, RET targets, and resolver
policy are unchanged.

### Verification

The Linux x64 Debug targets rebuilt successfully:

```text
cmake --build build/linux_x64_debug --config Debug --target repiu repiu_core_probe -j2
```

There were no new build errors; the only warning was the existing
`g_repiu_active_thread_context` declaration warning. `repiu_core_probe`
reported:

```text
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

A bounded runtime with `REPIU_LINUX_X64_GUEST_ENTRY_TRACE=0x010F0232`
confirmed:

```text
fault_kind=breakpoint fault_eip=0x200018E7
entry_eip=0x200018E7 entry_guest=0x010F0232
exit_eip=0x20001917 exit_guest=0x010F0237
entry_esp=0x0158C84C exit_esp=0x0158C860
pending=0 legacy=0 exit_site=step-trace-hle-resumed
```

The static map entry start `0x200018C3` was also distinguished from the
dynamic HLE-boundary fixup at `0x200018E7`. The subsequent zero-return frame
remained unchanged with `status=0x010F0237` and `guest_eip=0`, followed by the
existing fail-closed `SIGTRAP`.

In the comparison run without the new filter, no new
`[repiu-x64-guest-entry]` line appeared; only the existing zero-return frame
and fail-closed fault were observed.

### Conclusion and unresolved items

The capture confirms that `0x010F0232` is observed through a cache
breakpoint/HLE dispatcher boundary and that handling advances to
`0x010F0237 RET`. It does not identify the upstream guest instruction that
produced the breakpoint or prove execution of the guest INT3 at `0x010F022C`.
The cause of Task 661's four-byte delta also remains unresolved.

### Status

- Implementation: complete
- Build: passed
- Core probe: `27/27`, zero failures
- Bounded runtime: target entry observed; existing fail-closed frontier preserved
- Documentation: design, work order, analysis, and work log updated
