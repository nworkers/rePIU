# 작업 로그 20260915-686 — Linux x64 mode16 conditional branch lowering

## 결과 요약

mode16 conditional branch를 기존 planner rebasing과 x64 direct-branch slot에
연결했습니다. mode16 `74 cb`는 planner가 code-object base를 포함한
linear target을 기록하고, emitter는 condition을 유지하는 `0F 8x rel32`를
생성합니다. direct target과 block fallthrough는 각각 기존 fixup 계약으로
처리됩니다.

## 구현 및 검증

* mode16 synthetic `74 01` planner probe가 length 2와 rebased target을
  확인했습니다.
* mode16 JZ emission probe가 `0F 84 rel32`, conditional fixup,
  block-fallthrough fixup을 확인했습니다.
* unresolved conditional target은 branch entry 전체와 fallthrough edge를
  기존 INT3 neutralisation으로 처리했습니다.

```text
long_mode_16bit_jcc_plan=true,length=2,target_rebased=true
long_mode_emission_16bit_jcc=true,slot=1,conditional_fixup=1,fallthrough_fixup=1
long_mode_emission_16bit_jcc_unresolved=true,entry=1,fallthrough=1
core_probe_failures=0
core_probe_all=true
```

* Linux x64 `repiu` 빌드도 통과했습니다.
* object-3 runtime trace에서 TEST 이후 Jcc가 실행되어 다음 instruction으로
  넘어가는 것을 확인했습니다.

```text
[repiu-aot-plan-trace] guest=0x01100009 bytes=B80700 length=3 ... code_mode=16
[repiu-aot-dynamic] stage=image-entry guest=0x01100004 bytes=85FF length=2
[repiu-fault] unhandled signal=0x5 ... eip=0x01100009
```

정적 object bytes 기준 Jcc는 `0x01100007: 74 39`이고, `ZF=0` 상태에서
not-taken 경로가 `0x01100009`로 진행했습니다. 따라서 Jcc INT3 boundary는
제거되었고, 현재 SIGTRAP은 mode16 `B8 07 00`입니다.

## 결론 및 다음 작업

mode16 Jcc를 단일 mnemonic이나 주소 예외 없이 공용 direct-branch slot으로
처리했습니다. 다음 frontier는 `0x01100009: B8 07 00` mode16
`MOV AX,7`이며, `66 B8 07 00` operand-width lowering이 필요합니다.

---

# Work Log 20260915-686 — Linux x64 mode16 conditional branch lowering

## Summary

Connected mode16 conditional branches to the existing planner rebasing and x64
direct-branch slot. The planner records a linear target including the
code-object base for mode16 `74 cb`, and the emitter produces `0F 8x rel32`
while preserving the condition. Direct-target and block-fallthrough edges use
the existing fixup contracts independently.

## Implementation and verification

* The mode16 synthetic `74 01` planner probe verifies length two and the
  rebased target.
* The mode16 JZ emission probe verifies `0F 84 rel32`, the conditional fixup,
  and the block-fallthrough fixup.
* An unresolved conditional target uses the existing whole-entry INT3
  neutralisation and separately neutralises the fallthrough edge.

```text
long_mode_16bit_jcc_plan=true,length=2,target_rebased=true
long_mode_emission_16bit_jcc=true,slot=1,conditional_fixup=1,fallthrough_fixup=1
long_mode_emission_16bit_jcc_unresolved=true,entry=1,fallthrough=1
core_probe_failures=0
core_probe_all=true
```

* The Linux x64 `repiu` target also builds.
* The object-3 runtime trace confirms that Jcc executes after TEST and reaches
  the following instruction:

```text
[repiu-aot-plan-trace] guest=0x01100009 bytes=B80700 length=3 ... code_mode=16
[repiu-aot-dynamic] stage=image-entry guest=0x01100004 bytes=85FF length=2
[repiu-fault] unhandled signal=0x5 ... eip=0x01100009
```

In the static object bytes, the Jcc is `0x01100007: 74 39`; with `ZF=0`, its
not-taken path reaches `0x01100009`. The Jcc INT3 boundary is therefore gone,
and the current SIGTRAP is mode16 `B8 07 00`.

## Conclusion and next task

Mode16 Jcc now uses the shared direct-branch slot without a mnemonic- or
address-specific exception. The next frontier is `0x01100009: B8 07 00`, a
mode16 `MOV AX,7`, which needs the operand-width lowering `66 B8 07 00`.
