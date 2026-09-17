# 작업 로그 20260914-680 — Linux x64 16-bit 스택 레지스터 lowering

## 결과

Task 679의 object별 code-mode 정보를 Linux x64 long-mode AOT lowering까지
연결했습니다. 첫 번째로 증명된 16-bit 명령인 `BC iw`를 일반적인 mode-aware
규칙으로 처리하고, 증명되지 않은 16-bit non-copy 명령이 기존 32-bit native
slot을 사용하지 않도록 닫았습니다.

## 변경 사항

* `ClassifyLongModeBytes`와 `LowerLongModeBytes`에 기본값이 `k32`인 guest
  code-mode 인자를 추가했습니다.
* `LEGACY_16`에서 무접두 `BC iw`를 길이 3의 `MOV SP, imm16`으로 인정하고
  `66 41 BF iw` (`MOV R15W, iw`)로 lowering하도록 추가했습니다.
* x64 cache emitter가 record mode를 classifier/lowerer에 전달하며, 16-bit
  non-copy record를 32-bit branch/return/selector slot에서 제외하고 INT3
  HLE boundary로 닫도록 했습니다.
* planner trace와 dynamic append trace에 `code_mode`를 추가했습니다.
* compatibility, lowering, emission probe에 mode16 bytes·실행·boundary
  regression을 추가했습니다.
* 설계·작업 지시·ARCHITECTURE·Linux port analysis를 갱신했습니다.

## 검증

`git diff --check`를 실행했고 오류는 없었습니다. 다만 현재 작업 환경에는
Windows PATH의 `cmake`가 없고, `wsl.exe -d Ubuntu-24.04`가
`Wsl/Service/E_ACCESSDENIED`를 반환했습니다. 따라서 Linux x64 build, core
probe, mode16 실행 probe, object 3 runtime smoke는 이번 세션에서 실행하지
못했으며 다음 Linux/WSL 실행에서 확인해야 합니다.

정적 점검으로 새 API의 선언·정의·호출부와 `LongModeLowering` 사용부를
대조했으며, 기존 호출자는 optional mode 기본값으로 기존 동작을 유지합니다.

## 남은 범위

16-bit `PUSH/POP`, `MOV SS`, far return, 16-bit address-size, 일반 HLE stack
ABI는 아직 구현하지 않았습니다. 다음 실행에서 object 3의
`0x01100022`가 `BC0020`, length 3, mode16으로 기록되고 이전의
`41BF0020FB8D`가 사라지는지 확인해야 합니다.

## Git 상태

작업은 `work/20260913-678-linux-x64-coredump-investigation` 브랜치에서
진행했으며 `main`에는 머지하지 않았습니다. 이 작업 로그와 구현을 하나의
커밋으로 남깁니다.

---

# Work Log 20260914-680 — Linux x64 16-bit stack-register lowering

## Result

Task 679's per-object code-mode metadata now reaches Linux x64 long-mode AOT
lowering. The first proven 16-bit instruction, `BC iw`, uses a general
mode-aware rule, while unproven 16-bit non-copy instructions are kept out of
the existing 32-bit native slots.

## Changes

* Added an optional guest code-mode argument to `ClassifyLongModeBytes` and
  `LowerLongModeBytes`, defaulting to `k32`.
* In `LEGACY_16`, admitted the unprefixed three-byte `BC iw` form as
  `MOV SP, imm16` and lowered it to `66 41 BF iw` (`MOV R15W, iw`).
* Passed record mode through the x64 cache emitter, excluding 16-bit non-copy
  records from 32-bit branch/return/selector slots and closing them with an
  INT3 HLE boundary.
* Added `code_mode` to planner and dynamic-append traces.
* Added mode16 byte, execution, and boundary regressions to the compatibility,
  lowering, and emission probes.
* Updated the design, work order, ARCHITECTURE, and Linux-port analysis.

## Verification

`git diff --check` completed without errors. The current environment has no
Windows-PATH `cmake`, and `wsl.exe -d Ubuntu-24.04` returns
`Wsl/Service/E_ACCESSDENIED`. Therefore the Linux x64 build, core probe, mode16
execution probe, and object-3 runtime smoke could not run in this session and
remain to be confirmed by the next Linux/WSL execution.

Static inspection compared the new API declarations, definitions, call sites,
and `LongModeLowering` uses. Existing callers retain their behavior through the
optional default mode.

## Remaining scope

16-bit `PUSH/POP`, `MOV SS`, far return, 16-bit address-size, and a general HLE
stack ABI remain unimplemented. The next run must confirm that object 3's
`0x01100022` reports `BC0020`, length 3, and mode16, and that the old
`41BF0020FB8D` sequence is absent.

## Git state

Work was performed on `work/20260913-678-linux-x64-coredump-investigation`; no
merge into `main` was requested. This work log and implementation are recorded
in one commit.

---

## 검증 추가 기록 — WSL 재빌드 및 runtime smoke

사용자 요청에 따라 WSL 접근 복구 후 다음 검증을 수행했습니다.

* `cmake --build build/linux_x64 --target repiu repiu_core_probe -j2`
  **통과**.
* `repiu_core_probe` 실행 결과 `core_probe_total=27`,
  `core_probe_failures=0`, `core_probe_all=true`.
* mode16 관련 결과는 `long_mode_16bit_stack_pointer_immediate=true`,
  `long_mode_emission_16bit_mode=true,planner_mode=true`,
  `long_mode_lowering_16bit_stack_pointer=true`였습니다.
* `REPIU_EXECUTION_BACKEND=dynamic` 짧은 smoke는 coredump 없이
  `reason=timeout`, `recovered=1`, `stopped=1`, `failure=0`으로 cleanup됐습니다.
* 모든 dynamic request trace도 확인했지만 관찰된 요청은 `0x010xxxxx`
  범위였고 object 3의 `0x01100022` 요청은 발생하지 않았습니다. 따라서
  실제 object 3 trace에서 이전 `41BF0020FB8D`가 사라졌다는 결론은 아직
  내리지 않습니다.

### Verification addendum — WSL rebuild and runtime smoke

At the user's request, after WSL access was restored:

* `cmake --build build/linux_x64 --target repiu repiu_core_probe -j2`
  **passed**.
* `repiu_core_probe` reported `core_probe_total=27`,
  `core_probe_failures=0`, and `core_probe_all=true`.
* Mode16 results were `long_mode_16bit_stack_pointer_immediate=true`,
  `long_mode_emission_16bit_mode=true,planner_mode=true`, and
  `long_mode_lowering_16bit_stack_pointer=true`.
* A short `REPIU_EXECUTION_BACKEND=dynamic` smoke ended without a coredump;
  shutdown reported `reason=timeout`, `recovered=1`, `stopped=1`, and
  `failure=0`.
* The all-request dynamic trace was also checked. Observed requests stayed in
  the `0x010xxxxx` range; no request for object 3 address `0x01100022` occurred.
  Therefore the real object-3 dynamic image has not yet confirmed removal of
  the old `41BF0020FB8D` sequence.
