# Task 633 작업 로그: census의 jump table 규칙과 `agrees=` 복구

설계: [20260907-633](../design/20260907-633-census-jump-table-agreement.md) ·
작업 지시: [20260907-633](../work-orders/20260907-633-census-jump-table-agreement.md) ·
분석: [linux-port-frontier 3.70](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`repiu_instruction_census`의 `RecordIsEmitted`에 `kJumpTable` 분기를 넣어
`LongModeJumpTableEmittable`을 묻게 했습니다. `LongModeTally`에
`jump_tables` 집계를 추가하고, emit 가능 합계와 보고 줄에 포함했으며,
`agrees` 비교와 emitter 카운터 줄에 `tables=`를 넣었습니다. 규칙을 census
안에서 재구현하지 않고 계속 emitter에게 묻습니다.

### 검증

```text
cmake --build build/linux_x64 --target repiu_instruction_census repiu_core_probe -j 4
./build/linux_x64/repiu_instruction_census build/runtime_mounts/pumpit2a/PIU/PIU.EXE
  jump tables         21  (0.04%)
  emittable           51126  (98.57%)
  refused             740  (1.43%)
  blocks complete     11117  (88.87%)
  emitter counters    ... indcalls=75 tables=21 refused=740  agrees=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`kJumpTable` 행은 non-copy 분포에서 사라졌습니다.

### 확인한 사실

* Task 632 직후 `agrees=false`였고, 차이 21건은 정확히 새 슬롯이 emit하는
  jump table 수였습니다. 소스 주석이 예고한 대로 이 장치가 낡은 규칙 사본을
  세 번째로 잡았습니다.
* `blocks complete`는 88.70%에서 88.87%로 올랐습니다. Task 632가 실제로 늘린
  값이며 census가 반영하지 못하고 있었습니다.
* 남은 거부는 740건(1.43%)이고 non-copy 386건, kCopy 354건입니다. non-copy는
  `kPortIo` 138, `kHleBoundary` 105, `kGuardedSegmentRead` 71,
  `kSegmentOverrideMem` 57, `kIndirectExit` 15입니다. kCopy는 `push`
  stack-pointer 277이 가장 크고, `pushad`와 `popad`는 각 1건입니다.

### 판단

이 수치는 Task 631의 위험 서술을 좁혀 줍니다. 경계가 되는 것 자체는 문제가
아니며, 해석기에 처리기가 있는 경계는 정상 동작합니다. `PUSH ES`가 그
예이고 push 타임라인이 이를 확인했습니다. 위험한 것은 처리기가 없는
경계뿐입니다. 따라서 정책 전면 교체보다 처리기 없는 경계를 하나씩 없애는
쪽이 계속 유효합니다.

이번 작업은 도구만 고쳤고 실행 동작은 바꾸지 않았습니다.

### 다음 작업

`PUSHAD`와 `POPAD`에 long mode stack sequence를 붙입니다. 각 1건이고 현재
frontier이며, `HasStackSequenceLowering`과 `WriteStackSequence`가 이미
그 자리입니다. 그 뒤의 큰 덩어리는 ESP를 피연산자로 쓰는 `push` 277건이고,
이는 별도 설계가 필요합니다.

## English

### Result

`RecordIsEmitted` in `repiu_instruction_census` now has a `kJumpTable` arm that
asks `LongModeJumpTableEmittable`. A `jump_tables` tally was added to
`LongModeTally` and included in the emittable total and the report, and the
`agrees` comparison and the emitter-counter line now carry `tables=`. The census
still asks the emitter rather than reimplementing the rule.

### Verification

```text
cmake --build build/linux_x64 --target repiu_instruction_census repiu_core_probe -j 4
./build/linux_x64/repiu_instruction_census build/runtime_mounts/pumpit2a/PIU/PIU.EXE
  jump tables         21  (0.04%)
  emittable           51126  (98.57%)
  refused             740  (1.43%)
  blocks complete     11117  (88.87%)
  emitter counters    ... indcalls=75 tables=21 refused=740  agrees=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

The `kJumpTable` row is gone from the non-copy distribution.

### Facts established

* `agrees=false` straight after Task 632, and the 21-record gap was exactly the
  jump tables the new slot emits. The guard caught a stale copy of an emission
  rule for the third time, as its comment predicted it would.
* `blocks complete` rose from 88.70% to 88.87%. That is what Task 632 bought and
  the census had not been reporting.
* 740 refusals remain (1.43%): 386 non-copy and 354 kCopy. The non-copy split is
  `kPortIo` 138, `kHleBoundary` 105, `kGuardedSegmentRead` 71,
  `kSegmentOverrideMem` 57, `kIndirectExit` 15. Among kCopy, `push` refused for
  stack-pointer is the largest at 277, and `pushad` and `popad` are one each.

### Assessment

These numbers narrow the hazard Task 631 described. Becoming a boundary is not
itself a problem, and a boundary the interpreter services works correctly --
`PUSH ES` is the example, and the push timeline confirmed it. Only a boundary
with no handler is dangerous. Removing those one at a time therefore remains
better value than replacing the policy wholesale.

This task changed a tool and no execution behavior.

### Next task

Give `PUSHAD` and `POPAD` a long-mode stack sequence. They are one record each,
they are the current frontier, and `HasStackSequenceLowering` and
`WriteStackSequence` are already the place for them. The larger block after that
is the 277 `push` records naming ESP as an operand, which needs its own design.
