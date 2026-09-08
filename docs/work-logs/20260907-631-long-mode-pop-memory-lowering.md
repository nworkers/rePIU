# Task 631 작업 로그: long mode `POP r/m32` 메모리 형식 lowering

설계: [20260907-631](../design/20260907-631-long-mode-pop-memory-lowering.md) ·
작업 지시: [20260907-631](../work-orders/20260907-631-long-mode-pop-memory-lowering.md) ·
분석: [linux-port-frontier 3.68](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`POP r/m32`의 메모리 형식에 long mode stack sequence lowering을 추가했습니다.
`mov r14d,[r15]`, `lea r15d,[r15+4]`, 그리고 guest의 ModRM·SIB·변위를 그대로
옮기고 `reg` 필드만 `110`으로 바꾼 `mov [<guest mem>], r14d` 세 명령입니다.
목적지가 ESP를 base로 쓰는 형식과 prefix가 붙은 형식은 fail-closed로
남겼습니다. 레지스터 형식 경로와 다른 opcode의 sequence는 그대로입니다.

`long_mode_compatibility` 코어 프로브에 lowering 항목 두 개, 바이트 단위
인코딩 확인, 거부 유지 확인을 추가했습니다.

### 검증

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
long_mode_stack_sequences=true,push_imm8_sign_extended=true,
control_flow_still_refused=true,pop_memory_store_encoding=true,
pop_memory_refusals_kept=true
long_mode_compatibility_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`REPIU_GUEST_WATCH=0x010F6056`은 이제 아무 event도 내지 않습니다.
`0x010F6062` fault는 사라졌고, 세 번 연속 실행이 모두 새 지점에서
멈췄습니다.

```text
[repiu-fault] unhandled signal=0xb rip=0x105547d eip=0x105547d access=0x0
  bytes=2e ff 24 9d 10 54 05 01 ...
```

### 확인한 사실

* 진입 프레임은 정상이었습니다. `PUSH ES`와 `PUSH DS`의 HLE stack 폭은 정확히
  4바이트이며, Task 630이 남긴 segment HLE 후보는 기각되었습니다.
* 원인은 `0x010F6056`의 `8F 47 14`였습니다. 메모리 형식에 lowering이 없어
  `INT3` 경계가 되었고, 해석기에도 처리가 없어 guest ESP가 오르지 않았습니다.
* x64에서 처리기 없는 경계 명령은 guest 주소에서 그대로 한 번 실행됩니다.
  32비트 명령을 64비트로 실행하는 것이므로 의미가 같은 명령에만 우연히
  안전합니다. 이번 건은 그 위험이 드러난 사례입니다.
* 다음 frontier는 `0x0105547D`의 `JMP CS:[EBX*4+0x01055410]`입니다. long mode
  경로에는 `kJumpTable` 슬롯이 없어 초기 map에서 이미 `CC` 한 바이트로
  emit되어 있습니다.

### 판단

Task 630이 남긴 stack 정렬 문제가 해결되었고 실행이 더 진행됩니다. 새 정지
지점은 이번 수정이 만든 것이 아니라 이전에는 도달하지 못했던 곳입니다.
경계 명령을 native로 단일 실행하는 일반적 위험은 이번 작업에서 고치지
않았고, 분석 문서에 남겼습니다.

### 다음 작업

`0x0105547D`의 jump table을 long mode에서 처리해야 합니다. 선택지는 long mode
`kJumpTable` 슬롯을 구현하는 것과, 처리기 없는 경계의 native 단일 실행을
fail-closed로 바꾸는 것입니다. 후자는 진행을 되돌리지만 지금의 조용한 오실행을
없앱니다. 두 선택지의 범위를 먼저 설계에서 비교해야 합니다.

## English

### Result

Added a long-mode stack sequence for the memory form of `POP r/m32`: three
instructions, `mov r14d,[r15]`, `lea r15d,[r15+4]`, and
`mov [<guest mem>], r14d` carrying the guest's ModRM, SIB, and displacement
unchanged with only the `reg` field replaced by `110`. Destinations addressed
through ESP and any prefixed form stay fail-closed. The register-form path and
every other opcode's sequence are unchanged.

The `long_mode_compatibility` core probe gained two lowering items, a byte-level
encoding check, and a check that the refusals are kept.

### Verification

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
long_mode_stack_sequences=true,push_imm8_sign_extended=true,
control_flow_still_refused=true,pop_memory_store_encoding=true,
pop_memory_refusals_kept=true
long_mode_compatibility_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`REPIU_GUEST_WATCH=0x010F6056` now reports no events. The `0x010F6062` fault is
gone, and three consecutive runs stopped at the same new point.

```text
[repiu-fault] unhandled signal=0xb rip=0x105547d eip=0x105547d access=0x0
  bytes=2e ff 24 9d 10 54 05 01 ...
```

### Facts established

* The entry frame was correct. The HLE stack width for `PUSH ES` and `PUSH DS`
  is exactly four bytes, so Task 630's segment-HLE candidate is rejected.
* The cause was `8F 47 14` at `0x010F6056`. The memory form had no lowering, so
  it became an `INT3` boundary, and with no interpreter case either, guest ESP
  was never raised.
* On x64 an unhandled boundary instruction is executed once at its guest
  address. That runs a 32-bit instruction as 64-bit code and is only
  accidentally safe when both modes agree. This was a case where they do not.
* The next frontier is `JMP CS:[EBX*4+0x01055410]` at `0x0105547D`. The
  long-mode path has no `kJumpTable` slot, so the initial map already holds a
  single `CC` byte for it.

### Assessment

The stack-alignment problem Task 630 left is solved and execution advances. The
new stopping point is not created by this change; it is somewhere the run could
not previously reach. The general hazard of single-stepping a boundary natively
is not fixed here and is recorded in the analysis.

### Next task

Handle the jump table at `0x0105547D` in long mode. The options are to implement
a long-mode `kJumpTable` slot, or to make the native single-step of an unhandled
boundary fail closed. The second gives up reach but removes the current silent
misexecution. Compare the scope of both in a design before choosing.
