# 작업 로그 20260905-593 — Linux x64 raw single-step byte 귀속

설계: [20260905-593](../design/20260905-593-linux-x64-raw-step-byte-attribution.md)  
작업 지시: [20260905-593](../work-orders/20260905-593-linux-x64-raw-step-byte-attribution.md)

## 결과

watched guest single-step 계측에 optional `le_bytes`를 추가했습니다.
`RecordSingleStepDiagnostics`는 watched guest EIP일 때만 `IsGuestRangeReadable`로
8바이트 전체 범위를 확인한 뒤 원본 guest 메모리를 little-endian `uint64_t`로 읽습니다.
읽을 수 없으면 `le_bytes`를 생략합니다. cache fault event의 `at` 의미와 다른 watch
event의 출력·상태·제어 흐름은 변경하지 않았습니다.

## 검증

* WSL Linux x64 `repiu` 빌드 성공.
* WSL Linux x64 `repiu_core_probe` 재링크 및 실행 성공:
  `core_probe_failures=0`, `core_probe_all=true`.
* `REPIU_GUEST_WATCH=0x010F010C REPIU_LINUX_X64_RETURN_TRACE=1 timeout 5s`
  watched `pumpit2a` 실행 결과:

```text
[repiu-watch] event=fault guest=0x010F010C n=1 at=0x200050EF ...
[repiu-watch] event=step guest=0x010F010C n=1 at=0x010F010C le_bytes=0x00000118820F31CD ...
[repiu-fault] unhandled signal=0xb rip=0x10f010c eip=0x10f010c access=0x0
```

`fault`와 `step` event가 동일한 watched guest address를 가리키고, `step` event가
`le_bytes`를 출력하는 것을 확인했습니다. 값은 메모리 바이트 `CD 31 0F 82 18 01 00 00`에
해당합니다. 기존 raw `SIGSEGV` frontier `0x010F010C`도 유지되었습니다. 프로그램은
미처리 SIGSEGV 후 core dump로 종료되어 외부 명령의 종료 코드는 `1`이었습니다.

---

# Work log 20260905-593 — Linux x64 raw single-step byte attribution

Design: [20260905-593](../design/20260905-593-linux-x64-raw-step-byte-attribution.md)  
Work order: [20260905-593](../work-orders/20260905-593-linux-x64-raw-step-byte-attribution.md)

## Result

Watched guest single-step diagnostics now include an optional `le_bytes` field.
When the watched guest EIP matches, `RecordSingleStepDiagnostics` first checks
that the full eight-byte range is accepted by `IsGuestRangeReadable`, then reads
the guest bytes into a little-endian `uint64_t`. The field is omitted when the
range cannot be read. Cache-fault `at` semantics and all other watch output,
state, and control flow remain unchanged.

## Verification

* Linux x64 `repiu` built successfully under WSL.
* Linux x64 `repiu_core_probe` was relinked and passed:
  `core_probe_failures=0`, `core_probe_all=true`.
* A watched `pumpit2a` run with
  `REPIU_GUEST_WATCH=0x010F010C REPIU_LINUX_X64_RETURN_TRACE=1 timeout 5s`
  produced:

```text
[repiu-watch] event=fault guest=0x010F010C n=1 at=0x200050EF ...
[repiu-watch] event=step guest=0x010F010C n=1 at=0x010F010C le_bytes=0x00000118820F31CD ...
[repiu-fault] unhandled signal=0xb rip=0x10f010c eip=0x10f010c access=0x0
```

The fault and step events identify the same watched guest address, and the
step event prints `le_bytes`. The value corresponds to memory bytes
`CD 31 0F 82 18 01 00 00`. The existing raw `SIGSEGV` frontier at
`0x010F010C` remains. The process terminated with exit code `1` after the
unhandled SIGSEGV generated a core dump.
