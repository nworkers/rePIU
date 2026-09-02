# 작업 기록 20260902-571 — Linux x64 guarded segment pop

설계: [20260902-571](../design/20260902-571-linux-x64-guarded-segment-pop.md) ·
작업 지시: [20260902-571](../work-orders/20260902-571-linux-x64-guarded-segment-pop.md)

## 구현

long-mode `kGuardedSegmentPop` slot을 추가했습니다. guest stack top의 하위
16비트가 해당 segment의 shadow selector와 같으면 명령의 남은 효과는 `ESP += 4`
하나뿐이므로 slot이 그것만 수행하고 fallthrough block으로 갑니다. 다르면 guest
ESP를 진입 값 그대로 둔 채 flags를 복원하고 INT3 HLE 경계로 갑니다.

이 단위의 실제 위험은 segment 의미가 아니라 **offset**이었습니다. Task 559의
lowered `PUSHFD`는 host stack이 아니라 **guest stack**에 씁니다. 따라서 flags를
저장한 뒤 guest ESP는 진입 값보다 4 작고, 비교해야 할 stack word는 `[r15]`가
아니라 `[r15+4]`에 있습니다. `[r15]`를 읽었다면 저장된 flags를 selector와
비교하게 되고, 그것은 **항상 불일치**하므로 fallback만 타는 guard가 됩니다 —
크래시 없이 "동작하는 것처럼 보이는" 실패입니다. 그래서 probe가 값과 ESP를
둘 다 고정합니다.

방출 바이트는 설계 결정 2의 표와 같고, 어셈블러로 인코딩을 먼저 확인한 뒤
emitter에 넣었습니다.

```text
9C 41 5E 45 8D 7F FC 45 89 37   flags save (lowered PUSHFD)   ESP -= 4
45 8B 77 04                     mov r14d, [r15+4]
67 66 44 3B 34 25 <disp32>      cmp r14w, word ptr [shadow]
74 0B                           je success
45 8B 37 45 8D 7F 04 41 56 9D   flags restore
CC                              int3  (fallback_offset)
45 8B 37 45 8D 7F 04 41 56 9D   flags restore
45 8D 7F 04                     lea r15d,[r15+4]
E9 <rel32>                      jmp fallthrough_target
```

## 엔진에 남아 있던 사본과 그것이 숨기고 있던 손상

설계 결정 3대로 pop patch를 `PatchAotGuardedSegmentPopSites`로 runtime에 옮기고,
engine의 세 경로(정적 배치·dynamic append·재해결)를 모두 runtime patcher 호출로
바꿨습니다.

그 과정에서 **기존 결함 하나를 확인했습니다.** `ResolveAotGuardedSegmentLoads`는
counter operand가 항상 있다고 가정하고 `image_bytes + success_counter_address_offset`에
무조건 씁니다. Task 569의 x64 load site는 counter operand가 없어 그 offset이 0이므로,
이 경로는 counter 주소를 **이미지의 첫 4바이트에** 씁니다. x64가 아직 게스트를
돌리지 않아 드러나지 않았을 뿐이고, dynamic append 경로에서는 방금 복사한 이미지
시작부를 덮게 됩니다. 두 patcher가 `has_counter_operands`를 보도록 통일해
없앴습니다.

hand-built site 두 곳(`selector_guard_probe`의 pop site)도 emitter를 대신하는
자리이므로 `guard_prologue`와 `has_counter_operands`를 선언하도록 고쳤습니다.

## 검증

### Linux x64 Release — 확인됨

`repiu_core_probe`: 20/20, failures 0, skipped 2
(`stack_bridge`, `guest_stack_switch`), `linux_x64_guest_register_all=true`.

새 `ProbeGuardedSegmentPop`의 12개 확인 항목이 모두 통과했습니다.

```text
segment_pop_patcher_sites     0x1
segment_pop_patcher_native    0x1
segment_pop_unresolved        0x1
segment_pop_unresolved_trap   0xcc
segment_pop_restored          0x1
segment_pop_marker            0x12345678
segment_pop_flags             0xffffff00
segment_pop_esp               0x20001804   (진입 0x20001800 + 4)
segment_pop_boundary          0x1
segment_pop_no_marker         0x0
segment_pop_mismatch_esp      0x20001800   (불변)
segment_pop_mismatch_word     0xdead0024   (소비되지 않음)
guest_segment_pop=true slots=1
```

`segment_pop_esp`와 `segment_pop_mismatch_esp`가 설계 결정 2의 offset을 직접
겨눕니다. 일치 경로는 정확히 4 증가하고 불일치 경로는 불변입니다.
`segment_pop_mismatch_word`는 stack word가 그대로 남아 HLE가 원본 `POP`을
재실행할 수 있음을 보입니다. stack word의 상위 16비트를 shadow와 다르게
(`0xDEAD____`) 두었으므로 32비트 비교였다면 일치 경로가 실패했을 것입니다.

