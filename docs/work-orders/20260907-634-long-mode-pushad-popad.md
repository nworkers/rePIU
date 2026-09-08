# Task 634 작업 지시: long mode `PUSHAD` / `POPAD`

설계: [20260907-634](../design/20260907-634-long-mode-pushad-popad.md)

## 한국어

### 범위

`PUSHAD`(`0x60`)와 `POPAD`(`0x61`)에 long mode stack sequence를 추가합니다.
다른 opcode의 분류나 sequence는 바꾸지 않습니다.

### 구현 단계

1. `include/repiu/runtime/aot_long_mode_compatibility.h`
   * `kMaxLoweredBytes`를 24에서 48로 올리고, 이유를 주석에 남깁니다.
2. `src/runtime/aot_long_mode_compatibility.cpp`
   * `SequenceWriter`에 변위를 갖는 두 helper를 추가합니다.
     * `StoreGuestRegisterAt(reg, disp8)` — `mov [r15+disp8], r32`
     * `LoadGuestRegisterAt(reg, disp8)` — `mov r32, [r15+disp8]`
     * 그리고 `mov [r15+12], r14d`를 위한 변위 포함 확장 move.
   * `NeedsWidthReencode`에 `0x60`, `0x61`을 추가합니다.
   * `HasStackSequenceLowering`에 `0x60`, `0x61`을 `length == 1U` 조건으로
     추가합니다.
   * `WriteStackSequence`에 두 sequence를 설계대로 emit합니다. `PUSHAD`는
     10개 명령, `POPAD`는 8개 명령입니다.
3. `src/tools/aot_probe/long_mode_compatibility_probe.cpp`
   * stack sequence 표에 `pushad`와 `popad` 항목을 추가합니다.
   * `PUSHAD`의 `+12` 슬롯이 진입 ESP를 쓰는지 바이트로 확인하는 항목을
     추가하고, 결과를 `ProbeStackSequenceLowering` 반환값에 넣습니다.

### 금지 사항

* `0x66` prefix가 붙은 `PUSHA`/`POPA`(16비트 형식)를 받지 않습니다.
* `PUSHAD`가 진입 ESP를 `lea` 이후에 읽지 않습니다.
* `POPAD`가 ESP 슬롯을 복원하지 않습니다.
* 다른 opcode의 sequence, fixup 규칙, 실행 정책 변경 금지.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu repiu_instruction_census -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0`,
   `long_mode_stack_sequences=true` 확인.
3. `./build/linux_x64/repiu_instruction_census build/runtime_mounts/pumpit2a/PIU/PIU.EXE`
   — `pushad`/`popad` 행이 사라지고 `agrees=true` 유지 확인.
4. `./build/linux_x64/repiu pumpit2a` — `0x010EFE38` SIGILL이 사라지는지 확인.
5. 다음 정지 지점을 기록합니다.

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 634 절을 추가합니다.
* `docs/work-logs/20260907-634-long-mode-pushad-popad.md`를 씁니다.

## English

### Scope

Add long-mode stack sequences for `PUSHAD` (`0x60`) and `POPAD` (`0x61`). Do not
change the classification or sequence of any other opcode.

### Implementation steps

1. `include/repiu/runtime/aot_long_mode_compatibility.h`
   * Raise `kMaxLoweredBytes` from 24 to 48 and record why in the comment.
2. `src/runtime/aot_long_mode_compatibility.cpp`
   * Add two displacement-carrying helpers to `SequenceWriter`:
     * `StoreGuestRegisterAt(reg, disp8)` — `mov [r15+disp8], r32`
     * `LoadGuestRegisterAt(reg, disp8)` — `mov r32, [r15+disp8]`
     * and a displacement-carrying extended move for `mov [r15+12], r14d`.
   * Add `0x60` and `0x61` to `NeedsWidthReencode`.
   * Add `0x60` and `0x61` to `HasStackSequenceLowering` under `length == 1U`.
   * Emit both sequences in `WriteStackSequence` as the design specifies: ten
     instructions for `PUSHAD` and eight for `POPAD`.
3. `src/tools/aot_probe/long_mode_compatibility_probe.cpp`
   * Add `pushad` and `popad` entries to the stack-sequence table.
   * Add a byte check that `PUSHAD`'s `+12` slot is written from the entry ESP,
     and include the result in `ProbeStackSequenceLowering`'s return value.

### Prohibited

* Accepting the 16-bit `PUSHA` / `POPA` forms carrying a `0x66` prefix.
* Reading the entry ESP after the `lea` in `PUSHAD`.
* Restoring the ESP slot in `POPAD`.
* Changing any other opcode's sequence, the fixup rules, or execution policy.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu repiu_instruction_census -j 4`
2. `./build/linux_x64/repiu_core_probe` — confirm `core_probe_failures=0` and
   `long_mode_stack_sequences=true`.
3. `./build/linux_x64/repiu_instruction_census build/runtime_mounts/pumpit2a/PIU/PIU.EXE`
   — confirm the `pushad` and `popad` rows are gone and `agrees=true` holds.
4. `./build/linux_x64/repiu pumpit2a` — confirm the `0x010EFE38` SIGILL is gone.
5. Record the next stopping point.

### Documentation

* Add a Task 634 section to `docs/analysis/linux-port-frontier.md`.
* Write `docs/work-logs/20260907-634-long-mode-pushad-popad.md`.
