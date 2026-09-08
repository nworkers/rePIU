# Task 632 작업 로그: long mode jump table slot

설계: [20260907-632](../design/20260907-632-long-mode-jump-table.md) ·
작업 지시: [20260907-632](../work-orders/20260907-632-long-mode-jump-table.md) ·
분석: [linux-port-frontier 3.69](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

long mode emit 경로에 `AotInstructionKind::kJumpTable` 슬롯을 추가했습니다.
i386처럼 target 표를 cache에 복사하지 않고 guest의 표를 실행 시점에 읽어
target을 기존 return thunk resolver에 넘깁니다. Task 573의 indirect call
슬롯에서 return 주소 push만 뺀 형태입니다. 주소 재작성은
`LowerLongModeTargetLoad`로 일반화하여 두 슬롯이 공유합니다.

받는 형식은 `FF /4` + SIB(scale 4, base 없음, index가 ESP가 아님) + disp32이며
선행 `2E`(CS) 하나를 무시합니다. 다른 segment override, `/2`, ESP index,
base를 쓰는 SIB는 거부하고 기존처럼 경계로 남깁니다.

`linux_x64_guest_register_probe`에 실행 프로브와 거부 프로브를 추가하고,
`ARCHITECTURE.md`의 AOT emission 절에 규칙을 반영했습니다.

### 검증

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
  jump_table_first_asked observed=0x140030 expected=0x140030
  jump_table_resolver_calls observed=0x1 expected=0x1
  jump_table_landed observed=0x3333 expected=0x3333
  jump_table_esp_untouched observed=0x20001800 expected=0x20001800
guest_jump_table=true tables=1
guest_jump_table_refusals=true,cs=1,bare=1,segment=1,call=1,esp_index=1,base=1
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`REPIU_GUEST_WATCH=0x0105547D`는 아무 event도 내지 않고, 해당 fault는
사라졌습니다. 세 번 연속 실행이 모두 새 지점에서 멈췄습니다.

```text
[repiu-fault] unhandled signal=0x4 rip=0x10efe38 eip=0x10efe38 access=0x0
  bytes=60 89 c7 81 3d 4c 62 1a 01 ff ff 00 00 75 2b e8
```

### 확인한 사실

* `0x0105547D`는 planner가 `kJumpTable`로 분류했지만 long mode에 슬롯이 없어
  초기 map에서 이미 `CC` 한 바이트였습니다.
* 새 슬롯은 guest ESP를 건드리지 않습니다. 실행 프로브가 진입 ESP와 같은 값을
  확인하고, resolver 질문이 한 번뿐이라는 점이 같은 사실을 반대편에서
  말합니다.
* 다음 frontier는 `0x010EFE38`의 `60`, 즉 `PUSHAD`입니다. 64비트 모드에 없는
  인코딩이라 native 단일 실행이 SIGILL을 냅니다. Task 631이 기록한 일반적
  위험의 세 번째 사례이며, 앞의 두 건과 달리 조용히 잘못 실행되지 않습니다.
* `0x010EFE38`도 초기 map에서 이미 경계이므로 동적 번역 문제가 아닙니다.

### 판단

frontier가 해소되고 실행이 더 진행됩니다. 새 정지 지점은 이번 수정이 만든
것이 아닙니다. 경계 명령의 native 단일 실행이라는 일반적 위험은 이번에도
고치지 않았고, 이제 세 번째 사례가 쌓였습니다.

### 다음 작업

같은 위험이 세 번 나왔으므로 다음은 개별 명령이 아니라 정책이어야 합니다.
먼저 초기 map의 `hle-boundary` fixup을 명령 종류와 opcode별로 세는 census가
필요합니다. 그 숫자가 있어야 "처리기 없는 경계를 fail-closed로 바꾼다"가
얼마를 되돌리는지, 어떤 opcode부터 lowering을 붙여야 하는지 정할 수 있습니다.
`PUSHAD`/`POPAD`는 그 census의 첫 후보입니다.

## English

### Result

Added an `AotInstructionKind::kJumpTable` slot to the long-mode emit path.
Rather than copying a table of targets into the cache as i386 does, it reads the
guest's own table at run time and hands the target to the existing return-thunk
resolver -- Task 573's indirect-call slot without its return-address push. The
address rewrite was generalized into `LowerLongModeTargetLoad` so both slots
share it.

The accepted form is `FF /4` with a SIB of scale four, no base, and an index
that is not ESP, plus a disp32, with one leading `2E` (CS) ignored. Other
segment overrides, `/2`, an ESP index, and a SIB naming a base register are
refused and stay boundaries as before.

An executing probe and a refusal probe were added to
`linux_x64_guest_register_probe`, and the AOT emission section of
`ARCHITECTURE.md` records the rule.

### Verification

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
  jump_table_first_asked observed=0x140030 expected=0x140030
  jump_table_resolver_calls observed=0x1 expected=0x1
  jump_table_landed observed=0x3333 expected=0x3333
  jump_table_esp_untouched observed=0x20001800 expected=0x20001800
guest_jump_table=true tables=1
guest_jump_table_refusals=true,cs=1,bare=1,segment=1,call=1,esp_index=1,base=1
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`REPIU_GUEST_WATCH=0x0105547D` reports no events and that fault is gone. Three
consecutive runs stopped at the same new point.

```text
[repiu-fault] unhandled signal=0x4 rip=0x10efe38 eip=0x10efe38 access=0x0
  bytes=60 89 c7 81 3d 4c 62 1a 01 ff ff 00 00 75 2b e8
```

### Facts established

* The planner classified `0x0105547D` as `kJumpTable`, but with no long-mode
  slot it was already a single `CC` in the initial map.
* The new slot leaves guest ESP alone. The executing probe checks it against the
  entry ESP, and the single resolver question says the same thing from the other
  side.
* The next frontier is `60` at `0x010EFE38`, `PUSHAD`. That encoding does not
  exist in 64-bit mode, so the native single step raises SIGILL. It is the third
  case of the hazard Task 631 recorded, and unlike the first two it does not run
  something wrong quietly.
* `0x010EFE38` is a boundary in the initial map too, so this is not a dynamic
  translation problem.

### Assessment

The frontier is resolved and execution advances. The new stopping point is not
created by this change. The general hazard of single-stepping a boundary
natively is again not fixed, and there are now three cases of it.

### Next task

Three cases mean the next step should be a policy rather than another single
instruction. It needs a census first: count the initial map's `hle-boundary`
fixups by instruction kind and opcode. Only with those numbers can "make an
unhandled boundary fail closed" be costed against how much reach it gives up,
and only then is it clear which opcodes deserve a lowering first. `PUSHAD` and
`POPAD` are the census's first candidates.
