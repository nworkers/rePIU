# 작업 로그 20260905-588 — Linux x64 full RIP attribution

설계: [20260905-588](../design/20260905-588-linux-x64-full-rip-attribution.md)  
작업 지시: [20260905-588](../work-orders/20260905-588-linux-x64-full-rip-attribution.md)

## 결과

Linux fault handler의 기존 async-signal-safe 미처리 fault 출력에 `rip=`을 추가했습니다.
`rip`는 kernel `ucontext_t`의 full host RIP이며, 기존 `eip`는 engine의 32-bit fault
ABI 값을 유지합니다. `FaultEvent` 및 signal resume 동작은 바꾸지 않았습니다.

`pumpit2a` 재현에서 `rip=0x402aaef7`, `eip=0x402aaef6` SIGTRAP와
`rip=0x402aaef7`, `eip=0x402aaef7` SIGILL을 얻었습니다. Linux executable의
`addr2line`/`objdump` 확인 결과 `0x402AAEF6`은
`RepiuLinuxX64ReturnThunk`의 unresolved `int3`, `0x402AAEF7`은 바로 인접한
`RecoverGuestStackException`의 `ud2`입니다.

따라서 `push es` HLE 다음 문제는 high-half RIP merge가 아니라 x64 return-dispatch
resolver가 null target을 반환한 것입니다. SIGILL은 미처리 INT3 뒤 인접 `ud2`가
실행된 결과입니다.

## 검증

* WSL `cmake --build build/linux_x64_debug --target repiu --parallel 4` 성공:
  `repiu_exe`, `repiu` 모두 빌드했습니다.
* `REPIU_GUEST_WATCH=0x010F4A96 timeout 5s build/linux_x64_debug/repiu pumpit2a`
  실행에서 새 `rip=` 필드와 위 fault 순서를 재현했습니다.
* `addr2line -f -C -e build/linux_x64_debug/repiu 0x402aaef7`은
  `RecoverGuestStackException` / `guest_stack_recover_x64.S:64`를 가리켰습니다.
* `objdump -d -Mintel --start-address=0x402aaee8 --stop-address=0x402aaf08`
  은 `0x402aaef6: int3`, `0x402aaef7: ud2`를 확인했습니다.

## 후속 작업

`RepiuLinuxX64ReturnThunk`의 guest source와 resolver 반환 target을 기록하여 null
target의 원인을 cache miss, dispatch 설치 누락, 또는 resolver 정책으로 분리합니다.

---

# Work log 20260905-588 — Linux x64 full-RIP attribution

Design: [20260905-588](../design/20260905-588-linux-x64-full-rip-attribution.md)  
Work order: [20260905-588](../work-orders/20260905-588-linux-x64-full-rip-attribution.md)

## Result

The Linux fault handler now adds `rip=` to its existing async-signal-safe
unhandled-fault line. `rip` is the full host RIP from the kernel `ucontext_t`;
the existing `eip` remains the engine's 32-bit fault ABI value. Neither the
`FaultEvent` ABI nor signal resumption changed.

Reproduction produced SIGTRAP with `rip=0x402aaef7`, `eip=0x402aaef6`, then
SIGILL with both fields at `0x402aaef7`. `addr2line` and `objdump` identify
`0x402AAEF6` as `RepiuLinuxX64ReturnThunk`'s unresolved `int3`, and the adjacent
`0x402AAEF7` as `RecoverGuestStackException`'s `ud2`.

Thus the failure after segment-PUSH HLE is not a high-half RIP merge. The Linux
x64 return-dispatch resolver returned a null target; SIGILL is the adjacent
`ud2` reached after the unhandled INT3.

## Verification

* WSL Linux x64 `repiu_exe` and `repiu` built successfully.
* The five-second watched `pumpit2a` run reproduced the new `rip=` field and
  fault sequence.
* `addr2line` identified `RecoverGuestStackException` at
  `guest_stack_recover_x64.S:64`; `objdump` confirmed `int3` followed by `ud2`.

## Follow-up

Record the guest source and resolver-return target in `RepiuLinuxX64ReturnThunk`
to separate cache miss, missing dispatch installation, and resolver policy.
