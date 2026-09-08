# Task 628 작업 로그: Linux x64 return stack writer tail

설계: [20260907-628](../design/20260907-628-linux-x64-return-stack-tail.md) ·
작업 지시: [20260907-628](../work-orders/20260907-628-linux-x64-return-stack-tail.md) ·
분석: [linux-port-frontier 3.65](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`REPIU_LINUX_X64_RETURN_STACK_TAIL=<count>`를 추가했습니다.
`REPIU_LINUX_X64_RETURN_REG_TRACE`가 고른 return target에서, x64 dispatch
frame의 stack write ring 최근 `count`개를 기록 순서대로 출력합니다. 값은
ring capacity로 상한을 두고, 환경 변수가 없으면 기존 실행 경로와 출력이
그대로입니다. resolver 정책과 guest stack semantics는 바꾸지 않았습니다.

### 검증

빌드:

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
```

코어 프로브:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

재현:

```text
REPIU_LINUX_X64_STACK_TRACE=1 \
REPIU_LINUX_X64_RETURN_TRACE=1 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
REPIU_LINUX_X64_RETURN_STACK_TAIL=48 \
./build/linux_x64/repiu pumpit2a
```

새 출력의 마지막 다섯 건입니다.

```text
[repiu-x64-return-stack-tail] index=13671 writer=direct-call site=0x0102A07D fallthrough=0x0102A082 esp=0x0158CC40 value=0x0102A082
[repiu-x64-return-stack-tail] index=13672 writer=guest-push site=0x010F1A80 esp=0x0158CC3C value=0x00000000
[repiu-x64-return-stack-tail] index=13673 writer=guest-push site=0x010F1A81 esp=0x0158CC38 value=0x0128CC2C
[repiu-x64-return-stack-tail] index=13674 writer=guest-push site=0x010F1A80 esp=0x0158CC40 value=0x00000004
[repiu-x64-return-stack-tail] index=13675 writer=guest-push site=0x010F1A81 esp=0x0158CC3C value=0x0128CC2C
```

환경 변수를 모두 뺀 실행은 기존과 같은 fault를 재현했습니다.

```text
[repiu-fault] unhandled signal=0xb rip=0x11a6440 eip=0x11a6440 access=0x37016be9 ...
```

### 확인한 사실

* LE object는 균일한 `+0x00FF0000` delta로 적재되고 `runtime_base`는
  `0x01000000`입니다. object 2는 `0x01010000`, object 4는 `0x01110000`입니다.
* `0x010F1A80`은 문자열 비교 함수, `0x010F1AF8`은 그 "같음" 경로의 `RET`,
  호출자는 `0x0102A065`의 5회 반복 루프입니다.
* 그 루프 블록은 초기 AOT map에 entry가 없고 동적으로 번역됩니다.
* 같은 `RET` site가 연속 두 번 resolver에 도달했고, 첫 번째는
  `0x0102A082`를 정상적으로 소비했습니다.
* 두 번째 반복은 `call 0x010F1A80`의 return 주소 push 없이 함수에
  진입했습니다. 진입 ESP가 `0x0158CC40`에서 `0x0158CC44`로 4 높아졌습니다.
* 실패한 `RET`이 소비한 `0x011A643A`는 ring index `485`에서 남은 오래된
  stack 잔여물이며, 실패 시점의 sequence는 `13676`입니다.

### 판단

Task 627이 남긴 "최종 writer 미확정"은 질문 자체가 잘못 놓여 있었습니다.
문제의 slot에는 실패 직전에 아무도 쓰지 않았고, 원인은 누락된 push입니다.
이번 작업은 그 사실을 확정한 진단 추가로 완료되었습니다. push가 누락된
이유는 아직 확정하지 못했으므로 다음 작업으로 남깁니다. 이 작업은 return
target이나 stack slot을 고치지 않았고 원인을 해결하지도 않았습니다.

### 다음 작업

동적 세대에서 `0x0102A07D`의 direct-call entry가 어떻게 배치되고 어떤
edge가 그곳으로 들어오는지 확인해야 합니다. 확인 대상은 call site의 동적
map entry와 emitted bytes, 그 direct-call fixup, 그리고 `0x0102A065`
back edge와 `0x0102A072` fallthrough가 가리키는 cache 주소입니다.

## English

### Result

Added `REPIU_LINUX_X64_RETURN_STACK_TAIL=<count>`. At the return target
selected by `REPIU_LINUX_X64_RETURN_REG_TRACE`, it prints the most recent
`count` records of the x64 dispatch frame's stack-write ring in write order.
The value is clamped to the ring capacity, and with the variable unset the
execution path and output are unchanged. Resolver policy and guest stack
semantics were not touched.

### Verification

Build:

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
```

Core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Reproduction:

```text
REPIU_LINUX_X64_STACK_TRACE=1 \
REPIU_LINUX_X64_RETURN_TRACE=1 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
REPIU_LINUX_X64_RETURN_STACK_TAIL=48 \
./build/linux_x64/repiu pumpit2a
```

The last five records of the new output:

```text
[repiu-x64-return-stack-tail] index=13671 writer=direct-call site=0x0102A07D fallthrough=0x0102A082 esp=0x0158CC40 value=0x0102A082
[repiu-x64-return-stack-tail] index=13672 writer=guest-push site=0x010F1A80 esp=0x0158CC3C value=0x00000000
[repiu-x64-return-stack-tail] index=13673 writer=guest-push site=0x010F1A81 esp=0x0158CC38 value=0x0128CC2C
[repiu-x64-return-stack-tail] index=13674 writer=guest-push site=0x010F1A80 esp=0x0158CC40 value=0x00000004
[repiu-x64-return-stack-tail] index=13675 writer=guest-push site=0x010F1A81 esp=0x0158CC3C value=0x0128CC2C
```

A run with every trace variable removed reproduced the same fault:

```text
[repiu-fault] unhandled signal=0xb rip=0x11a6440 eip=0x11a6440 access=0x37016be9 ...
```

### Facts established

* LE objects load with a uniform `+0x00FF0000` delta and `runtime_base` is
  `0x01000000`; object 2 is at `0x01010000` and object 4 at `0x01110000`.
* `0x010F1A80` is a string-compare function, `0x010F1AF8` is the `RET` on its
  "equal" path, and the caller is the five-iteration loop at `0x0102A065`.
* That loop block has no initial AOT map entry; it is translated dynamically.
* The same `RET` site reached the resolver twice in a row, and the first
  consumed `0x0102A082` correctly.
* The second iteration entered the function without the return-address push of
  `call 0x010F1A80`; entry ESP rose by four, from `0x0158CC40` to `0x0158CC44`.
* The `0x011A643A` the failing `RET` consumed is stale residue written at ring
  index `485`, against sequence `13676` at the failure.

### Assessment

Task 627's open "final writer" question was posed against the wrong slot:
nothing wrote that slot just before the failure, and the cause is a push that
did not run. This task is complete as the diagnostic that established that.
Why the push is missing is not yet determined and remains the next task. This
task did not repair a return target or a stack slot, and it did not resolve the
root cause.

### Next task

Establish how the dynamic generation lays out the direct-call entry for
`0x0102A07D` and which edge enters it. The items to inspect are the call site's
dynamic map entry and emitted bytes, its direct-call fixup, and the cache
addresses the `0x0102A065` back edge and the `0x0102A072` fallthrough target.
