# 작업 로그 20260915-688 — Linux x64 mode16 MOV register lowering

## 결과 요약

object 3의 mode16 frontier `0x01100015: 89 CA`를 공통 register-register
lowering으로 처리했습니다. mode16 `MOV DX,CX`는 x64 cache에서 `66 89 CA`로
실행되며, `89/8B`의 register-only GPR subset을 같은 규칙으로 처리합니다.
guest SP가 포함된 ModRM과 memory/prefix 변형은 fail-closed로 유지됩니다.

## 구현 및 검증

* `89 CA`와 `8B D1`은 `k16BitMovRegisterToGuestGprs`로 분류됩니다.
* lowering 결과는 각각 `66 89 CA`, `66 8B D1`이고 instruction count는 1입니다.
* `89 C4` guest-SP 형식, memory 형식, operand-size prefix 형식은 새 lowering에
  포함되지 않습니다.
* x64 실행 probe에서 destination `RDX=0xA5A5A5A512340000`의 low word만
  source `RCX` 값으로 바뀌어 `0xA5A5A5A512340007`이 되는 것을 확인했습니다.

```text
long_mode_16bit_mov_register=true,length=2,lowered=3,unsupported_variants=true
long_mode_lowering_16bit_mov_register=true,observed=0xa5a5a5a512340007
core_probe_failures=0
core_probe_all=true
```

실제 object-3 실행 trace에서는 `89 CA` 경계를 통과하고 다음 mode16 shift
경계에서 멈췄습니다.

```text
[repiu-aot-plan-trace] guest=0x01100017 bytes=66C1E910 length=4 ... code_mode=16
[repiu-fault] unhandled signal=0x5 ... eip=0x01100017
```

실행은 `ulimit -c 0`으로 수행했으며, timeout이 core-dump 종료 메시지를
표시했지만 `build` 아래에는 core 파일이 생성되지 않았습니다.

## 결론 및 다음 작업

이번 변경은 특정 주소나 특정 레지스터 값에 대한 예외처리가 아니라 opcode,
operand/address 폭, prefix 상태, register-only 및 guest SP 제외 조건을 갖는
공통 규칙입니다. 다음 세션의 frontier는 mode16 `66 C1 E9 10`이며, mode16의
32비트 shift를 x64에서 `C1 E9 10`으로 lower할 수 있는지 확인해야 합니다.

---

# Work Log 20260915-688 — Linux x64 mode16 MOV register lowering

## Summary

Added shared register-register lowering for the object-3 mode16 frontier
`0x01100015: 89 CA`. Mode16 `MOV DX,CX` executes as `66 89 CA` in the x64
cache, and the same rule covers the register-only GPR subset of `89/8B`.
ModRM forms involving guest SP, memory, or prefixes remain fail-closed.

## Implementation and verification

* `89 CA` and `8B D1` classify as `k16BitMovRegisterToGuestGprs`.
* Their lowerings are `66 89 CA` and `66 8B D1`, each with instruction count one.
* `89 C4` guest-SP, memory, and operand-size-prefixed forms do not use the new
  lowering.
* The x64 execution probe seeds destination `RDX` with
  `0xA5A5A5A512340000`, moves the source `RCX` low word, and observes
  `0xA5A5A5A512340007`.

```text
long_mode_16bit_mov_register=true,length=2,lowered=3,unsupported_variants=true
long_mode_lowering_16bit_mov_register=true,observed=0xa5a5a5a512340007
core_probe_failures=0
core_probe_all=true
```

The live object-3 trace passes the `89 CA` boundary and stops at the next
mode16 shift boundary:

```text
[repiu-aot-plan-trace] guest=0x01100017 bytes=66C1E910 length=4 ... code_mode=16
[repiu-fault] unhandled signal=0x5 ... eip=0x01100017
```

The run used `ulimit -c 0`; `timeout` reported a core-dump termination message,
but no core file was created under `build`.

## Conclusion and next task

This is a shared rule based on opcode, operand/address widths, prefix state,
register-only form, and guest-SP exclusion rather than a specific address or
register value. The next frontier is mode16 `66 C1 E9 10`; determine whether its
32-bit shift can be lowered to `C1 E9 10` in long mode.
