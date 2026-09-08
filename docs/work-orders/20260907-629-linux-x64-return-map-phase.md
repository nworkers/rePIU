# Task 629 작업 지시: Linux x64 return-time AOT guest map phase

설계: [20260907-629](../design/20260907-629-linux-x64-return-map-phase.md)

## 한국어

### 범위

`REPIU_LINUX_X64_RETURN_REG_TRACE`가 고른 return target에서 기존 AOT guest
map dump를 한 번 더 실행하는 진단만 추가합니다. 실행 정책은 바꾸지 않습니다.

### 구현 단계

1. `src/engine/execution/execution_trampoline.cpp`의
   `TraceLinuxX64ReturnRegisters` 끝에서 `TraceAotGuestMap`을 호출합니다.
   * `context`와 `context->aot_placement`가 모두 유효할 때만 호출합니다.
   * `phase` 인자는 `"return-trace"`를 사용합니다.
   * `runtime_base`는 `context->runtime_base`를 씁니다.
2. 새 환경 변수를 만들지 않습니다. `TraceAotGuestMap`은
   `REPIU_AOT_GUEST_MAP_TRACE`가 없으면 즉시 반환합니다.
3. 호출 위치는 Task 628의 stack tail 출력 뒤로 하여, 한 return에 대한 출력이
   register, stack tail, map 순서로 이어지게 합니다.

### 금지 사항

* fault handler에서 map을 dump하지 않습니다.
* return target 보정, stack slot 수정, resolver 결과 변경 금지.
* 환경 변수가 없을 때의 실행 경로와 출력 변경 금지.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0` 확인.
3. 실패 return 시점:

```text
REPIU_AOT_GUEST_MAP_TRACE=0x2A065,0x2A072,0x2A07D,0x2A082,0xF1A80 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64/repiu pumpit2a
```

4. 직전 정상 return 시점: 같은 명령에서
   `REPIU_LINUX_X64_RETURN_REG_TRACE=0x0102A082`.
5. 두 출력의 entry 수, cache 주소, emitted bytes, fixup을 비교.
6. 진단 변수를 뺀 실행이 같은 fault를 재현하는지 확인.

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 629 절을 추가합니다.
* `docs/work-logs/20260907-629-linux-x64-return-map-phase.md`를 씁니다.

## English

### Scope

Add diagnostics only: run the existing AOT guest map dump once more at the
return target selected by `REPIU_LINUX_X64_RETURN_REG_TRACE`. Do not change
execution policy.

### Implementation steps

1. Call `TraceAotGuestMap` at the end of `TraceLinuxX64ReturnRegisters` in
   `src/engine/execution/execution_trampoline.cpp`.
   * Call it only when `context` and `context->aot_placement` are both valid.
   * Pass `"return-trace"` as the `phase` argument.
   * Use `context->runtime_base` as the runtime base.
2. Add no new environment variable; `TraceAotGuestMap` already returns
   immediately when `REPIU_AOT_GUEST_MAP_TRACE` is unset.
3. Place the call after Task 628's stack tail output so one return prints
   registers, then the stack tail, then the map.

### Prohibited

* Dumping the map from the fault handler.
* Repairing return targets, editing stack slots, or altering resolver results.
* Changing the execution path or output when the variables are unset.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — confirm `core_probe_failures=0`.
3. At the failing return:

```text
REPIU_AOT_GUEST_MAP_TRACE=0x2A065,0x2A072,0x2A07D,0x2A082,0xF1A80 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64/repiu pumpit2a
```

4. At the correct return just before it: the same command with
   `REPIU_LINUX_X64_RETURN_REG_TRACE=0x0102A082`.
5. Compare entry counts, cache addresses, emitted bytes, and fixups.
6. Confirm a run without the diagnostic variables reproduces the same fault.

### Documentation

* Add a Task 629 section to `docs/analysis/linux-port-frontier.md`.
* Write `docs/work-logs/20260907-629-linux-x64-return-map-phase.md`.
