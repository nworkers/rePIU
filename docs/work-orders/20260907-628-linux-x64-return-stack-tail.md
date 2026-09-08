# Task 628 작업 지시: Linux x64 return stack writer tail

설계: [20260907-628](../design/20260907-628-linux-x64-return-stack-tail.md)

## 한국어

### 범위

`REPIU_LINUX_X64_RETURN_REG_TRACE`가 선택한 return target에 대해, x64
dispatch frame의 stack write ring 최근 기록을 순서대로 출력하는 진단만
추가합니다. 실행 정책은 바꾸지 않습니다.

### 구현 단계

1. `src/engine/execution/execution_trampoline.cpp`에
   `LinuxX64ReturnStackTailCount()`를 추가합니다.
   * `REPIU_LINUX_X64_RETURN_STACK_TAIL`을 base 0으로 읽습니다.
   * 값이 없거나 파싱에 실패하면 `0`을 돌려줍니다.
   * `REPIU_LINUX_X64_FRAME_STACK_TRACE_CAPACITY`로 상한을 둡니다.
2. `TraceLinuxX64ReturnRegisters`에서 register line 출력 뒤에 tail을
   출력합니다.
   * `frame.stack_trace_sequence`를 다음에 쓸 sequence로 보고, 마지막
     기록은 `sequence - 1`입니다.
   * `count`와 `sequence` 중 작은 값만큼 오래된 것부터 출력합니다.
   * ring index는 `sequence & (capacity - 1)`로 계산합니다.
   * `site == 0`인 미기록 slot은 건너뜁니다.
3. writer 종류는 기존 규칙을 유지합니다. `fallthrough != 0`이면
   `direct-call`, 아니면 `guest-push`입니다.
4. 출력 태그는 `[repiu-x64-return-stack-tail]`을 사용하여 기존
   `[repiu-x64-return-stack]` 줄과 구분합니다.

### 금지 사항

* return target 보정, stack slot 수정, resolver 결과 변경 금지.
* 환경 변수가 없을 때의 실행 경로와 출력 변경 금지.
* guest stack write emitter의 기록 형식 변경 금지.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0` 확인.
3. `REPIU_LINUX_X64_STACK_TRACE=1 REPIU_LINUX_X64_RETURN_TRACE=1`
   `REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A`
   `REPIU_LINUX_X64_RETURN_STACK_TAIL=48 ./build/linux_x64/repiu pumpit2a`
4. tail이 정상 return과 실패 return 사이의 write를 보여주는지 확인.
5. tail 변수를 뺀 실행이 같은 fault를 재현하는지 확인.

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 628 절을 추가합니다.
* `docs/work-logs/20260907-628-linux-x64-return-stack-tail.md`를 씁니다.

## English

### Scope

Add diagnostics only: for the return target selected by
`REPIU_LINUX_X64_RETURN_REG_TRACE`, print the most recent stack-write ring
records from the x64 dispatch frame in write order. Do not change execution
policy.

### Implementation steps

1. Add `LinuxX64ReturnStackTailCount()` in
   `src/engine/execution/execution_trampoline.cpp`.
   * Read `REPIU_LINUX_X64_RETURN_STACK_TAIL` with base 0.
   * Return `0` when absent or unparsable.
   * Clamp to `REPIU_LINUX_X64_FRAME_STACK_TRACE_CAPACITY`.
2. Print the tail in `TraceLinuxX64ReturnRegisters` after the register line.
   * Treat `frame.stack_trace_sequence` as the next sequence to write, so the
     newest record is `sequence - 1`.
   * Print the smaller of `count` and `sequence` records, oldest first.
   * Compute the ring index as `sequence & (capacity - 1)`.
   * Skip unwritten slots where `site == 0`.
3. Keep the existing writer rule: `fallthrough != 0` is `direct-call`,
   otherwise `guest-push`.
4. Use the tag `[repiu-x64-return-stack-tail]` so the new lines stay distinct
   from the existing `[repiu-x64-return-stack]` output.

### Prohibited

* Repairing return targets, editing stack slots, or altering resolver results.
* Changing the execution path or output when the variable is unset.
* Changing the guest stack-write emitter's record format.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — confirm `core_probe_failures=0`.
3. `REPIU_LINUX_X64_STACK_TRACE=1 REPIU_LINUX_X64_RETURN_TRACE=1`
   `REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A`
   `REPIU_LINUX_X64_RETURN_STACK_TAIL=48 ./build/linux_x64/repiu pumpit2a`
4. Confirm the tail shows the writes between the correct and failing returns.
5. Confirm a run without the new variable reproduces the same fault.

### Documentation

* Add a Task 628 section to `docs/analysis/linux-port-frontier.md`.
* Write `docs/work-logs/20260907-628-linux-x64-return-stack-tail.md`.
