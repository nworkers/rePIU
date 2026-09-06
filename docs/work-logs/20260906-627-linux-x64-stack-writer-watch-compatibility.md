# Task 627 작업 로그: Linux x64 stack writer watch compatibility

## 한국어

### 작업 결과

`REPIU_GUEST_WRITE_TRACE=0x0158CC44`가 활성화된 상태에서 segment HLE의
`PUSH ES`가 watched stack page에 쓰기할 때 기존 직접 `memcpy` 대신
`WriteGuestUInt32`를 사용하도록 수정했습니다. 이 경로는 보호 상태를
복원하고 HLE write event를 guest write trace ring에 남깁니다. trace가
비활성화된 일반 실행 경로는 변경하지 않았습니다.

### 검증

빌드:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

결과:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

스택 감시 재현:

```text
REPIU_GUEST_WRITE_TRACE=0x0158CC44 \
REPIU_LINUX_X64_STACK_TRACE=1 \
REPIU_LINUX_X64_RETURN_TRACE=1 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64_repiu/repiu pumpit2a
```

HLE store가 중단되지 않고 다음 event가 기록되었습니다.

```text
[repiu-guest-write-trace-tail] event=hle ... destination=0x0158CC44 size=4 bytes=24000000
```

동시에 final fault는 기존과 동일하게 `0x011A6440`에서
`0x37016BE9`를 access하면서 재현되었습니다. 이번 실행에서는 target
`0x011A643A`의 native exact writer event를 관측하지 못했으므로 최종
writer 식별 자체는 다음 작업으로 남겼습니다.

### 판단

이번 작업은 writer trace를 깨뜨리던 HLE 보호 메모리 간섭을 제거한
진단 호환성 수정으로 완료되었습니다. 그러나 이것은 return target이나
stack slot을 수정한 것이 아니며, 원인 해결을 의미하지 않습니다.

## English

### Result

When `REPIU_GUEST_WRITE_TRACE=0x0158CC44` is active, the segment-HLE
`PUSH ES` now uses `WriteGuestUInt32` instead of the old direct `memcpy` when
its destination is on the watched stack page. The helper restores the prior
protection and records an HLE write event in the guest write trace ring. The
normal path with tracing disabled is unchanged.

### Verification

Build:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

Result:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Stack-watch reproduction:

```text
REPIU_GUEST_WRITE_TRACE=0x0158CC44 \
REPIU_LINUX_X64_STACK_TRACE=1 \
REPIU_LINUX_X64_RETURN_TRACE=1 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64_repiu/repiu pumpit2a
```

The HLE store no longer interrupts the run and records:

```text
[repiu-guest-write-trace-tail] event=hle ... destination=0x0158CC44 size=4 bytes=24000000
```

The existing final fault is still reproduced at `0x011A6440` while accessing
`0x37016BE9`. No native exact-writer event for target `0x011A643A` was
observed in this run, so identifying the final writer remains the next task.

### Assessment

This task is complete as a diagnostic-compatibility fix: the HLE protected
memory store no longer breaks writer tracing. It does not repair the return
target or stack slot, and it does not resolve the root cause.
