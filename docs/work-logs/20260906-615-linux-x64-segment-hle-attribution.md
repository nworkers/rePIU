# 20260906-615 Linux x64 segment HLE attribution 작업 로그

## 한국어

### 결과

`0x010F4A96`의 `PUSH ES`가 later fault-chain에서 거부된 것이 아니라,
`DispatchGuestFault` 이후 single-step 재진입에서 공유되는
`DispatchGuestHleHandlers` 경로로 처리된다는 사실을 확인했습니다.
진단은 실행 제어 흐름을 바꾸지 않으며, `REPIU_SEGMENT_HLE_TRACE=1`일 때만
최대 16회의 segment push 관찰을 출력합니다.

대표 출력은 다음과 같습니다.

```text
[repiu-watch] event=fault guest=0x010F4A96 n=1 at=0x2004F95A ...
[repiu-watch] event=step guest=0x010F4A96 n=1 at=0x010F4A96 ...
[repiu-segment-hle] stage=shared-dispatch n=1 eip=0x010F4A96 opcode=06 second=89 offset=0 enabled=1 cache_active=1 call_state=0 esp=0x0158CC7C es=0x0024
[repiu-segment-hle] stage=shared-handler n=1 handled=1 eip_after=0x010F4A97 esp_after=0x0158CC78
```

즉 handler는 selector를 guest stack에 기록하고 EIP/ESP를 각각 1바이트와
4바이트 전진시킨 뒤 resume을 반환했습니다. 이후 다른 segment push도
`handled=1`로 처리됐으며, trace 상한 16회가 적용됐습니다.

### 검증

* Linux x64 Debug `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Linux x64 `repiu` 빌드 성공.
* `pumpit2a`에서 `0x010F4A96` shared dispatch 및 handler 성공 확인.
* 실행은 이후 `0x401C94F5`의 `no-host-frame-to-unwind` / `UD2` fail-closed
  경계에서 종료했습니다.

### 결론

`PUSH ES` HLE은 현재 Linux x64 frontier의 원인이 아닙니다. 다음 작업은
x64 return thunk가 `guest_source=0`을 받는 시점의 frame, guest ESP, guest
stack return word를 직접 기록하여 반환 source가 실제로 0인지 또는 thunk
진입 전에 덮어써졌는지 구분해야 합니다.

## English

### Result

The `PUSH ES` at `0x010F4A96` is not rejected by the later fault-chain. It is
handled by the shared `DispatchGuestHleHandlers` path used during single-step
reentry after `DispatchGuestFault`. The diagnostic does not change control
flow and prints at most 16 segment-push observations when
`REPIU_SEGMENT_HLE_TRACE=1` is set.

Representative output:

```text
[repiu-watch] event=fault guest=0x010F4A96 n=1 at=0x2004F95A ...
[repiu-watch] event=step guest=0x010F4A96 n=1 at=0x010F4A96 ...
[repiu-segment-hle] stage=shared-dispatch n=1 eip=0x010F4A96 opcode=06 second=89 offset=0 enabled=1 cache_active=1 call_state=0 esp=0x0158CC7C es=0x0024
[repiu-segment-hle] stage=shared-handler n=1 handled=1 eip_after=0x010F4A97 esp_after=0x0158CC78
```

The handler writes the selector to guest stack memory, advances EIP by one
byte and ESP by four bytes, and returns resume. Other segment pushes in the
same run also returned `handled=1`, and the 16-event bound was applied.

### Verification

* Linux x64 Debug `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Linux x64 `repiu` build succeeded.
* `pumpit2a` confirmed shared dispatch and handler success for
  `0x010F4A96`.
* Execution then stopped at the `0x401C94F5`
  `no-host-frame-to-unwind` / `UD2` fail-closed boundary.

### Conclusion

`PUSH ES` HLE is not the cause of the current Linux x64 frontier. The next
task must record the return thunk frame, guest ESP, and guest-stack return word
when the x64 resolver receives `guest_source=0`, distinguishing a real zero
return source from corruption before thunk entry.
