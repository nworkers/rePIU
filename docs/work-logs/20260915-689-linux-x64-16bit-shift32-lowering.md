# 작업 로그 20260915-689 — Linux x64 mode16 32-bit shift lowering

## 결과 요약

object 3의 mode16 frontier `0x01100017: 66 C1 E9 10`을 공통 operand-size
lowering으로 처리했습니다. mode16에서 `66`이 선택한 32비트 `SHR ECX,16`을
long mode에서 `C1 E9 10`으로 실행하며, guest SP/memory/address 변형은
fail-closed로 유지됩니다.

## 구현 및 검증

* `66 C1 E9 10`은 `k16BitShift32ToGuestGprs`로 분류됩니다.
* lowering 결과는 `C1 E9 10`이고 instruction count는 1입니다.
* prefix 없는 16비트 shift, address-size, memory, guest-SP 변형은 새
  lowering에 포함되지 않습니다.
* x64 실행 probe에서 seed 값을 32비트 shift한 결과 `0x1234`를 확인했습니다.

```text
long_mode_16bit_shift32=true,length=4,lowered=3,unsupported_variants=true
long_mode_lowering_16bit_shift32=true,observed=0x1234
core_probe_failures=0
core_probe_all=true
```

실제 object-3 trace는 shift를 통과하고 `CD 31`을 HLE 경로로 처리한 뒤,
다음 미지원 mode16 경계에서 멈췄습니다.

```text
[repiu-aot-plan-trace] guest=0x0110001B bytes=CD31 length=2 ... code_mode=16 ... hle=1
[repiu-fault] unhandled signal=0x5 ... eip=0x0110002A
```

`0x0110002A: 25 FF 0F`는 mode16 `AND AX,0FFF`입니다. 실행은
`ulimit -c 0`으로 제한했으며 timeout 종료 메시지는 core-dump라고 표시했지만
`build` 아래에는 core 파일이 생성되지 않았습니다.

## 결론 및 다음 작업

이번 변경은 특정 주소나 특정 shift count가 아니라 `C1` opcode, mode16
operand/address 폭, 단일 `66` prefix, register-only 및 guest SP 제외 조건을
기반으로 합니다. 다음 세션의 frontier는 mode16 accumulator immediate
`25 FF 0F`이며, long mode에서 `66 25 FF 0F`로 유지할 수 있는지 확인해야
합니다.

---

# Work Log 20260915-689 — Linux x64 mode16 32-bit shift lowering

## Summary

Added shared operand-size lowering for the object-3 mode16 frontier
`0x01100017: 66 C1 E9 10`. The mode16 32-bit `SHR ECX,16` executes as
`C1 E9 10` in long mode, while guest-SP, memory, and address variants remain
fail-closed.

## Implementation and verification

* `66 C1 E9 10` classifies as `k16BitShift32ToGuestGprs`.
* The lowering output is `C1 E9 10` with instruction count one.
* Prefix-free 16-bit, address-size, memory, and guest-SP variants do not use the
  new lowering.
* The x64 execution probe verifies the 32-bit shift result `0x1234`.

```text
long_mode_16bit_shift32=true,length=4,lowered=3,unsupported_variants=true
long_mode_lowering_16bit_shift32=true,observed=0x1234
core_probe_failures=0
core_probe_all=true
```

The live object-3 trace passes the shift and routes `CD 31` through the HLE
path, then stops at the next unsupported mode16 boundary:

```text
[repiu-aot-plan-trace] guest=0x0110001B bytes=CD31 length=2 ... code_mode=16 ... hle=1
[repiu-fault] unhandled signal=0x5 ... eip=0x0110002A
```

`0x0110002A: 25 FF 0F` is mode16 `AND AX,0FFF`. The run used `ulimit -c 0`;
although timeout reported a core-dump termination message, no core file was
created under `build`.

## Conclusion and next task

This is a shared rule based on the `C1` opcode, mode16 operand/address widths,
the single `66` prefix, register-only form, and guest-SP exclusion rather than
a specific address or shift count. The next frontier is mode16 accumulator
immediate `25 FF 0F`; determine whether `66 25 FF 0F` preserves it in long
mode.
