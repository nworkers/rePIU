# 작업 지시 20260914-680 — Linux x64 16-bit 스택 레지스터 lowering

## 목표

Task 679의 mode-aware planner가 만든 첫 16-bit `MOV SP, imm16`을 Linux x64
cache에서 일반적인 instruction-mode 규칙으로 lowering하고, 증명되지 않은
16-bit 명령이 32-bit 전용 native slot에 들어가지 않도록 닫습니다.

## 범위

* long-mode compatibility API에 guest mode 입력을 추가합니다.
* 16-bit `BC iw`를 `66 41 BF iw`로 lowering합니다.
* x64 emitter가 record mode를 전달하고 16-bit non-copy를 boundary로 보냅니다.
* planner/dynamic trace에 mode를 출력합니다.
* compatibility, lowering, emission/core probe를 갱신합니다.

## 구현 순서

1. [x] Task 679 설계·로그와 기존 x64 stack-pointer lowering을 대조합니다.
2. [x] mode-aware classifier/lowerer와 전용 16-bit lowering을 구현합니다.
3. [x] x64 emitter의 16-bit fail-closed gate와 mode trace를 연결합니다.
4. [x] compatibility/lowering/emission probe regression을 추가합니다.
5. [ ] Linux x64 build와 core probe를 실행합니다.
6. [ ] WSL runtime smoke 가능 여부와 object 3 trace를 확인합니다.
7. [x] analysis, ARCHITECTURE, work log를 갱신하고 커밋합니다.

## 제한

* 특정 guest 주소 `0x01100022` 또는 특정 immediate를 조건으로 사용하지
  않습니다.
* 원본 guest bytes와 gameplay logic을 수정하지 않습니다.
* 16-bit push/pop, far return, segment-load, 일반 HLE stack ABI는 이번
  단위에서 구현하지 않습니다.
* 기존 `k32` API caller의 기본 동작을 변경하지 않습니다.

## 완료 기준

1. mode16 `BC iw`가 정확히 길이 3으로 분류되고 `66 41 BF iw`로 lowering됩니다.
2. x64 emitter가 mode16 non-copy record를 기존 32-bit native slot으로 보내지
   않고 boundary로 닫습니다.
3. mode16 lowering의 실제 실행 probe가 guest ESP low-word write와 upper-word
   보존을 확인합니다.
4. Linux x64 core probe가 기존 regression을 포함하여 통과합니다.
5. WSL이 가능하면 object 3 trace에서 `BC0020`, length 3, mode16이 보이고,
   이전의 `41BF0020FB8D`가 사라지는 것을 기록합니다.

---

# Work Order 20260914-680 — Linux x64 16-bit stack-register lowering

## Objective

Lower the first 16-bit `MOV SP, imm16` produced by Task 679's mode-aware
planner using a general instruction-mode rule, and prevent unproven 16-bit
instructions from entering 32-bit-only native slots in the Linux x64 cache.

## Scope

* Add guest mode input to the long-mode compatibility API.
* Lower 16-bit `BC iw` to `66 41 BF iw`.
* Pass record mode through the x64 emitter and boundary all 16-bit non-copy
  records.
* Add mode to planner and dynamic trace output.
* Update compatibility, lowering, emission, and core-probe regressions.

## Implementation order

1. [x] Compare Task 679's design/log with the existing x64 stack-pointer
   lowering.
2. [x] Implement the mode-aware classifier/lowerer and dedicated 16-bit form.
3. [x] Connect the x64 emitter's 16-bit fail-closed gate and mode trace.
4. [x] Add compatibility/lowering/emission probe regressions.
5. [ ] Run the Linux x64 build and core probe.
6. [ ] Check WSL runtime-smoke availability and object-3 trace.
7. [x] Update analysis, ARCHITECTURE, and the work log, then commit.

## Limits

* Do not key behavior to guest address `0x01100022` or a particular immediate.
* Do not modify original guest bytes or gameplay logic.
* Do not implement 16-bit push/pop, far return, segment-load, or a general HLE
  stack ABI in this unit.
* Preserve the existing default behavior of `k32` API callers.

## Done criteria

1. A mode16 `BC iw` is classified at length 3 and lowers exactly to
   `66 41 BF iw`.
2. The x64 emitter boundaries mode16 non-copy records instead of sending them
   to existing 32-bit native slots.
3. An execution probe confirms the low-word guest ESP write and upper-word
   preservation.
4. The Linux x64 core probe passes, including existing regressions.
5. If WSL is available, the object-3 trace reports `BC0020`, length 3, mode16,
   and no longer emits the old `41BF0020FB8D` sequence.
