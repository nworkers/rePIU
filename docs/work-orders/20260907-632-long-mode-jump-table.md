# Task 632 작업 지시: long mode jump table slot

설계: [20260907-632](../design/20260907-632-long-mode-jump-table.md)

## 한국어

### 범위

long mode emit 경로에 `AotInstructionKind::kJumpTable` 슬롯을 추가합니다.
i386 경로와 다른 명령 종류는 바꾸지 않습니다.

### 구현 단계

1. `include/repiu/runtime/aot_code_cache.h`
   * `AotCodeCacheImage`에 `std::uint32_t long_mode_jump_table_count = 0;`을
     추가합니다.
   * `LongModeJumpTableEmittable(const AotInstructionRecord&)`를 선언합니다.
2. `src/runtime/aot_code_cache.cpp`
   * `LowerLongModeIndirectTargetLoad`를 바이트 구간과 기대 `/reg`를 받는
     내부 함수로 일반화하고, 기존 호출자는 `/2`를 넘기게 합니다. 재작성
     규칙의 사본을 만들지 않습니다.
   * `LowerLongModeJumpTableTargetLoad`를 추가합니다. 선행 `2E` 하나를
     건너뛰고 `/4`를 기대하며, `mod == 0`, `rm == 100`, SIB scale `4`,
     base `101`, index가 `table_index_register`이고 `4`가 아님을 확인합니다.
   * `EmitLongModeJumpTable`을 추가합니다. target load, producer tag,
     `movabs r12`, `jmp r12` 순서로 emit하고 `long_mode_jump_table_count`를
     올립니다. return 주소는 push하지 않습니다.
   * `LongModeJumpTableEmittable`을 추가합니다.
   * long mode emit 체인에 `kJumpTable` 절을 추가합니다.
3. `src/tools/aot_probe/linux_x64_guest_register_probe.cpp`
   * `ProbeJumpTable()`을 추가합니다. 표를 데이터 영역에 두고 `EBX=1`로
     실행하여 다음을 확인합니다.
     * resolver의 첫 질문이 표의 두 번째 항목일 것
     * resolver 호출 횟수가 1일 것
     * `EAX`가 그 대상 블록의 표식일 것
     * `observed_r15`가 진입 ESP와 같을 것
   * `ProbeJumpTableRefusals()`를 추가합니다. 레지스터 형식, `/2`,
     ESP index, `2E` 아닌 prefix가 모두 거부되는지 확인합니다.
   * `RunLinuxX64GuestRegisterProbe` 집계에 두 항목을 넣습니다.

### 금지 사항

* i386 `EmitJumpTableSlot` 경로 변경 금지.
* `2E` 외의 segment override를 받지 않습니다.
* ESP를 index로 쓰는 형식을 받지 않습니다.
* 처리기 없는 경계의 native 단일 실행 정책은 이번 작업에서 바꾸지 않습니다.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0`,
   `guest_jump_table=true`, `guest_jump_table_refusals=true` 확인.
3. `REPIU_GUEST_WATCH=0x0105547D ./build/linux_x64/repiu pumpit2a` —
   `event=fault`가 사라지는지 확인.
4. `./build/linux_x64/repiu pumpit2a` — `0x0105547D` fault가 사라지는지 확인.
5. 다음 정지 지점을 기록합니다.

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 632 절을 추가합니다.
* `docs/work-logs/20260907-632-long-mode-jump-table.md`를 씁니다.
* `ARCHITECTURE.md`의 AOT emission 절에 long mode jump table 규칙을
  반영합니다.

## English

### Scope

Add an `AotInstructionKind::kJumpTable` slot to the long-mode emit path. Do not
change the i386 path or any other instruction kind.

### Implementation steps

1. `include/repiu/runtime/aot_code_cache.h`
   * Add `std::uint32_t long_mode_jump_table_count = 0;` to
     `AotCodeCacheImage`.
   * Declare `LongModeJumpTableEmittable(const AotInstructionRecord&)`.
2. `src/runtime/aot_code_cache.cpp`
   * Generalize `LowerLongModeIndirectTargetLoad` into an internal function
     taking a byte range and the `/reg` extension it expects, and have the
     existing caller pass `/2`. Do not copy the rewrite.
   * Add `LowerLongModeJumpTableTargetLoad`: skip one leading `2E`, expect
     `/4`, and require `mod == 0`, `rm == 100`, SIB scale `4`, base `101`, and
     an index equal to `table_index_register` that is not `4`.
   * Add `EmitLongModeJumpTable`: emit the target load, the producer tag,
     `movabs r12`, and `jmp r12`, and raise `long_mode_jump_table_count`. Push
     no return address.
   * Add `LongModeJumpTableEmittable`.
   * Add a `kJumpTable` clause to the long-mode emit chain.
3. `src/tools/aot_probe/linux_x64_guest_register_probe.cpp`
   * Add `ProbeJumpTable()`: place a table in the data region, run with
     `EBX=1`, and check that
     * the resolver's first question is the table's second entry,
     * the resolver was called once,
     * `EAX` carries that target block's marker, and
     * `observed_r15` equals the entry ESP.
   * Add `ProbeJumpTableRefusals()`: the register form, `/2`, an ESP index, and
     a prefix other than `2E` are all refused.
   * Include both in the `RunLinuxX64GuestRegisterProbe` aggregate.

### Prohibited

* Changing the i386 `EmitJumpTableSlot` path.
* Accepting any segment override other than `2E`.
* Accepting ESP as the index register.
* Changing the native single-step policy for unhandled boundaries.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — confirm `core_probe_failures=0`,
   `guest_jump_table=true`, and `guest_jump_table_refusals=true`.
3. `REPIU_GUEST_WATCH=0x0105547D ./build/linux_x64/repiu pumpit2a` — confirm the
   `event=fault` is gone.
4. `./build/linux_x64/repiu pumpit2a` — confirm the `0x0105547D` fault is gone.
5. Record the next stopping point.

### Documentation

* Add a Task 632 section to `docs/analysis/linux-port-frontier.md`.
* Write `docs/work-logs/20260907-632-long-mode-jump-table.md`.
* Reflect the long-mode jump-table rule in the AOT emission section of
  `ARCHITECTURE.md`.
