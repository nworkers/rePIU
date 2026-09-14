# 작업 로그 20260915-691 — Linux x64 mode16 segment push HLE boundary

> Task 692 정정: 아래 trace의 0110002E는 Task 690과 동일한 중단 주소입니다.
> 검증된 변경은 planner의 hle=0→1이며 실행 경계 진전이나 올바른 mode16
> PUSH semantics를 입증하지 않습니다. core-dump 메시지는 여전히 발생했습니다.
>
> Task 692 correction: 0110002E below is the same stop as Task 690. The confirmed
> change is planner hle=0 to 1, not runtime advancement or correct mode16 PUSH
> semantics. The core-dump termination message still occurred.

## 결과 요약

기존 `HandleSegmentPushInstruction`이 이미 지원하는 `PUSH CS`를 mode16 AOT
planner가 기존 HLE dispatch로 연결하도록 수정했습니다. mode32 native
segment-push 정책이나 selector/stack semantics는 변경하지 않았습니다.

## 구현 및 검증

* mode16, prefix-free, segment-register PUSH를 `kHleBoundary`로 계획하는
  `IsMode16SegmentPushHle` predicate를 추가했습니다.
* synthetic mode16 `0E` planner probe가 `boundary=1`로 통과했습니다.
* Linux x64 `repiu_core_probe`와 `repiu` 빌드가 성공했습니다.

```text
long_mode_16bit_segment_push_hle=true,boundary=1
core_probe_failures=0
core_probe_all=true
```

실제 runtime plan trace에서 `0E`가 `hle=1`로 기록되었고, 기존 segment
HLE를 통과해 다음 mode16 boundary에서 중단했습니다.

```text
[repiu-aot-plan-trace] guest=0x0110002D bytes=0E length=1 ... code_mode=16 ... hle=1
[repiu-fault] unhandled signal=0x5 ... eip=0x0110002E
```

`0x0110002E: 50`은 mode16 `PUSH AX`이며 다음 작업 대상입니다. 실행은
`ulimit -c 0`으로 제한했고 timeout 종료 메시지는 core-dump라고 표시했지만
`build` 아래에는 core 파일이 생성되지 않았습니다.

## 결론 및 다음 작업

이번 변경은 새 segment 예외처리나 주소 우회가 아니라, code mode와 decoded
segment-register operand class를 기존 HLE boundary에 연결한 것입니다. 다음
세션에서는 `PUSH AX`의 guest-stack semantics를 기존 general stack lowering과
어떻게 연결할지 설계해야 합니다.

---

# Work Log 20260915-691 — Linux x64 mode16 segment push HLE boundary

## Summary

Connected the existing `HandleSegmentPushInstruction` support for `PUSH CS` to
the mode16 AOT planner's existing HLE dispatch. No mode32 native segment-push
policy or selector/stack semantics changed.

## Implementation and verification

* Added `IsMode16SegmentPushHle` to plan prefix-free mode16 segment-register
  PUSH instructions as `kHleBoundary`.
* The synthetic mode16 `0E` planner probe passes with `boundary=1`.
* The Linux x64 `repiu_core_probe` and `repiu` targets build successfully.

```text
long_mode_16bit_segment_push_hle=true,boundary=1
core_probe_failures=0
core_probe_all=true
```

The live runtime plan trace records `0E` with `hle=1`, and execution passes the
existing segment HLE before stopping at the next mode16 boundary:

```text
[repiu-aot-plan-trace] guest=0x0110002D bytes=0E length=1 ... code_mode=16 ... hle=1
[repiu-fault] unhandled signal=0x5 ... eip=0x0110002E
```

`0x0110002E: 50` is mode16 `PUSH AX` and is the next task frontier. The run
used `ulimit -c 0`; although timeout reported a core-dump termination message,
no core file was created under `build`.

## Conclusion and next task

This change connects a code-mode and decoded segment-operand class to the
existing HLE boundary; it does not add a new segment exception or address
workaround. The next session should design how mode16 `PUSH AX` connects to the
existing general guest-stack lowering.
