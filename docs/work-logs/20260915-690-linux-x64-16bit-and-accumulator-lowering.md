# 작업 로그 20260915-690 — Linux x64 mode16 AND accumulator lowering

## 결과 요약

object 3의 mode16 frontier `0x0110002A: 25 FF 0F`를 공통 accumulator
immediate lowering으로 처리했습니다. mode16 `AND AX,0FFF`는 x64 cache에서
`66 25 FF 0F`로 실행되며, 원본 immediate와 instruction boundary를 보존합니다.

## 구현 및 검증

* `25 FF 0F`는 `k16BitAndAccumulatorImmediate`로 분류됩니다.
* lowering 결과는 `66 25 FF 0F`이고 instruction count는 1입니다.
* operand/address prefix 및 잘린 입력은 새 lowering에 포함되지 않습니다.
* x64 실행 probe는 seed `RAX=0xA5A5A5A51234F0F0`에 AND를 적용한 뒤
  `0xA5A5A5A5123400F0`을 확인했습니다.

```text
long_mode_16bit_and_accumulator=true,length=3,lowered=4,unsupported_variants=true
long_mode_lowering_16bit_and_accumulator=true,observed=0xa5a5a5a5123400f0
core_probe_failures=0
core_probe_all=true
```

실제 object-3 실행에서는 AND를 통과하고 `CD 31`을 기존 HLE 경로로 처리한
뒤, 다음 미지원 경계에서 멈췄습니다.

```text
[repiu-aot-plan-trace] guest=0x0110002D bytes=0E length=1 ... code_mode=16 ... hle=0
[repiu-fault] unhandled signal=0x5 ... eip=0x0110002E
```

실행은 `ulimit -c 0`으로 제한했으며 timeout 종료 메시지는 core-dump라고
표시했지만 `build` 아래에는 core 파일이 생성되지 않았습니다.

## 결론 및 다음 작업

이번 변경은 특정 주소나 특정 immediate 값이 아닌 opcode `25`, mode16 폭,
prefix 상태를 기준으로 하는 공통 규칙입니다. 다음 세션의 frontier는
mode16 `0x0110002D: 0E` (`PUSH CS`)이며, segment-stack semantics와 기존
HLE/fail-closed 정책을 분리해 조사해야 합니다.

---

# Work Log 20260915-690 — Linux x64 mode16 AND accumulator lowering

## Summary

Added shared accumulator-immediate lowering for the object-3 mode16 frontier
`0x0110002A: 25 FF 0F`. Mode16 `AND AX,0FFF` executes as `66 25 FF 0F` in the
x64 cache, preserving the original immediate and instruction boundary.

## Implementation and verification

* `25 FF 0F` classifies as `k16BitAndAccumulatorImmediate`.
* The lowering output is `66 25 FF 0F` with instruction count one.
* Operand/address-prefixed and truncated inputs do not use the new lowering.
* The x64 execution probe seeds `RAX=0xA5A5A5A51234F0F0`, executes the AND,
  and observes `0xA5A5A5A5123400F0`.

```text
long_mode_16bit_and_accumulator=true,length=3,lowered=4,unsupported_variants=true
long_mode_lowering_16bit_and_accumulator=true,observed=0xa5a5a5a5123400f0
core_probe_failures=0
core_probe_all=true
```

The live object-3 run passes the AND and routes `CD 31` through the existing HLE
path, then stops at the next unsupported boundary:

```text
[repiu-aot-plan-trace] guest=0x0110002D bytes=0E length=1 ... code_mode=16 ... hle=0
[repiu-fault] unhandled signal=0x5 ... eip=0x0110002E
```

The run used `ulimit -c 0`; although timeout reported a core-dump termination
message, no core file was created under `build`.

## Conclusion and next task

This is a shared rule based on opcode `25`, mode16 widths, and prefix state rather
than a specific address or immediate value. The next frontier is mode16
`0x0110002D: 0E` (`PUSH CS`); investigate its segment-stack semantics separately
from the existing HLE and fail-closed policies.
