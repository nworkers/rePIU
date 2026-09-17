# 작업 지시 20260915-683 — Linux x64 mode16 LOOPNZ lowering

## 배경

Task 682에서 `0x0110000E` mode16 LEA16 boundary를 제거한 뒤 runtime은
`0x01100012: E0 FF`에서 중단됩니다. 이 명령은 mode16 `LOOPNZ`이므로
x64 cache의 현재 INT3을 그대로 두면 SIGTRAP이 발생합니다.

## 범위

* mode16 prefix-free `LOOPNZ`의 공용 classifier verdict를 추가합니다.
* `CX` 16비트 감소와 원래 `ZF` 조건을 보존하는 x64 control-flow slot을
  구현합니다.
* 기존 conditional direct-target 및 block-fallthrough fixup 계약을
  재사용합니다.
* mode16 relative target offset을 executable code object의 relocated base에
  rebasing하여 linear guest CFG edge를 생성합니다.
* compatibility, emission, execution, Linux x64 runtime regression을
  추가합니다.
* address-size override, prefix/segment, 다른 mode16 branch 및 unresolved
  edge는 계속 fail-closed합니다.

## 구현 순서

1. [x] 실제 runtime `E0 FF` decode와 다음 frontier를 확인합니다.
2. [x] counter/flags/fixup 보존 설계를 작성합니다.
3. [x] compatibility classifier에 mode16 LOOPNZ lowering kind를 추가합니다.
4. [x] long-mode emitter에 전용 slot과 direct-edge 연결을 추가합니다.
5. [x] mode16 relative target rebasing을 planner에 추가하고 regression을
       작성합니다.
6. [x] compatibility/emission/lowering probe를 추가합니다.
7. [x] Linux x64 build와 core probe를 실행합니다.
8. [ ] object-3 dynamic trace에서 `0x01100012` boundary 제거와 다음
       frontier를 확인합니다.
9. [ ] analysis/work-log 갱신 후 checkpoint 커밋합니다.

## 제한

원본 guest bytes와 gameplay logic은 수정하지 않습니다. 특정 주소
`0x01100012` 또는 displacement `0xFF`에 종속된 예외를 추가하지 않으며,
mode16 `LOOP`, `LOOPZ`, address-size override, 16비트 PUSH/POP, MOV SS,
far return 및 일반 stack ABI는 이 작업에서 확장하지 않습니다.

## 완료 기준

1. prefix-free mode16 `LOOPNZ`만 전용 lowering으로 분류됩니다.
2. mode16 relative target이 code object base를 포함한 linear guest address로
   plan에 기록됩니다.
3. x64 slot이 `CX` wrap, saved `ZF`, flags 복원, taken/not-taken edge를
   probe로 통과합니다.
4. direct target이 unresolved일 때 전체 entry가 fail-closed로 중화되고,
   fallthrough fixup이 slot 내부를 덮어쓰지 않습니다.
5. Linux x64 core probe가 기존 regression과 함께 통과하고 runtime의
   다음 fault가 `E0 FF` 자체가 아닙니다.

---

# Work Order 20260915-683 — Linux x64 mode16 LOOPNZ lowering

## Background

After Task 682 removed the mode16 LEA16 boundary at `0x0110000E`, runtime
stops at `0x01100012: E0 FF`, a mode16 `LOOPNZ` whose x64-cache INT3 currently
causes SIGTRAP.

## Scope

Add a shared classifier for prefix-free mode16 `LOOPNZ`, emit an x64 slot that
preserves the 16-bit `CX` counter and original flags, reuse the existing
conditional-target and block-fallthrough fixups, and verify compatibility,
emission, execution, core-probe, and runtime behavior. Unsupported forms and
unresolved edges remain fail-closed. Mode16 relative targets are rebased from
the decoder's segment-relative IP offset to the relocated executable object's
linear guest address before CFG edges are emitted.

## Done criteria

The supported form is classified and emitted as a multi-instruction x64 slot;
probes verify relative-target rebasing, wrap, saved-ZF condition, flag
restoration, edge fixups, and whole-entry neutralisation; and the Linux x64 core
probe passes. The live-runtime criterion remains open: the far-transfer path
still permits the first mode16 instruction to execute as original long-mode
bytes, so the run reaches `0x0110000E` before the `LOOPNZ` slot. That prior
generic re-entry policy is recorded as Task 684 rather than hidden behind a
LOOPNZ-specific exception.
