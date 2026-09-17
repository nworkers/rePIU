# 작업 로그 20260914-679 — Linux x64 혼합 모드 게스트 디코드

## 결과 요약

이번 작업은 게임 전체 실행 성공이 아니라, coredump 직전의 잘못된 게스트
스택 포인터가 16-bit LE code object를 32-bit로 디코드한 결과임을 확인하고,
그 일반 원인을 수정한 checkpoint를 남기는 것으로 마무리했다.

## 확인된 원인

object 3의 LE flags는 `0x1045`(`OBJALIAS16`, `OBJBIGDEF` 없음)이다. 따라서
`0x01100022`의 바이트 `BC 00 20`은 16-bit `MOV SP,0x2000`이어야 한다.
기존 Linux x64 AOT planner는 모든 object를 `LEGACY_32`로 디코드하여 다음
바이트까지 소비하는 5-byte `MOV ESP,0x8DFB2000`을 만들었다. 그 결과 x64
게스트 ESP가 arena 밖의 `0x8DFB2000`으로 변하고, 뒤따른 `PUSH ES` HLE의
범위 거부가 SIGTRAP 및 후속 SIGSEGV/coredump로 관찰되었다.

이것은 특정 명령이나 주소에 대한 예외처리 문제가 아니라, LE code object의
기본 operand size가 AOT planner에 전달되지 않은 공통 설계 문제이다.

## 반영한 변경

* `RuntimeCodeModeRange`를 추가하여 executable LE object의 mode metadata를
  relocated image에서 AOT plan, cache image, placement, dynamic append까지
  전달했다.
* AOT planner에 `LEGACY_16`과 `LEGACY_32` decoder를 추가하고, 명령 주소가
  속한 executable object의 `OBJBIGDEF`에 따라 선택하도록 했다.
* `AotInstructionRecord`에 게스트 기본 operand size를 기록했다.
* x64 cache emitter는 아직 전용 lowering이 없는 16-bit 명령을 legacy-32
  compatibility 경로로 보내지 않고 fail-closed 처리한다.
* Task 678의 Linux x64 shutdown recovery 변경과 기존 진단 trace도 현재
  branch checkpoint에 함께 보존했다. 진단 환경 변수와 임시 trace는 후속
  원인 확인이 끝나면 정리할 수 있다.

## 검증

성공한 빌드:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc \
  'cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_x64_debug \
   --target repiu repiu_core_probe -j2'
```

결과:

```text
[100%] Built target repiu
[100%] Built target repiu_core_probe
core_probe_total=27
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
core_probe_host=x64 (Task 545: i386 assembly probes are not built)
```

코어 프로브에는 임시 16-bit 바이트 진단도 포함되어 있으며, 현재 classifier
호출 자체는 legacy-32 기본 API를 사용한다. 따라서 planner의 실제 object-3
mode trace와 전체 게임 재실행은 아직 완료하지 않았다.

## 미완료 및 다음 세션 시작점

1. `REPIU_EXECUTION_BACKEND=dynamic`, `REPIU_LAUNCHER=0`,
   `REPIU_AOT_PLAN_TRACE=0x01100022`, `REPIU_AOT_DYNAMIC_CONTAINS=0x01100022`
   로 object 3의 post-change plan/cache trace를 확인한다.
2. trace에서 `0x01100022`가 `BC0020`, length 3, 16-bit으로 나타나는지와
   `41BF0020FB8D`가 사라지는지 확인한다.
3. 첫 fail-closed 16-bit 명령을 기준으로 일반적인 16-bit lowering 또는 HLE
   dispatcher를 설계한다. `MOV SP`, 16-bit stack push/pop, `PUSH CS`,
   `MOV SS`, `AND/LEA`, far return은 stack width/base/limit ABI를 함께
   다뤄야 한다.
4. 16-bit classifier/lowerer API에 명시적인 mode 인자를 연결할지 결정하고,
   전용 lowering이 증명된 명령만 x64 cache에 허용한다.
5. 정상 화면·입력·종료와 coredump 부재를 확인한 후 진단 trace를 정리한다.

## Git 상태

현재 작업은 `work/20260913-678-linux-x64-coredump-investigation` 브랜치에서
checkpoint 커밋으로 보존한다. `main` 머지는 요청받지 않았으므로 수행하지
않으며, 다음 세션에서 이 브랜치에서 계속 진행한다.

---

# Work log 20260914-679 — Linux x64 mixed-mode guest decoding

## Summary

This task stops at a checkpoint rather than claiming full-game success. It
confirmed that the malformed guest stack pointer before the coredump came from
decoding a 16-bit LE code object as 32-bit, and recorded the general fix and
remaining work.

## Confirmed cause

Object 3 has LE flags `0x1045` (`OBJALIAS16`, without `OBJBIGDEF`). Therefore
the bytes `BC 00 20` at `0x01100022` are a 16-bit `MOV SP,0x2000`.
The old Linux x64 AOT planner decoded every object as `LEGACY_32`, consuming the
next bytes as a five-byte `MOV ESP,0x8DFB2000`. x64 guest ESP then became the
out-of-arena value `0x8DFB2000`; the subsequent `PUSH ES` HLE range rejection
was observed as SIGTRAP followed by SIGSEGV/coredump.

This is a shared missing code-mode propagation problem, not an exception keyed
to one instruction or address.

## Changes made

* Added `RuntimeCodeModeRange` and propagated executable LE object mode metadata
  from the relocated image through the AOT plan, cache image, placement, and
  dynamic append.
* Added `LEGACY_16` and `LEGACY_32` planner decoders selected from the containing
  executable object's `OBJBIGDEF` flag.
* Recorded the guest default operand size in `AotInstructionRecord`.
* Made the x64 cache emitter fail closed for 16-bit instructions without a
  dedicated proven lowering instead of sending them through legacy-32
  compatibility emission.
* Preserved the Task 678 Linux x64 shutdown-recovery changes and existing
  diagnostic traces in this branch checkpoint. The diagnostic environment
  variables and temporary traces can be removed after the next causal run.

## Verification

The Linux x64 target and core probe built successfully. The core probe reported
`core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`, with two
expected skipped probes (`stack_bridge`, `guest_stack_switch`) on x64. The
temporary 16-bit diagnostic is present, but the classifier call still uses the
legacy-32-default API; the post-change object-3 planner/cache trace and a full
game rerun were intentionally left for the next session.

## Next-session starting point

1. Run the dynamic object-3 trace with `REPIU_EXECUTION_BACKEND=dynamic`,
   `REPIU_LAUNCHER=0`, `REPIU_AOT_PLAN_TRACE=0x01100022`, and
   `REPIU_AOT_DYNAMIC_CONTAINS=0x01100022`.
2. Verify that `0x01100022` reports `BC0020`, length 3, and 16-bit mode, and
   that `41BF0020FB8D` is absent.
3. Implement a general 16-bit lowering or HLE dispatcher for the first
   fail-closed instruction. `MOV SP`, 16-bit stack push/pop, `PUSH CS`,
   `MOV SS`, `AND/LEA`, and far return require shared stack width/base/limit
   semantics.
4. Decide whether to add an explicit mode parameter to the 16-bit
   classifier/lowerer APIs; only proven lowerings should be admitted to the
   x64 cache.
5. After normal screen/input/exit and no-coredump behavior is verified, remove
   temporary diagnostics.

## Git state

The checkpoint is preserved on
`work/20260913-678-linux-x64-coredump-investigation`. No merge into `main` was
requested, so `main` remains untouched and the next session should continue on
this branch.
