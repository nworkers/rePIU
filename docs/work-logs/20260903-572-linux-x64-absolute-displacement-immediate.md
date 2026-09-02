# 작업 기록 20260903-572 — Linux x64 absolute displacement + immediate

설계: [20260903-572](../design/20260903-572-linux-x64-absolute-displacement-immediate.md) ·
작업 지시: [20260903-572](../work-orders/20260903-572-linux-x64-absolute-displacement-immediate.md)

## 구현

`LowerLongModeBytes`의 `kAbsoluteToSib` 분기에서 폭 조건 한 줄을 교체했습니다.

```cpp
// 이전
if (modrm_offset + 1U + 4U != length) { return false; }
```

이 산술 하나가 두 가지를 동시에 주장하고 있었습니다 — "disp32가 여기 있다"와
"뒤에 아무것도 없다". 필요한 것은 앞의 주장뿐이었고, 뒤의 주장이 immediate를 갖는
absolute 형식 전부를 거절하고 있었습니다.

교체한 조건은 설계 결정 2대로 필드 위치를 디코더에게 묻습니다: `raw.disp.size == 32`,
`raw.disp.offset == modrm_offset + 1`, immediate 총 바이트 수가
`length - (disp.offset + 4)`와 정확히 일치, `is_relative` immediate 없음. 그리고
disp32를 쓴 뒤 `[disp.offset + 4, length)` 구간을 그대로 이어 씁니다.

**세 번째 조건이 이 단위의 안전 장치입니다.** displacement 끝과 명령 끝 사이의
바이트가 전부 immediate로 설명되지 않으면 거절합니다. 길이가 맞는다는 이유로
정체를 모르는 바이트를 뒤에 붙이는 것과, 그것이 immediate임을 디코더에게 확인받고
붙이는 것은 다릅니다.

### 없앤 조건이 숨기고 있던 보호

`IsAbsoluteDisplacementForm`은 ModRM의 `mod`와 `rm` **필드만** 봅니다. 명령에 이미
`0x67` prefix가 있으면 guest는 16-bit addressing이고, 그때 `mod=00 rm=101`은 절대
주소가 아니라 `[DI]`입니다. 분류기는 이 형식에도 `kAbsoluteToSib`를 답합니다.

지금까지 그것을 막고 있던 것은 분류기가 아니라 `modrm_offset + 1 + 4 == length`
**산술의 부수 효과**였습니다. 조건을 푸는 순간 보호가 함께 사라지므로,
`raw.disp.size == 32`로 명시적으로 옮기고 probe로 고정했습니다. 이 형식은
displacement가 없어 `disp.size == 0`이므로 계속 거절됩니다.

## 검증

### Linux x64 Release — 확인됨

`repiu_core_probe`: 20/20, failures 0, skipped 2 (`stack_bridge`,
`guest_stack_switch`).

`long_mode_lowering` — lowering 자체:

```text
long_mode_lowering_absolute_refusals=true,classified_absolute=1,refused=1
long_mode_lowering_absolute_imm=true,zf_equal=1,zf_other=0
long_mode_lowering_all=true
```

`long_mode_lowering_absolute_imm`은 방출 바이트가 설계 결정 1의 표와 **정확히**
일치하는지 먼저 보고(`67 80 3c 25 <disp32> <imm8>`), 그 바이트를 실행합니다. 같은
바이트를 두 번 비교합니다 — 같은 immediate로 한 번, 다른 immediate로 한 번 — 그리고
ZF가 각각 1과 0이어야 합니다. **한 번만 비교했다면 검사가 비어 있었을 것입니다**:
immediate를 통째로 잃어버려 0과 비교하는 lowering도 "다르다"고 답하므로 불일치
경우만으로는 구분되지 않습니다.

`long_mode_lowering_absolute_refusals`는 `classified_absolute=1, refused=1`로,
분류기는 여전히 이 형식을 absolute라고 부르고 rewriter가 거절한다는 것 —
즉 보호가 옮겨졌다는 것 — 을 보입니다.

`linux_x64_guest_register` — emitter 경로:

