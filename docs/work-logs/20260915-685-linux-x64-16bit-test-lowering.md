# 작업 로그 20260915-685 — Linux x64 mode16 TEST lowering

## 결과 요약

mode16 prefix-free `66 85 /r` register TEST subset을 공용 classifier와
byte-only lowerer에 추가했습니다. x64 cache는 operand-size override를
제거한 `85 /r`를 emit하며, memory/ESP/prefix 변형은 기존 fail-closed
boundary로 유지됩니다.

## 구현 및 검증

* `66 85 FF`가 `k16BitTest32ToGuestGprs`로 분류됩니다.
* lowering output은 `85 FF`, instruction count는 1입니다.
* x64 실행 probe에서 TEST의 ZF/CF/OF와 register state를 확인했습니다.
* compatibility probe 결과:

```text
long_mode_16bit_test32=true,length=3,lowered=2,unsupported_variants=true
long_mode_compatibility_all=true
```

* lowering/core probe 결과:

```text
long_mode_lowering_16bit_test32=true,flags=true,register=true
long_mode_lowering_all=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

* Linux x64 `repiu`도 빌드되었습니다.
* object-3 runtime trace에서 다음 전환을 확인했습니다.

```text
[repiu-aot-plan-trace] guest=0x01100004 bytes=6685FF ... code_mode=16
[repiu-aot-dynamic] stage=image-entry guest=0x01100004 bytes=85FF length=2
[repiu-fault] unhandled signal=0x5 ... eip=0x01100009
```

따라서 Task 684의 잘못된 원본 byte fallback에 이어 Task 685의 TEST
boundary도 제거되었습니다. 현재 SIGTRAP은 다음 mode16 `74 39` Jcc이며,
동적 이미지에는 아직 mode16 Jcc lowering이 없습니다.

## 결론 및 다음 작업

특정 주소나 register 예외 없이 mode16 32비트 register TEST를 일반
lowering했습니다. 다음 작업에서는 mode16 conditional branch의 target
rebasing과 x64 direct-branch slot 연결을 설계합니다.

---

# Work Log 20260915-685 — Linux x64 mode16 TEST lowering

## Summary

Added the prefix-free mode16 `66 85 /r` register TEST subset to the shared
classifier and byte-only lowerer. The x64 cache emits `85 /r` after removing
the operand-size override; memory, ESP, and prefixed variants remain
fail-closed boundaries.

## Implementation and verification

* `66 85 FF` classifies as `k16BitTest32ToGuestGprs`.
* The lowering output is `85 FF` with instruction count one.
* The x64 execution probe verifies TEST ZF/CF/OF and register state.
* Compatibility probe:

```text
long_mode_16bit_test32=true,length=3,lowered=2,unsupported_variants=true
long_mode_compatibility_all=true
```

* Lowering/core probe:

```text
long_mode_lowering_16bit_test32=true,flags=true,register=true
long_mode_lowering_all=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

* The Linux x64 `repiu` target also builds.
* The object-3 runtime trace now shows:

```text
[repiu-aot-plan-trace] guest=0x01100004 bytes=6685FF ... code_mode=16
[repiu-aot-dynamic] stage=image-entry guest=0x01100004 bytes=85FF length=2
[repiu-fault] unhandled signal=0x5 ... eip=0x01100009
```

Task 684's incorrect original-byte fallback and Task 685's TEST boundary are
therefore both removed. The current SIGTRAP is the next mode16 `74 39` Jcc;
the dynamic image does not yet lower mode16 conditional branches.

## Conclusion and next task

Mode16 32-bit register TEST is now a shared lowering without an address- or
register-specific exception. The next task will design mode16 conditional
branch target rebasing and its x64 direct-branch slot integration.
