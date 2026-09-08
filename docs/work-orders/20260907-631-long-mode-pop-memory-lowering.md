# Task 631 작업 지시: long mode `POP r/m32` 메모리 형식 lowering

설계: [20260907-631](../design/20260907-631-long-mode-pop-memory-lowering.md)

## 한국어

### 범위

`src/runtime/aot_long_mode_compatibility.cpp`에서 `POP r/m32`의 메모리 형식에
long mode lowering을 추가합니다. 실행 동작을 고치는 작업입니다.

### 구현 단계

1. `POP r/m32` 메모리 형식의 지원 여부를 판정하는 helper를 익명 namespace에
   추가합니다. 다음을 모두 만족할 때만 참입니다.
   * `instruction.raw.prefix_count == 0`
   * `instruction.raw.modrm.mod != 3`
   * SIB가 있으면 `sib.base != 4`(ESP). SIB가 없으면 `rm != 4`.
   * `instruction.length`가 ModRM 이후 바이트를 담기에 일관될 것.
2. `HasStackSequenceLowering`의 `case 0x8FU`를 확장하여 기존 레지스터 형식과
   위 helper가 참인 메모리 형식을 함께 받습니다.
3. `WriteStackSequence`의 `case 0x8FU`에서 `mod == 3`이 아닐 때 다음을
   emit합니다.
   * `ExtendedMove(0x45, 0x8B, 0x37)` — `mov r14d, [r15]`
   * `AdjustGuestEsp(4)`
   * `0x67`, `0x44`, `0x89`, `reg` 필드를 `110`으로 바꾼 ModRM, 그리고 원본
     SIB와 변위 바이트
   * `instructions`는 3이 되어야 합니다.
4. 기존 레지스터 형식 경로와 다른 opcode의 sequence는 바꾸지 않습니다.
5. `src/tools/aot_probe/long_mode_compatibility_probe.cpp`에 항목을
   추가합니다.
   * `pop_mem_disp8` (`8F 47 14`): 3 명령, `MOV`, `LEA`, `MOV`.
   * `pop_mem_disp32` (`8F 05 78 56 34 12`): 3 명령, `MOV`, `LEA`, `MOV`.
   * emit된 바이트가 `67 44 89 77 14`로 끝나는지 바이트 단위로 확인.
   * `8F 44 24 04`(`POP [ESP+4]`)와 `66 8F 47 14`가 `kNone`으로 남는지 확인.
   * 새 결과를 `ProbeStackSequenceLowering`의 반환값에 포함합니다.

### 금지 사항

* ESP를 base로 쓰는 목적지를 받지 않습니다.
* prefix가 붙은 형식을 받지 않습니다.
* 해석기에 별도 `POP r/m32` 처리를 추가하지 않습니다. 이번 수정은 lowering
  하나입니다.
* 다른 opcode의 분류나 sequence 변경 금지.

### 검증

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — `core_probe_failures=0`과
   `long_mode_stack_sequences=true` 확인.
3. `REPIU_GUEST_WATCH=0x010F6056 ./build/linux_x64/repiu pumpit2a` —
   `event=fault`가 사라지는지 확인.
4. `./build/linux_x64/repiu pumpit2a` — `0x010F6062` fault가 사라지는지 확인.
5. 다음 정지 지점을 기록합니다.

### 문서

* `docs/analysis/linux-port-frontier.md`에 Task 631 절을 추가하고, 처리기가
  없는 경계 명령을 guest 주소에서 단일 실행하는 일반적 위험을 함께 남깁니다.
* `docs/work-logs/20260907-631-long-mode-pop-memory-lowering.md`를 씁니다.

## English

### Scope

Add a long-mode lowering for the memory form of `POP r/m32` in
`src/runtime/aot_long_mode_compatibility.cpp`. This changes execution behavior.

### Implementation steps

1. Add a helper in the anonymous namespace deciding whether a `POP r/m32`
   memory form is supported. It is true only when all of these hold:
   * `instruction.raw.prefix_count == 0`
   * `instruction.raw.modrm.mod != 3`
   * when a SIB is present, `sib.base != 4` (ESP); otherwise `rm != 4`
   * `instruction.length` is consistent with the bytes after the ModRM
2. Extend `case 0x8FU` in `HasStackSequenceLowering` to accept both the existing
   register form and a memory form the helper approves.
3. In `case 0x8FU` of `WriteStackSequence`, emit this when `mod != 3`:
   * `ExtendedMove(0x45, 0x8B, 0x37)` — `mov r14d, [r15]`
   * `AdjustGuestEsp(4)`
   * `0x67`, `0x44`, `0x89`, the ModRM with its `reg` field replaced by `110`,
     then the original SIB and displacement bytes
   * `instructions` must be 3.
4. Leave the register-form path and every other opcode's sequence unchanged.
5. Add items to
   `src/tools/aot_probe/long_mode_compatibility_probe.cpp`.
   * `pop_mem_disp8` (`8F 47 14`): 3 instructions, `MOV`, `LEA`, `MOV`.
   * `pop_mem_disp32` (`8F 05 78 56 34 12`): 3 instructions, `MOV`, `LEA`,
     `MOV`.
   * A byte check that the emitted bytes end with `67 44 89 77 14`.
   * `8F 44 24 04` (`POP [ESP+4]`) and `66 8F 47 14` still classify as `kNone`.
   * Include the new results in `ProbeStackSequenceLowering`'s return value.

### Prohibited

* Accepting a destination whose base register is ESP.
* Accepting any prefixed form.
* Adding a `POP r/m32` case to the interpreter. This change is one lowering.
* Changing the classification or sequence of any other opcode.

### Verification

1. `cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4`
2. `./build/linux_x64/repiu_core_probe` — confirm `core_probe_failures=0` and
   `long_mode_stack_sequences=true`.
3. `REPIU_GUEST_WATCH=0x010F6056 ./build/linux_x64/repiu pumpit2a` — confirm the
   `event=fault` is gone.
4. `./build/linux_x64/repiu pumpit2a` — confirm the `0x010F6062` fault is gone.
5. Record the next stopping point.

### Documentation

* Add a Task 631 section to `docs/analysis/linux-port-frontier.md`, recording
  alongside it the general hazard of single-stepping an unhandled boundary
  instruction at the guest address.
* Write `docs/work-logs/20260907-631-long-mode-pop-memory-lowering.md`.
