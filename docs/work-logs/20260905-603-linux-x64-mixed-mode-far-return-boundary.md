# 작업 로그 20260905-603 — Linux x64 혼합 모드 far return 경계

## 결과

object 3의 `66 CB`가 generic near `RET` resolver로 들어가며 wrapper의 기존
`0x000000FF` stack word를 잘못된 반환 source로 만들던 경로를 분리했습니다.
새 `kFarReturn`은 현재 selector/stack descriptor ABI가 없으므로 Linux x64에서
`CC` fail-closed boundary로 멈춥니다.

## 수행 내용

* `AotInstructionKind::kFarReturn`과 `far_return_count`를 추가했습니다.
* Zydis far return metadata를 near return과 분리했습니다.
* long-mode/default emitter가 far return을 generic near-return thunk로 연결하지
  않고 HLE boundary로 내보내도록 했습니다.
* long-mode emission probe에 합성 `66 CB` boundary 검증을 추가했습니다.
* instruction census가 새 kind를 이름 붙이고 terminal boundary로 처리하도록
  갱신했습니다.

## 검증 증거

빌드:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
100% Built target repiu
100% Built target repiu_core_probe
```

core probe:

```text
long_mode_emission_far_return_boundary=true
long_mode_emission_counts=true,copied=1,lowered=4,refused=3
core_probe_total=22
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

probe-success runtime 환경:

```text
REPIU_DOS_INT_TRACE=1
REPIU_DPMI_1E7F_TRACE=1
REPIU_DPMI_1E7F_PROBE_SUCCESS=1
REPIU_LINUX_X64_RETURN_TRACE=1
```

핵심 관찰:

```text
[repiu-dpmi-1e7f] ... eip=0x010F010C ... esp=0x0158CC5C ... probe-success=1
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-guest-int3] #1 eip=0x01100042 ... esp=0x0158CC48
[repiu-fault] ... rip=0x1100040 eip=0x1100040 ... bytes=66 cb cc ...
```

기존 `source=0x000000FF` return resolver 실패 대신 guest `0x01100040`의
far-return boundary가 먼저 관찰되었습니다. `1E7Fh` private ABI나 실제
far-return frame 형식은 아직 미확정이며, `0xFF`를 유효 target으로 보정하거나
`ESP += 6`을 가정하지 않았습니다.

## 변경 파일

* `include/repiu/runtime/aot_translation_plan.h`
* `src/runtime/aot_translation_plan.cpp`
* `src/runtime/aot_code_cache.cpp`
* `src/tools/aot_probe/long_mode_emission_probe.cpp`
* `src/tools/instruction_census/main.cpp`
* `docs/analysis/linux-port-frontier.md`
* `docs/design/20260905-603-linux-x64-mixed-mode-far-return-boundary.md`
* `docs/work-orders/20260905-603-linux-x64-mixed-mode-far-return-boundary.md`

---

# Work log 20260905-603 — Linux x64 mixed-mode far-return boundary

## Result

The path in which object 3's `66 CB` entered the generic near-`RET` resolver and
caused the wrapper's existing `0x000000FF` stack word to become an invalid return
source is now separated. The new `kFarReturn` stops at a `CC` fail-closed
boundary on Linux x64 because the selector/stack descriptor ABI is not available.

## Work performed

* Added `AotInstructionKind::kFarReturn` and `far_return_count`.
* Separated Zydis far-return metadata from near returns.
* Kept far returns out of the generic near-return thunk in both emitters and
  emitted an HLE boundary instead.
* Added a synthetic `66 CB` boundary assertion to the long-mode emission probe.
* Updated instruction census naming and terminal-boundary handling.

## Verification evidence

The Linux x64 targets built successfully:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
100% Built target repiu
100% Built target repiu_core_probe
```

The core probe passed:

```text
long_mode_emission_far_return_boundary=true
long_mode_emission_counts=true,copied=1,lowered=4,refused=3
core_probe_total=22
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

With the existing probe-success environment, the runtime reached guest
`0x01100040` and reported `bytes=66 cb cc ...` after the guest `INT3` at
`0x01100042`. The former `source=0x000000FF` resolver failure was no longer the
first frontier on this path.

The private `1E7Fh` ABI and actual far-return frame format remain unresolved.
No correction treating `0xFF` as a valid target and no guessed `ESP += 6` behavior
was added.

## Changed files

The changed files are the five AOT/planner/probe sources listed in the Korean
section, the cumulative Linux frontier analysis, and the Task 603 design and
work-order documents.