### census — 확인됨

`roms/pumpipx3/PIU/PIU.EXE`:

| 항목 | Task 570 | Task 571 |
|---|---:|---:|
| guarded seg pops | 0 | **49** |
| emittable | 72,675 (97.77%) | **72,724 (97.84%)** |
| refused | 1,658 | **1,609** |
| complete block | 14,736 (84.85%) | **14,782 (85.12%)** |
| 도달 가능 block | 28 | **29** |
| reachable instrs | 76 | **77** |
| first stop | `0x10fc2d5` (`1f`) | **`0x10fc2fa`** |

`emitter counters ... segpops=49 ... agrees=true`입니다. 즉 census의 판정과
emitter가 실제로 낸 수가 일치합니다.

첫 정지는 `0x10fc2fa`, 바이트 `80 3d a6 93 15 01 01` = `cmp byte ptr [0x11593a6], 1`로
이동했습니다. **kind가 바뀌었습니다** — 지금까지 세 단위는 전용 slot이 없는
non-copy kind가 막고 있었지만, 이제 막는 것은 refused `kCopy`입니다. 다음 단위는
segment slot이 아니라 이 형식의 lowering을 봐야 합니다.

도달 block 증가폭(28 → 29)이 앞 두 단위보다 작습니다. 49개 slot이 열렸는데 도달은
1개만 늘었다는 것은, 나머지 48개가 아직 도달 불가능한 영역에 있다는 뜻입니다.
`emittable`과 `reachable`이 다른 척도라는 Task 563의 지적이 그대로 유효합니다.

### 회귀 — Task 572에서 확인됨

이 커밋 시점에는 아래 두 확인이 없었고, 그래서 이 작업을 완료로 보지 않았습니다.
둘 다 [Task 572](20260903-572-linux-x64-absolute-displacement-immediate.md)에서
실행했습니다. 두 축 모두 회귀가 없으므로 이 단위는 완료입니다.

- **Linux i386 Release `repiu_core_probe`**: 19/19, failures 0, skipped 3
  (x64 전용 probe).
- **Win32 x86 Debug `repiu_aot_probe`**: `_all=true` 41개, `_all=false` 0개.
  `cache_executable=false`는 실패가 아니라 아직 실행 가능 메모리에 배치되지 않은
  이미지의 상태 줄입니다.

이번 변경이 `ResolveAotGuardedSegmentPops`/`ResolveAotGuardedSegmentLoads`와
`selector_guard_probe`의 hand-built site를 건드리므로 **i386 경로에 실제 영향이
있었고**, 그래서 필요한 확인이었습니다.

### 아직 확인하지 않음

- `ValidateAotCodeCacheHleCoverage`의 pop/load layout 단언은 i386 바이트 배치를
  그대로 검사합니다. 이 함수는 Win32 전용 `repiu_aot_probe`에서만 호출되고
  거기서는 long-mode 방출이 꺼져 있으므로 지금은 문제가 없지만, long-mode
  image로 호출하면 실패합니다. Task 569의 load 단언도 같은 성질이며 이번에
  바꾸지 않았습니다.

---

# Work log 20260902-571 — Linux x64 guarded segment pop

Design: [20260902-571](../design/20260902-571-linux-x64-guarded-segment-pop.md) ·
work order:
[20260902-571](../work-orders/20260902-571-linux-x64-guarded-segment-pop.md)

## Implementation

Added the long-mode `kGuardedSegmentPop` slot. When the low 16 bits of the guest
stack top equal that segment's shadow selector, the instruction's only remaining
effect is `ESP += 4`; the slot performs exactly that and continues into the
fallthrough block. Otherwise it restores flags, leaves guest ESP at its entry
value, and reaches the INT3 HLE boundary.

The real hazard in this unit was not segment semantics but the **offset**. Task
559's lowered `PUSHFD` writes to the **guest** stack, not the host's. After the
flags save, guest ESP is four below its entry value and the word to compare sits
at `[r15+4]`, not `[r15]`. Reading `[r15]` would compare the saved flags against
a selector, which **always mismatches** -- producing a guard that only ever falls
back. That failure crashes nothing and looks like working code, so the probe pins
both the value delivered to the guest and the ESP left behind.

The emitted bytes follow design decision 2's table; the encodings were confirmed
with an assembler before going into the emitter.

```text
9C 41 5E 45 8D 7F FC 45 89 37   flags save (lowered PUSHFD)   ESP -= 4
45 8B 77 04                     mov r14d, [r15+4]
67 66 44 3B 34 25 <disp32>      cmp r14w, word ptr [shadow]
74 0B                           je success
45 8B 37 45 8D 7F 04 41 56 9D   flags restore
CC                              int3  (fallback_offset)
45 8B 37 45 8D 7F 04 41 56 9D   flags restore
45 8D 7F 04                     lea r15d,[r15+4]
E9 <rel32>                      jmp fallthrough_target
```

