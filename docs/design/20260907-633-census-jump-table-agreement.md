# Task 633 설계: census의 jump table 규칙과 `agrees=` 복구

## 한국어

### 배경

`repiu_instruction_census`는 emit 가능 여부를 emitter에게 묻고, 마지막에
emitter의 자체 카운터와 자기 집계를 비교하여 `agrees=`를 보고합니다. 이
비교는 "emit 규칙의 사본이 낡았다"를 잡기 위한 장치이며, 소스 주석은 이미 두
번 그렇게 잡혔다고 기록하고 있습니다.

Task 632가 long mode에 jump table 슬롯을 추가한 뒤 이 장치가 정확히 그 일을
했습니다.

```text
emitter counters ... indcalls=75 refused=740  agrees=false
refused             761  (1.47%)
  kJumpTable                            21  (5.16% of non-copy)
```

census는 여전히 `kJumpTable`을 `default: return false`로 보내 21건을 거부로
세고, emitter는 그 21건을 emit합니다. 761 - 740 = 21이 정확히 그 차이입니다.

### 설계

1. `RecordIsEmitted`의 switch에 `kJumpTable` 분기를 넣고
   `LongModeJumpTableEmittable`을 묻습니다.
2. `LongModeTally`에 `jump_tables` 집계를 넣고, 다른 종류와 같은 자리에서
   누적합니다.
3. emit 가능 합계와 보고 줄에 jump table을 포함합니다.
4. `agrees` 비교에 `long_mode_jump_table_count == long_mode.jump_tables`를
   넣고, emitter 카운터 줄에 `tables=`를 출력합니다.

census는 규칙을 재구현하지 않고 계속 emitter에게 묻습니다. 이 작업이 고치는
것은 "묻지 않고 default로 떨어뜨리던" 한 종류입니다.

### 검증 전략

* `repiu_instruction_census`를 `pumpit2a`의 `PIU.EXE`에 대해 실행합니다.
* `agrees=true`가 되는지 확인합니다.
* `refused`가 761에서 740으로 내려가고 `kJumpTable` 행이 사라지는지
  확인합니다.
* `repiu_core_probe`가 계속 통과하는지 확인합니다.

## English

### Background

`repiu_instruction_census` asks the emitter whether a record can be emitted and,
at the end, compares the emitter's own counters against its tally as `agrees=`.
That comparison exists to catch a stale copy of an emission rule, and the source
comments record that it has already caught one twice.

After Task 632 added the long-mode jump-table slot, the guard did exactly its
job:

```text
emitter counters ... indcalls=75 refused=740  agrees=false
refused             761  (1.47%)
  kJumpTable                            21  (5.16% of non-copy)
```

The census still sends `kJumpTable` to `default: return false` and counts those
21 as refusals while the emitter emits them. 761 - 740 = 21 is exactly that gap.

### Design

1. Add a `kJumpTable` arm to the `RecordIsEmitted` switch that asks
   `LongModeJumpTableEmittable`.
2. Add a `jump_tables` tally to `LongModeTally` and accumulate it beside the
   other kinds.
3. Include jump tables in the emittable total and in the report.
4. Add `long_mode_jump_table_count == long_mode.jump_tables` to the `agrees`
   comparison and print `tables=` in the emitter-counter line.

The census keeps asking the emitter rather than reimplementing the rule. What
this fixes is the one kind it was not asking about.

### Verification strategy

* Run `repiu_instruction_census` against `pumpit2a`'s `PIU.EXE`.
* Confirm `agrees=true`.
* Confirm `refused` falls from 761 to 740 and the `kJumpTable` row is gone.
* Confirm `repiu_core_probe` still passes.
