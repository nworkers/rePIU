# Task 634 작업 로그: long mode `PUSHAD` / `POPAD`

설계: [20260907-634](../design/20260907-634-long-mode-pushad-popad.md) ·
작업 지시: [20260907-634](../work-orders/20260907-634-long-mode-pushad-popad.md) ·
분석: [linux-port-frontier 3.71](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`PUSHAD`(`0x60`)와 `POPAD`(`0x61`)에 long mode stack sequence를 추가했습니다.
`SequenceWriter`에 변위를 갖는 store/load helper와 스크래치 store를
추가했고, `kMaxLoweredBytes`를 24에서 48로 올렸습니다.

작업 지시와 달라진 점이 하나 있습니다. 지시는 `NeedsWidthReencode`에 두
opcode를 추가하라고 했지만, `ClassifyLongModeBytes`는 `IsInvalidInLongMode`를
먼저 검사하고 그 자리에서 `Refuse`하므로 그 경로에 도달하지 않았습니다.
대신 invalid 분기가 `HasStackSequenceLowering`을 함께 묻도록 했습니다.
divergence는 `kInvalidInLongMode`로 유지하고 lowering만 붙입니다. 같은
목록의 `PUSH ES`는 `kNone`으로 남아 기존 segment HLE가 처리합니다.

### 검증

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu repiu_instruction_census -j 4
long_mode_stack_sequences=true,push_imm8_sign_extended=true,
control_flow_still_refused=true,pop_memory_store_encoding=true,
pop_memory_refusals_kept=true,pushad_entry_esp=true
long_mode_invalid=true,refused=a/a
long_mode_divergence_reasons=true
long_mode_compatibility_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

census:

```text
  lowered             23565
  emittable           51128  (98.58%)
  refused             738  (1.42%)
  blocks complete     11118  (88.88%)
  agrees=true
```

`pushad`와 `popad` 행이 거부 목록에서 사라졌습니다.

guest 실행은 세 번 연속 같은 새 지점에서 멈췄습니다.

```text
[repiu-fault] unhandled signal=0xb rip=0x10f06d0 eip=0x10f06d0 access=0x9fdd
  bytes=65 80 38 6a 75 4d 8d 45 ff b6 01 65 8a 18 88 35
```

### 확인한 사실

* `0x60`은 invalid 목록에 있어 width 경로에 도달하지 못했습니다. 분류 순서를
  확인하지 않았다면 아무 효과 없는 변경이 될 뻔했습니다.
* 기존 `long_mode_invalid`와 `long_mode_divergence_reasons` 프로브는 그대로
  통과합니다. 앞의 것은 `compatibility != kIdenticalBytes`만, 뒤의 것은
  divergence만 보기 때문이며, 둘 다 이번 변경이 지켜야 할 성질입니다.
* `PUSHAD`는 진입 ESP를 `lea` 이전에 붙듭니다. 이후에 읽으면 32가 빠진 값이
  `+12`에 남고 아무것도 raise하지 않습니다. 프로브가 그 두 바이트와 store를
  바이트로 확인합니다.
* 다음 frontier는 `0x010F06D0`의 `65 80 38 6A`, 즉
  `GS: CMP BYTE PTR [EAX], 0x6A`입니다. 64비트에서 `65`는 host의 GS base를
  쓰므로 access가 `0x9FDD`가 되었습니다.

### 판단

frontier가 해소되고 실행이 더 진행됩니다. 새 정지 지점은 이번 수정이 만든
것이 아닙니다. 처리기 없는 경계의 native 단일 실행이라는 일반적 위험은 이번에도
고치지 않았습니다.

### 다음 작업

GS override는 기계적인 lowering으로 풀 수 없습니다. Task 546 결정 5가 guest
segment를 host FS/GS에 설치하지 않는다고 정했으므로, 설계 판단이 먼저
필요합니다. census의 `kSegmentOverrideMem` 57건이 그 덩어리이고, 기존
`LongModeSegmentOverrideEmittable`은 ES/SS/DS만 받고 FS/GS를 거부합니다.
따라서 다음 작업은 `0x010F06D0`이 어떤 문맥에서 GS를 읽는지 확인하고, 그
selector가 무엇을 가리키는지부터 측정하는 것입니다.

## English

### Result

Added long-mode stack sequences for `PUSHAD` (`0x60`) and `POPAD` (`0x61`).
`SequenceWriter` gained displacement-carrying store and load helpers and a
scratch store, and `kMaxLoweredBytes` rose from 24 to 48.

One thing differs from the work order. It said to add the two opcodes to
`NeedsWidthReencode`, but `ClassifyLongModeBytes` tests `IsInvalidInLongMode`
first and `Refuse`s there, so that path was never reached. The invalid branch
now also asks `HasStackSequenceLowering`: the divergence stays
`kInvalidInLongMode` and only a lowering is attached. `PUSH ES`, on the same
list, keeps `kNone` and its existing segment HLE.

### Verification

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu repiu_instruction_census -j 4
long_mode_stack_sequences=true,push_imm8_sign_extended=true,
control_flow_still_refused=true,pop_memory_store_encoding=true,
pop_memory_refusals_kept=true,pushad_entry_esp=true
long_mode_invalid=true,refused=a/a
long_mode_divergence_reasons=true
long_mode_compatibility_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Census:

```text
  lowered             23565
  emittable           51128  (98.58%)
  refused             738  (1.42%)
  blocks complete     11118  (88.88%)
  agrees=true
```

The `pushad` and `popad` rows are gone from the refusal list.

Three consecutive guest runs stopped at the same new point.

```text
[repiu-fault] unhandled signal=0xb rip=0x10f06d0 eip=0x10f06d0 access=0x9fdd
  bytes=65 80 38 6a 75 4d 8d 45 ff b6 01 65 8a 18 88 35
```

### Facts established

* `0x60` is on the invalid list, so it never reached the width path. Without
  checking the classification order this would have been a change with no
  effect.
* The existing `long_mode_invalid` and `long_mode_divergence_reasons` probes
  still pass, because the first checks only
  `compatibility != kIdenticalBytes` and the second only the divergence. Both
  are properties this change had to preserve.
* `PUSHAD` captures the entry ESP before the `lea`. Read after it, a value
  thirty-two lower would sit at `+12` and nothing would raise; the probe checks
  those two bytes and the store that spends them.
* The next frontier is `65 80 38 6A` at `0x010F06D0`,
  `GS: CMP BYTE PTR [EAX], 0x6A`. In 64-bit mode `65` uses the host's GS base,
  which is why the access is `0x9FDD`.

### Assessment

The frontier is resolved and execution advances. The new stopping point is not
created by this change. The general hazard of single-stepping an unhandled
boundary natively is again not fixed.

### Next task

A GS override is not a mechanical lowering. Task 546's decision 5 settled that
raw guest segments are never installed into host FS or GS, so a design decision
comes first. The census's 57 `kSegmentOverrideMem` records are that block, and
the existing `LongModeSegmentOverrideEmittable` admits ES, SS, and DS while
refusing FS and GS. The next task is therefore to measure what context
`0x010F06D0` reads GS in, and what that selector points at.
