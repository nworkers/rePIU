# 작업 로그 20260915-683 — Linux x64 mode16 LOOPNZ lowering

## 결과 요약

mode16 `LOOPNZ`를 특정 주소 예외가 아닌 공용 classifier와 x64 전용
control-flow slot으로 처리했습니다. mode16 near-relative target을 code
object base에 rebasing하는 planner 수정도 함께 반영했습니다. compatibility,
emission, lowering probe와 Linux x64 core probe는 통과했습니다.

실제 게임 실행은 아직 완료되지 않았습니다. far transfer가
`0x01100004`로 정상 해석된 뒤 정적 map에 없는 target을 legacy fallback으로
원본 long-mode bytes에서 실행했고, mode16 `B8 07 00`을 x64가
`MOV EAX,0x66670007`로 소비하여 EIP가 `0x0110000E`로 이동했습니다.
따라서 `LOOPNZ` slot까지 도달하지 못했으며, 이번 작업은 이 사실을 숨기지
않는 checkpoint로 남깁니다.

## 확인된 원인

`CanResumeLinuxX64LegacyTarget`는 legacy-32 기본 decoder를 사용하고 첫
명령만 호환 여부를 확인했습니다. object 3은 `OBJBIGDEF`가 없는 mode16
code object이므로 `0x01100004: 66 85 FF`와 `0x01100009: B8 07 00`은
32-bit 기본 해석과 다릅니다. 첫 명령만 legacy-32로 보아 통과시키는 공통
re-entry 정책이 mode16 original-byte 실행을 허용한 것이 SIGTRAP 이전의
직접 원인입니다. 이는 `LOOPNZ` 또는 `0x01100012`에 종속된 문제가
아닙니다.

## 구현 및 검증

* prefix-free mode16 `E0 cb`를 `CX` 기반 `k16BitLoopNzToGuestCx`로 분류했습니다.
* x64 slot에서 flags를 저장하고 `CX`를 감소·검사한 뒤 원래 `ZF`를
  적용하고, existing conditional/fallthrough fixup을 사용하도록 했습니다.
* mode16 relative target을 해당 executable code range의 relocated base와
  결합하도록 planner를 수정했습니다.
* unresolved direct target에서는 entry 전체를 `INT3`로 중화하고
  fallthrough suffix를 별도로 처리하는 probe를 추가했습니다.
* Linux x64 build와 core probe 결과:

```text
long_mode_16bit_loopnz=true,length=2,target_free_lowerer_refused=true,planner_target_rebased=true
long_mode_compatibility_all=true
long_mode_emission_16bit_loopnz=true,slot=1,conditional_fixup=1,fallthrough_fixup=1
long_mode_emission_16bit_loopnz_unresolved=true,entry=1,fallthrough=1
long_mode_emission_all=true
long_mode_lowering_16bit_loopnz=true
long_mode_lowering_all=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

## 다음 작업

Task 684에서 code-mode metadata를 `CanResumeLinuxX64LegacyTarget`에
반영하고, non-identical mode16 continuation을 original bytes로 통과시키지
않도록 공통 re-entry 정책을 수정합니다. 그 뒤 mode16 dynamic entry
`0x01100004`에서 첫 boundary가 되는 일반 operand-width/control-flow
형식을 순서대로 설계합니다.

---

# Work log 20260915-683 — Linux x64 mode16 LOOPNZ lowering

## Summary

Implemented a shared mode16 `LOOPNZ` classifier and a dedicated x64
control-flow slot, without adding an address-specific exception. The planner
now rebases mode16 near-relative targets against the relocated executable
code-object base. Compatibility, emission, lowering, and Linux x64 core probes
passed.

The live game run is not complete. After the far transfer resolved to
`0x01100004`, the target was absent from the static map and entered the legacy
fallback path. Original long-mode execution consumed mode16 `B8 07 00` as
`MOV EAX,0x66670007`, moving EIP to `0x0110000E`. The run therefore never
reached the `LOOPNZ` slot; this task is recorded as an explicit checkpoint.

## Confirmed cause

`CanResumeLinuxX64LegacyTarget` used the legacy-32 default decoder and checked
only the first instruction. Object 3 is a mode16 code object without
`OBJBIGDEF`, so `0x01100004: 66 85 FF` and `0x01100009: B8 07 00` do not have
the legacy-32 meanings assumed by the gate. That shared re-entry policy
allowed mode16 original-byte execution and caused the direct pre-SIGTRAP
failure. The issue is not specific to `LOOPNZ` or `0x01100012`.

## Verification

The mode16 classifier, x64 emission, unresolved-edge neutralisation, and
standalone execution probes all passed. The Linux x64 build and core probe
reported `core_probe_total=27`, `core_probe_failures=0`, and
`core_probe_all=true`.

## Next task

Task 684 will feed code-mode metadata into
`CanResumeLinuxX64LegacyTarget`, preventing non-identical mode16 continuations
from passing through original long-mode bytes. It will then design the next
generic mode16 operand-width/control-flow lowerings from dynamic entry
`0x01100004`.
