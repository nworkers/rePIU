# Task 633 작업 지시: census의 jump table 규칙과 `agrees=` 복구

설계: [20260907-633](../design/20260907-633-census-jump-table-agreement.md)

## 한국어

### 범위

`src/tools/instruction_census/main.cpp`만 바꿉니다. emitter와 실행 동작은
바꾸지 않습니다.

### 구현 단계

1. `LongModeTally`에 `std::uint64_t jump_tables = 0;`을 추가합니다.
2. `RecordIsEmitted`의 switch에 `kJumpTable` 분기를 넣고
   `repiu::runtime::LongModeJumpTableEmittable(record)`를 돌려줍니다.
3. `kIndirectExit` 누적 바로 뒤에 `kJumpTable` 누적을 추가합니다.
4. `emitted` 합계에 `long_mode.jump_tables`를 더합니다.
5. `indirect calls` 줄 뒤에 `jump tables` 줄을 출력합니다.
6. `agrees` 비교에 jump table 카운터를 추가하고, emitter 카운터 줄에
   `tables=`를 출력합니다.

### 금지 사항

* emit 규칙을 census 안에서 재구현하지 않습니다. 반드시 emitter에게 묻습니다.
* emitter, 실행 경로, 다른 도구 변경 금지.

### 검증

1. `cmake --build build/linux_x64 --target repiu_instruction_census repiu_core_probe -j 4`
2. `./build/linux_x64/repiu_instruction_census build/runtime_mounts/pumpit2a/PIU/PIU.EXE`
   * `agrees=true`
   * `refused` 740, `kJumpTable` 행 없음
   * `jump tables 21`
3. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0`

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 633 절을 추가하고 boundary
  census 수치를 정책 판단의 입력으로 남깁니다.
* `docs/work-logs/20260907-633-census-jump-table-agreement.md`를 씁니다.

## English

### Scope

Change only `src/tools/instruction_census/main.cpp`. Do not change the emitter
or execution behavior.

### Implementation steps

1. Add `std::uint64_t jump_tables = 0;` to `LongModeTally`.
2. Add a `kJumpTable` arm to the `RecordIsEmitted` switch returning
   `repiu::runtime::LongModeJumpTableEmittable(record)`.
3. Accumulate `kJumpTable` immediately after the `kIndirectExit` accumulation.
4. Add `long_mode.jump_tables` to the `emitted` total.
5. Print a `jump tables` line after the `indirect calls` line.
6. Add the jump-table counter to the `agrees` comparison and print `tables=` in
   the emitter-counter line.

### Prohibited

* Reimplementing an emission rule inside the census. It must ask the emitter.
* Changing the emitter, the execution path, or any other tool.

### Verification

1. `cmake --build build/linux_x64 --target repiu_instruction_census repiu_core_probe -j 4`
2. `./build/linux_x64/repiu_instruction_census build/runtime_mounts/pumpit2a/PIU/PIU.EXE`
   * `agrees=true`
   * `refused` 740 and no `kJumpTable` row
   * `jump tables 21`
3. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0`

### Documentation

* Add a Task 633 section to `docs/analysis/linux-port-frontier.md` recording the
  boundary census as the input to the policy decision.
* Write `docs/work-logs/20260907-633-census-jump-table-agreement.md`.