```text
absolute_immediate_equal    observed=0x1          expected=0x1
absolute_immediate_other    observed=0x0          expected=0x0
absolute_immediate_esp      observed=0x20001800   expected=0x20001800
absolute_immediate_lowered  observed=0x2          expected=0x2
guest_absolute_immediate=true lowered=2 copied=4
linux_x64_guest_register_all=true
```

lowering probe와 이 probe는 **다른 것을 봅니다.** 앞의 것은 바이트가 옳은지 보고,
이 것은 **emitter가 그 바이트를 실제로 내는지** 봅니다. 둘은 분리 가능합니다 —
lowering이 옳아도 거기 닿는 경로가 계속 거절할 수 있고, Task 572 이전의 emitter가
정확히 그 상태였습니다. `lowered=2`는 두 비교가 모두 재작성을 거쳤다는 것이고,
`copied=4`는 `sete`/`movzx` 네 개는 그대로 복사됐다는 것입니다.

### census — 확인됨

`roms/pumpipx3/PIU/PIU.EXE`:

| 항목 | Task 571 | Task 572 |
|---|---:|---:|
| lowered | 30,881 | **31,746** (+865) |
| emittable | 72,724 (97.84%) | **73,589 (99.00%)** |
| refused | 1,609 | **744** |
| complete block | 14,782 (85.12%) | **15,525 (89.39%)** |
| **도달 가능 block** | **29 (0.17%)** | **7,404 (42.63%)** |
| **reachable instrs** | **77** | **31,770** |
| serviced block | 18 | 55 |
| first stop | `0x10fc2fa` | **`0x1101370`** |

`agrees=true`입니다. `+865`는 설계에서 예측한 수와 정확히 같고, census의
`refused_mnemonics`에서 `rip-relative/lowering-declined` 항목이 **한 건도 남지 않고**
사라졌습니다.

### 이 단위가 앞의 세 단위와 다른 점 — 도달 가능 block 255배

Tasks 569–571은 방출 가능 명령을 각각 수십 개씩 늘렸지만 도달 가능 block은
18 → 28 → 29로 움직였습니다. Task 563이 지적한 대로 **emittable과 reachable은
다른 척도**이고, 그 세 단위가 연 slot들은 대부분 아직 도달할 수 없는 영역에
있었습니다.

이번에는 29 → 7,404입니다. 차이는 열린 명령의 **수**가 아니라 **위치**입니다.
`cmp [abs32], imm` 계열은 전역 플래그를 읽는 코드의 기본 형태이고, 그런 코드는
분기 직전에 놓입니다. 하나가 막히면 그 뒤의 block 전체가 도달 불가능해집니다.
865개를 한꺼번에 연 것이 아니라, **분기 앞을 막고 있던 것을 치운 것**입니다.

정지 지점의 모양도 바뀌었습니다. Task 571에서는 정지가 한 종류였지만 지금은
아홉 종류에 123곳입니다.

| 정지 종류 | 건수 |
|---|---:|
| `kIndirectExit` | 60 |
| `kCopy stack-pointer/lowering-declined` | 17 |
| `kCopy invalid-in-long-mode` | 16 |
| `kCopy stack-pointer` | 12 |
| `kJumpTable` | 12 |
| `kGuardedSegmentRead` | 3 |
| `kCopy operand-width` | 1 |
| `kCopy silently-different` | 1 |
| `kSegmentOverrideMem` | 1 |

새 first stop은 `0x1101370`, 바이트 `06` = `PUSH ES`이고 `invalid-in-long-mode`입니다.
**가장 큰 정지 원인은 이제 명령 lowering이 아니라 `kIndirectExit` 60건입니다** —
다음 단위가 볼 곳은 lowering 표가 아니라 indirect 분기 처리입니다.

### 회귀 — 확인됨

- **Linux i386 Release `repiu_core_probe`**: 19/19, failures 0, skipped 3
  (x64 전용 probe). 공용 경로인 `LowerLongModeBytes`를 건드렸으므로 필요한
  확인이었습니다.
- **Win32 x86 Debug `repiu_aot_probe`**: `_all=true` 41개, `_all=false` 0개.
  `cache_executable=false`는 실패가 아니라 **아직 실행 가능 메모리에 배치되지 않은
  이미지의 상태 줄**입니다(`src/tools/aot_probe/main.cpp`의 `PrintCache`).