## The engine's copy, and the corruption it was hiding

Per design decision 3, pop patching moved into
`PatchAotGuardedSegmentPopSites`, and all three engine paths -- static
placement, dynamic append, and re-resolution -- now call the runtime patchers.

Doing so **confirmed an existing defect.** `ResolveAotGuardedSegmentLoads`
assumed counter operands are always present and wrote unconditionally to
`image_bytes + success_counter_address_offset`. Task 569's x64 load sites have no
counter operands, so that offset is zero and the path wrote counter addresses
into the **image's first four bytes**. It is invisible only because x64 does not
run a guest yet; on the dynamic-append path it would overwrite the start of the
image just copied in. Making both patchers honour `has_counter_operands` removes
it.

The hand-built pop site in `selector_guard_probe` stands in for an emitter, so it
now declares its own `guard_prologue` and `has_counter_operands` too.

## Verification

### Linux x64 Release — confirmed

`repiu_core_probe`: 20/20, 0 failures, 2 skipped (`stack_bridge`,
`guest_stack_switch`), `linux_x64_guest_register_all=true`.

All twelve checks in the new `ProbeGuardedSegmentPop` passed:

```text
segment_pop_patcher_sites     0x1
segment_pop_patcher_native    0x1
segment_pop_unresolved        0x1
segment_pop_unresolved_trap   0xcc
segment_pop_restored          0x1
segment_pop_marker            0x12345678
segment_pop_flags             0xffffff00
segment_pop_esp               0x20001804   (entry 0x20001800 + 4)
segment_pop_boundary          0x1
segment_pop_no_marker         0x0
segment_pop_mismatch_esp      0x20001800   (unchanged)
segment_pop_mismatch_word     0xdead0024   (not consumed)
guest_segment_pop=true slots=1
```

`segment_pop_esp` and `segment_pop_mismatch_esp` aim directly at design decision
2's offset: the matching path advances by exactly four and the mismatching path
does not move. `segment_pop_mismatch_word` shows the stack word survives, so the
HLE can re-execute the original `POP`. The word's upper half is deliberately
unequal to the shadow (`0xDEAD____`), so a 32-bit comparison would have failed
the matching path.

### Census — confirmed

For `roms/pumpipx3/PIU/PIU.EXE`:

| Item | Task 570 | Task 571 |
|---|---:|---:|
| Guarded segment pops | 0 | **49** |
| Emittable | 72,675 (97.77%) | **72,724 (97.84%)** |
| Refused | 1,658 | **1,609** |
| Complete blocks | 14,736 (84.85%) | **14,782 (85.12%)** |
| Reachable blocks | 28 | **29** |
| Reachable instructions | 76 | **77** |
| First stop | `0x10fc2d5` (`1f`) | **`0x10fc2fa`** |

`emitter counters ... segpops=49 ... agrees=true`: the census's predicate and
what the emitter actually produced are the same number.

The first stop moved to `0x10fc2fa`, bytes `80 3d a6 93 15 01 01`
(`cmp byte ptr [0x11593a6], 1`). **The kind changed**: the last three units were
blocked by non-copy kinds with no dedicated slot, and what blocks now is a
refused `kCopy`. The next unit should look at lowering that form, not at another
segment slot.

Reachability grew less than in the previous two units (28 → 29). Forty-nine slots
opened and only one more block became reachable, which means the other
forty-eight sit in regions still unreachable. Task 563's point that emittability
and reachability are different measures continues to hold.

### Regression — verified under Task 572

Neither check below had a result at this commit, which is why this task was not
treated as complete. Both were run under
[Task 572](20260903-572-linux-x64-absolute-displacement-immediate.md). Neither
axis regressed, so this unit is complete.

- **Linux i386 Release `repiu_core_probe`**: 19/19, 0 failures, 3 skipped (the
  x64-only probes).
- **Win32 x86 Debug `repiu_aot_probe`**: 41 `_all=true`, 0 `_all=false`.
  `cache_executable=false` is not a failure but the status line of an image not
  yet placed into executable memory.

The checks were necessary because this change touches
`ResolveAotGuardedSegmentPops`/`ResolveAotGuardedSegmentLoads` and the
hand-built site in `selector_guard_probe`, so it **does affect the i386 path**.

### Not yet verified

- `ValidateAotCodeCacheHleCoverage`'s pop and load layout assertions still check
  i386 byte placement literally. That function is called only from the Win32-only
  `repiu_aot_probe`, where long-mode emission is off, so nothing fails today --
  but it would fail on a long-mode image. Task 569's load assertion has the same
  property and was left unchanged here.
