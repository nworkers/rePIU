# 작업 로그 20260915-687 — Linux x64 mode16 MOV immediate lowering

## 결과 요약

Task 686 이후 object 3의 mode16 frontier였던 `0x01100009: B8 07 00`을
공통 classifier와 byte-only lowerer로 처리했습니다. prefix-free `B8+r iw`
중 guest SP를 제외한 GPR 형식은 x64 cache에서 `66 B8+r iw`로 실행되며,
원본 guest 바이트는 변경하지 않습니다. `BC iw`는 기존 Task 680의 R15W
전용 lowering을 계속 사용합니다.

## 구현 및 검증

* `B8 07 00`은 `k16BitMovImmediateToGuestGprs`로 분류됩니다.
* lowering 결과는 `66 B8 07 00`, instruction count는 1입니다.
* `66` prefix, `67` address-size prefix, 잘린 입력은 새 lowering에 포함하지
  않습니다.
* mode16 `BC 00 20`은 `k16BitStackPointerImmediateToR15` 경로를 유지합니다.
* x64 실행 probe는 `RAX=0xA5A5A5A512340000`에 `MOV AX,7`을 적용한 뒤
  `0xA5A5A5A512340007`을 확인했습니다.

```text
long_mode_16bit_mov_immediate=true,length=3,lowered=4,unsupported_variants=true
long_mode_lowering_16bit_mov_immediate=true,observed=0xa5a5a5a512340007
core_probe_failures=0
core_probe_all=true
```

Linux x64 `repiu` 빌드도 성공했습니다. 실제 object-3 실행 trace는
TEST/Jcc/MOV immediate를 통과했고, 다음 미지원 mode16 경계에서 멈췄습니다.

```text
[repiu-aot-plan-trace] guest=0x01100015 bytes=89CA length=2 ... code_mode=16
[repiu-fault] unhandled signal=0x5 ... eip=0x01100015
```

이번 실행은 core dump 파일 생성을 막도록 `ulimit -c 0`으로 제한했으며,
실행 종료 시 timeout이 core-dump 종료 메시지를 표시했지만 `build` 아래에는
core 파일이 생성되지 않았습니다. SIGTRAP의 실제 원인은 기존 fail-closed
INT3 boundary입니다. 다음 세션의 frontier는 mode16 `89 CA` (`MOV DX,CX`)입니다.

## 결론 및 다음 작업

이번 lowering은 특정 주소나 특정 immediate 값에 대한 예외처리가 아니라,
mode16 opcode 범위·operand/address 폭·prefix 조건·guest SP 분리를 기준으로
하는 공통 규칙입니다. 다음 작업에서는 mode16 word GPR 이동(`89 CA`)의
register-register semantics와 이후 `66 C1 E9 10`, `CD 31` 경계를 조사합니다.

---

# Work Log 20260915-687 — Linux x64 mode16 MOV immediate lowering

## Summary

Added shared classifier and byte-only lowering support for the object-3 mode16
frontier `0x01100009: B8 07 00`. Prefix-free `B8+r iw` forms for GPRs other
than guest SP execute as `66 B8+r iw` in the x64 cache without modifying the
original guest bytes. `BC iw` remains on Task 680's dedicated R15W lowering.

## Implementation and verification

* `B8 07 00` classifies as `k16BitMovImmediateToGuestGprs`.
* The lowering output is `66 B8 07 00` with instruction count one.
* `66` operand-size, `67` address-size, and truncated variants do not use the
  new lowering.
* Mode16 `BC 00 20` continues to use
  `k16BitStackPointerImmediateToR15`.
* The x64 execution probe seeds `RAX` with `0xA5A5A5A512340000`, executes
  `MOV AX,7`, and observes `0xA5A5A5A512340007`.

```text
long_mode_16bit_mov_immediate=true,length=3,lowered=4,unsupported_variants=true
long_mode_lowering_16bit_mov_immediate=true,observed=0xa5a5a5a512340007
core_probe_failures=0
core_probe_all=true
```

The Linux x64 `repiu` target also builds successfully. The live object-3 trace
passes TEST, Jcc, and the immediate MOV, then stops at the next unsupported
mode16 boundary:

```text
[repiu-aot-plan-trace] guest=0x01100015 bytes=89CA length=2 ... code_mode=16
[repiu-fault] unhandled signal=0x5 ... eip=0x01100015
```

The runtime was executed with `ulimit -c 0`; although `timeout` reported a
core-dump termination message, no core file was created under `build`. The
actual SIGTRAP source is the existing fail-closed INT3 boundary. The
next-session frontier is mode16 `89 CA` (`MOV DX,CX`).

## Conclusion and next task

This lowering is a shared rule based on the mode16 opcode range, operand and
address widths, prefix state, and guest-SP separation; it is not an address- or
immediate-specific exception. The next task should investigate mode16 word
GPR register-register moves (`89 CA`) and then the `66 C1 E9 10` and `CD 31`
boundaries.