새로 추가한 probe 두 개는 CMake에서
`if(UNIX AND NOT EMSCRIPTEN AND CMAKE_SIZEOF_VOID_P GREATER 4)` 안에 있고 대상이
`repiu_core_probe`이므로, i386 빌드와 Win32 빌드는 이 파일들을 컴파일하지 않습니다.
두 회귀 확인은 공용 변경이 들어간 뒤에 실행되었습니다.

### Task 571에서 남겨 둔 확인

Task 571 작업 로그의 "아직 확인하지 않음" 두 항목을 이번에 함께 끝냈습니다. 위의
i386과 Win32 결과가 그것입니다. 세 번째 항목인
`ValidateAotCodeCacheHleCoverage`의 i386 바이트 배치 단언은 **그대로 남아 있습니다** —
이번 변경이 건드리지 않았고, 여전히 Win32 전용 `repiu_aot_probe`에서 long-mode
방출이 꺼진 채로만 호출됩니다.

---

# Work log 20260903-572 — Linux x64 absolute displacement with an immediate

Design: [20260903-572](../design/20260903-572-linux-x64-absolute-displacement-immediate.md) ·
work order:
[20260903-572](../work-orders/20260903-572-linux-x64-absolute-displacement-immediate.md)

## Implementation

One width condition in `LowerLongModeBytes`'s `kAbsoluteToSib` branch was
replaced.

```cpp
// before
if (modrm_offset + 1U + 4U != length) { return false; }
```

That single arithmetic identity asserted two things at once — "the disp32 is
here" and "nothing follows it". Only the first was wanted, and the second was
refusing every absolute form that carries an immediate.

The replacement asks the decoder for the field positions, per design decision 2:
`raw.disp.size == 32`, `raw.disp.offset == modrm_offset + 1`, a total immediate
size exactly equal to `length - (disp.offset + 4)`, and no `is_relative`
immediate. After writing the disp32 it appends the range
`[disp.offset + 4, length)` unchanged.

**The third condition is this unit's safety property.** Every byte between the
end of the displacement and the end of the instruction must be accounted for as
an immediate, or the instruction is refused. Appending bytes of unknown identity
because the length works out is a different thing from appending bytes the
decoder confirmed are an immediate.

### The protection the removed condition was hiding

`IsAbsoluteDisplacementForm` inspects **only** ModRM's `mod` and `rm` fields. If
the instruction already carries a `0x67` prefix the guest is addressing in 16
bits, and there `mod=00 rm=101` is `[DI]`, not an absolute address. The
classifier answers `kAbsoluteToSib` for that form too.

What kept it out until now was not the classifier but a **side effect of the
arithmetic** `modrm_offset + 1 + 4 == length`. Lifting the condition would have
dropped the protection with it, so it was moved into `raw.disp.size == 32`
explicitly and pinned by a probe. The form carries no displacement, so
`disp.size == 0` still refuses it.

## Verification

### Linux x64 Release — confirmed

`repiu_core_probe`: 20/20, 0 failures, 2 skipped (`stack_bridge`,
`guest_stack_switch`).

`long_mode_lowering` — the lowering itself:

```text
long_mode_lowering_absolute_refusals=true,classified_absolute=1,refused=1
long_mode_lowering_absolute_imm=true,zf_equal=1,zf_other=0
long_mode_lowering_all=true
```

`long_mode_lowering_absolute_imm` first checks that the emitted bytes match
design decision 1's table **exactly** (`67 80 3c 25 <disp32> <imm8>`), then runs
them. It compares the same byte twice — once against an equal immediate and once
against a different one — and requires ZF to be 1 and 0 respectively. **One
comparison would have been an empty check**: a lowering that lost the immediate
entirely and compared against zero also answers "not equal", so the unequal case
alone cannot tell them apart.

`long_mode_lowering_absolute_refusals` reports `classified_absolute=1,
refused=1`, showing that the classifier still calls the form absolute and the
rewriter still refuses it — that is, the protection moved rather than vanished.

`linux_x64_guest_register` — the emitter path:

```text
absolute_immediate_equal    observed=0x1          expected=0x1
absolute_immediate_other    observed=0x0          expected=0x0
absolute_immediate_esp      observed=0x20001800   expected=0x20001800
absolute_immediate_lowered  observed=0x2          expected=0x2
guest_absolute_immediate=true lowered=2 copied=4
linux_x64_guest_register_all=true
```

The two probes watch **different things**. The first asks whether the bytes are
right; this one asks whether the **emitter actually produces them**. Those are
separable — a lowering can be correct while the path reaching it still refuses,
and that is exactly where the emitter stood before Task 572. `lowered=2` says
both compares went through the rewrite; `copied=4` says the four `sete`/`movzx`
bytes were copied as they are.

### Census — confirmed

For `roms/pumpipx3/PIU/PIU.EXE`:

| Item | Task 571 | Task 572 |
|---|---:|---:|
| Lowered | 30,881 | **31,746** (+865) |
| Emittable | 72,724 (97.84%) | **73,589 (99.00%)** |
| Refused | 1,609 | **744** |
| Complete blocks | 14,782 (85.12%) | **15,525 (89.39%)** |
| **Reachable blocks** | **29 (0.17%)** | **7,404 (42.63%)** |
| **Reachable instructions** | **77** | **31,770** |
| Serviced blocks | 18 | 55 |
| First stop | `0x10fc2fa` | **`0x1101370`** |

`agrees=true`. The `+865` is exactly the number the design predicted, and the
`rip-relative/lowering-declined` entry is gone from the census's
`refused_mnemonics` **without a single one left**.

### How this unit differed from the three before it — 255x the reachable blocks

Tasks 569–571 each added tens of emittable instructions while reachable blocks
moved 18 → 28 → 29. As Task 563 observed, **emittable and reachable are
different measures**, and the slots those three units opened mostly sat in
regions still unreachable.

This time it went 29 → 7,404. The difference is not the **number** of
instructions opened but their **position**. The `cmp [abs32], imm` family is the
ordinary shape of code that reads a global flag, and such code sits immediately
before a branch. One of them blocked makes every block behind that branch
unreachable. This did not open 865 instructions so much as **clear what was
standing in front of the branches**.

The shape of the stopping points changed too. At Task 571 there was one kind of
stop; there are now nine kinds across 123 places.

| Stop kind | Count |
|---|---:|
| `kIndirectExit` | 60 |
| `kCopy stack-pointer/lowering-declined` | 17 |
| `kCopy invalid-in-long-mode` | 16 |
| `kCopy stack-pointer` | 12 |
| `kJumpTable` | 12 |
| `kGuardedSegmentRead` | 3 |
| `kCopy operand-width` | 1 |
| `kCopy silently-different` | 1 |
| `kSegmentOverrideMem` | 1 |

The new first stop is `0x1101370`, bytes `06` (`PUSH ES`), an
`invalid-in-long-mode` refusal. **The largest cause of stopping is no longer
instruction lowering but `kIndirectExit`, at 60** — the next unit should look at
indirect branch handling rather than at the lowering table.

### Regression — confirmed

- **Linux i386 Release `repiu_core_probe`**: 19/19, 0 failures, 3 skipped (the
  x64-only probes). Necessary because `LowerLongModeBytes` is a shared path.
- **Win32 x86 Debug `repiu_aot_probe`**: 41 `_all=true`, 0 `_all=false`.
  `cache_executable=false` is not a failure but the **status line of an image not
  yet placed into executable memory** (`PrintCache` in
  `src/tools/aot_probe/main.cpp`).

Both new probes sit inside CMake's
`if(UNIX AND NOT EMSCRIPTEN AND CMAKE_SIZEOF_VOID_P GREATER 4)` and target
`repiu_core_probe`, so neither the i386 nor the Win32 build compiles those files.
Both regression runs were made after the shared change went in.

### What Task 571 left unverified

The two open items in Task 571's work log are closed here; the i386 and Win32
results above are them. The third item — `ValidateAotCodeCacheHleCoverage`'s
literal i386 byte-placement assertions — **remains as it was**. This change did
not touch it, and it is still called only from the Win32-only `repiu_aot_probe`
with long-mode emission off.
