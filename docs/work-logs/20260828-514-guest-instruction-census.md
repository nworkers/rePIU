# Task 514 작업 로그 — 게스트 명령 census (웹 이식 Stage 2)

설계: [20260828-514](../design/20260828-514-guest-instruction-census.md) ·
작업 지시: [20260828-514](../work-orders/20260828-514-guest-instruction-census.md) ·
상위 설계: [20260828-513](../design/20260828-513-web-wasm-execution.md)

## 결과 — Stage 3의 크기는 **명령 형태 320개**입니다

`pumpipx3`의 `PIU.EXE`, 진입점에서 재귀 하강.

| 측정 | 값 |
|---|---:|
| 블록 | 17,367 |
| 명령 | 74,333 |
| **distinct mnemonic (층위 A)** | **120** |
| **distinct (mnemonic, 피연산자 서명) (층위 B)** | **320** |
| 선형 스윕 상한 — distinct mnemonic | **193** |
| 선형 스윕 — 디코드된 명령 | 287,337 (1,006,196 바이트 중) |
| 선형 스윕 — 디코드 불가 바이트 | 115 |

**Stage 3 견적의 근거는 120이 아니라 320입니다.** `MOV` 하나가 형태 19개이고, 인터프리터가
쓰는 것은 형태별 코드입니다.

### 그리고 그 320개는 균등하지 않습니다

| 상위 n개 mnemonic | 누적 비중 |
|---:|---:|
| 5 | 58.44% |
| 10 | 76.84% |
| 17 | **87.89%** |
| 30 | 95.91% |

상위 다섯이 `mov`(32.51%) · `push` · `call` · `add` · `cmp`입니다. **인터프리터를 점진적으로
세울 수 있다는 뜻이고**, 이것이 Stage 3을 한 번에 다 짜지 않아도 되는 근거입니다.

## 가장 중요한 결과 — 격차를 이름으로 찍게 한 것이 답을 뒤집었습니다

작업 지시는 재귀 하강과 선형 스윕 **둘 다** 찍으라고 했습니다. 실제로 찍고 보니 120 대
193이었고, **"120에서 193 사이"는 Stage 3 계획으로 쓸 수 없는 답**이었습니다. 그래서 도구에
차이 73개를 **이름으로** 찍는 것을 더했습니다.

찍고 나서야 보인 것이 있습니다.

```
-- the gap: in the sweep, not reached from the entry (73) --
  aaa aad aam aas arpl bound clc cmpsd daa das fcos fdecstp fdisi8087_nop
  fiadd ficom ficomp fidivr fimul fincstp fist fisttp fisub fisubr fldenv
  fldl2t fldlg2 fnclex fninit fnsave fnstenv frstor fsin insb insd int1 into
  invlpg iretd jecxz jno jnp jns jo js lahf lds lgdt lidt lodsd loop loope
  loopne movups nop outsb outsd popfd psubd pushfd rol salc scasd setl setnl
  sgdt sidt sldt smsw stac std str verr xlat
```

**`fnsave`·`frstor`·`fnstenv`·`fldenv`가 거기 있습니다.** 재귀 하강 census는 이 넷을
**0회**로 셌습니다. 아래 x87 절이 그 때문에 결론을 바꿉니다.

### 73개를 읽으면 (측정이 아니라 판단입니다)

| 분류 | 근거 | 해당 |
|---|---|---|
| **데이터를 명령으로 디코드한 것** | 1999년 Watcom C가 DOS 게임에 낼 수 없는 것들 | BCD `aaa aad aam aas daa das`, 시스템/특권 `arpl bound lgdt lidt sgdt sidt sldt smsw str verr invlpg stac`, 레거시·미문서 `fdisi8087_nop salc int1`, **게임보다 나중에 나온 명령** `fisttp`(SSE3, 2004), `movups psubd` |
| **실제 코드일 가능성이 높은데 못 닿은 것** | 컴파일러가 흔히 내는 것들 | x87 `fsin fcos fiadd fimul fist fisub fidivr ficom ficomp fnclex fninit fdecstp fincstp fldl2t fldlg2`, x87 상태 `fnsave frstor fnstenv fldenv`, 32비트 문자열 `cmpsd lodsd scasd`, 루프 `loop loope loopne jecxz`, 조건 분기·설정 `jo jno js jns jp(jnp) setl setnl`, 그 밖 `lahf pushfd popfd std clc nop rol xlat` |

`fisttp`가 특히 좋은 표지입니다 — **SSE3는 2004년이고 이 게임은 1999년입니다.** 그 바이트는
코드일 수 없으므로, 선형 스윕이 데이터를 디코드하고 있다는 것을 스스로 증명합니다.

### 일관성 확인 하나는 깨끗합니다

**"진입점에서 닿았는데 선형 스윕에는 없는 것: 0개."** 스윕이 재귀 하강의 진부분집합이 아니라
상위집합입니다. 스윕이 어딘가에서 어긋나 실제 명령을 통째로 건너뛰었다면 여기 값이 0이 아니
었을 것이고, 그러면 193이라는 상한 자체를 못 믿었을 것입니다.

## x87 — 80비트는 관측 가능합니다. 그리고 결론은 아직 닫히지 않았습니다

설계 513이 남긴 질문입니다: **게임 로직이 80비트 확장 정밀도에 의존하는가.**

재귀 하강으로 닿는 범위에서:

| 측정 | 값 |
|---|---:|
| x87 명령 | 3,080 (닿은 명령의 4.14%) |
| distinct x87 mnemonic | 40 |
| **80비트 메모리 피연산자** | **14** |
| **제어 워드 접근** (`fldcw` 10 + `fnstcw` 4) | **14** |
| 환경 save/restore | **0** |

**14는 0이 아닙니다.** 80비트 값이 메모리로 나갔다 들어오는 자리가 실제로 있고, 그 자리에서
`f64`는 값을 잃습니다. Watcom이 FPU 스택을 `tbyte`로 스필하는 전형적인 형태이며, 스필과
복원 사이에 정밀도가 살아 있어야 한다면 **Stage 3의 x87 레지스터 파일은 80비트 왕복을 견디는
표현이어야 합니다.**

`fldcw` 10회도 같은 방향입니다. 게스트가 정밀도 제어(PC) 필드를 **직접 설정**하므로, Stage 3은
그 설정을 읽어야 합니다.

**그런데 "환경 save/restore 0"은 결론이 아닙니다.** 위 격차 목록에 `fnsave`·`frstor`·
`fnstenv`·`fldenv`가 전부 들어 있습니다. 둘 중 하나입니다 — 재귀 하강이 못 닿은 실제 코드이거나,
데이터입니다. **이 census는 어느 쪽인지 가르지 못합니다.**

그래서 정직한 결론은 이렇습니다.

> **80비트는 관측 가능합니다(확인됨, 14곳).** 레지스터 파일 전체가 메모리로 노출되는지는
> **미확정**입니다.

## 불확실성 — 왜 재귀 하강이 74,333에서 멈추는가

| 카운터 | 값 |
|---|---:|
| 간접 exit | 117 |
| jump table | 34 |
| image 밖 target | 0 |
| **디코드 실패** | **0** |
| **분석 한계 도달** | **0** |

**디코드 실패 0과 분석 한계 0이 중요합니다.** 재귀 하강이 멈춘 이유는 도구의 한계가 아니라
**간접 분기 117개**입니다. 각각이 벽이고, 그래서 74,333에서 멈춥니다.

이것은 결함이 아니라 설계대로입니다 — `dynamic` backend가 실행 중에 live arena에서 블록을
덧붙이는 이유가 정확히 이것이고, 정적 계획은 **시작 집합**입니다.

## 세 호스트 일치 — 완전 일치입니다

같은 EXE에 대해 Windows x86 (MSVC Debug), Linux i386 (GCC Release), wasm32 (Emscripten
Release)가 **모든 숫자를 같게** 냅니다.

```
[repiu-census] target=pumpit1 blocks=17367 instructions=74333 counted=74333
               mnemonics=120 forms=320 sweep_mnemonics=193 sweep_decoded=287337
               sweep_failed=115 sweep_only=73 reachable_only=0
[repiu-census-x87] total=3080 float80_mem=14 control_word=14 env_save_restore=0
                   mnemonics=40 indirect_exits=117 jump_tables=34
                   decode_failures=0 redecode_failures=0
```

세 호스트에서 위 두 줄이 **바이트 단위로 동일**하고, 73개 격차 목록도 같은 순서로 같습니다.

이것은 census의 신뢰도이자 **Stage 1이 만든 웹 빌드의 첫 실사용**입니다. wasm32가 파서·재배치·
Zydis 디코드·CFG 걷기를 전부 통과해 x86 두 호스트와 같은 답을 냈습니다.

### wasm에서 한 번 죽었습니다 — 그리고 그것이 첫 메모리 숫자입니다

첫 wasm 실행은 한 줄도 찍기 전에 이렇게 끝났습니다.

```
RuntimeError: Aborted(OOM)
    at abortOnCannotGrowMemory ...
    at _emscripten_resize_heap ...
```

Emscripten은 **16 MiB 힙으로 시작하고 기본적으로 늘리지 않고 abort합니다.** 재배치된 게스트
이미지가 그것만으로 **20,382,644 바이트**이므로, 이 도구가 하는 첫 번째 일이 기본값을
넘깁니다.

**이것은 빌드 설정 문제가 아니라 웹 타깃이 낸 첫 하드 넘버입니다.** `-sALLOW_MEMORY_GROWTH=1`로
고쳤고, 고정 `INITIAL_MEMORY`를 쓰지 않은 이유는 이미지 위에 얹히는 plan 구조의 크기가
고정이 아니기 때문입니다 — 한 타이틀에 맞춘 고정값은 아무것도 참으로 유지해 주지 않는
숫자입니다.

Stage 5에서 다시 만납니다. 게스트 runtime object 4는 virtual size가 `0x0127A940`
(약 19.4 MiB)이고, 실제 실행에서는 이미지 20 MB에 AOT 캐시와 arena가 더해집니다.

## 구현에서 갈린 판단들

### 1. 디스어셈블러를 새로 쓰지 않았습니다

`runtime::BuildAotTranslationPlan`이 이미 CFG를 걷고, Zydis로 32비트 legacy 디코드를 하고,
`AotInstructionRecord.mnemonic`에 mnemonic을 기록하며 원본 바이트도 들고 있었습니다.
**Task 512와 같은 형태입니다 — 새로 세는 것이 없고, 보고기가 없었을 뿐입니다.**

새 디스어셈블러를 썼다면 그 발견 범위가 엔진의 발견 범위와 달라졌을 것이고, census가 재는
것은 "게스트가 쓰는 명령"이 아니라 "내가 쓴 도구가 찾은 명령"이 됐을 것입니다. **Stage 3이
구현해야 하는 것은 엔진이 실제로 번역하는 집합입니다.**

층위 B의 피연산자 서명만 `bytes`를 다시 디코드해서 얻습니다. 이미 확정된 바이트에 대한
재해석이지 새 발견이 아닙니다.

### 2. 선형 스윕은 실패 시 명령 길이가 아니라 **1바이트**를 건너뜁니다

디코드에 실패한 바이트는 다음 명령이 어디서 시작하는지에 대해 아무것도 말해주지 않습니다.
더 건너뛰면 하필 그 바이트 뒤에 있던 코드가 통째로 사라지고, 상한이 상한이 아니게 됩니다.

### 3. 명시적 피연산자만 셉니다

Zydis는 숨은 피연산자도 보고합니다 — 플래그 레지스터, `PUSH`의 암묵적 스택 포인터. 그것들은
mnemonic에서 따라오지 별도로 짤 코드가 아니므로 서명에 넣지 않았습니다. 넣었다면 320이
부풀었을 것이고, 부푼 숫자로 Stage 3을 견적했을 것입니다.

### 4. 재디코드 실패를 세서 찍습니다

plan이 이미 한 번 디코드한 바이트이므로, 여기서 실패하면 그것은 평범한 miss가 아니라
**모순**입니다. 건너뛰지 않고 세서 찍습니다. 이번 실행에서 **0**입니다.

## 확인하지 않은 것

- **동적 census를 하지 않았습니다.** 설계는 "정적 결과가 Stage 3 견적에 부족할 만큼 좁게
  나오면 그때 판단한다"고 적었습니다. 이제 판단할 재료가 생겼습니다 — 아래 다음 절.
- **정적 빈도는 실행 빈도가 아닙니다.** 80비트 메모리 접근 14곳이 정적으로 14개 자리라는
  뜻이지, 14번 실행된다는 뜻이 아닙니다. 뜨거운 루프 안에 있을 수도 있습니다.
- **한 타이틀만 쟀습니다.** `pumpipx3`의 `PIU.EXE`입니다. 다른 롬셋의 실행 파일은 다른 분포를
  낼 수 있습니다.

## 다음 — 동적 census가 이제 정당해졌습니다

설계가 정한 조건이 충족됐습니다. 정적 census는 **층위 A·B·C를 충분히 답했지만**, 두 가지를
열어 뒀습니다.

1. **간접 분기 117개 뒤의 코드.** 재귀 하강은 74,333개에서 멈추고, 선형 스윕 상한은
   데이터를 섞어 넣습니다. 실행 중 `dynamic` backend가 실제로 번역한 mnemonic을 세면 그
   사이가 정확히 메워집니다 — **그리고 그 계측 자리는 이미 있습니다.** `aot_boundary_opcode_census`가
   경계 opcode를 세고 있고, 번역 경로에 같은 형태의 집계를 붙이는 일입니다.
2. **`fnsave`/`frstor`/`fnstenv`/`fldenv`가 코드인가 데이터인가.** 이것이 x87 결론을 닫는
   유일한 방법이고, Stage 3·4의 정확성이 여기 걸려 있습니다.

동적 census 없이 Stage 3을 시작해도 됩니다 — 상위 17개가 87.89%이므로 인터프리터의 뼈대는
지금 숫자로 충분합니다. **다만 x87 표현은 동적 census 뒤에 정하는 것이 맞습니다.** 80비트
레지스터 파일을 나중에 바꾸는 것은 인터프리터를 다시 쓰는 일입니다.

## 재현

```bash
# Windows
scripts/build_win32_x86.ps1 -Configuration Debug -Target repiu_instruction_census
build/win32_x86_debug/Debug/repiu_instruction_census.exe roms/pumpipx3/PIU/PIU.EXE

# Linux i386
scripts/build_linux_i386.sh --config Release --headless --target repiu_instruction_census
build/linux_i386/repiu_instruction_census roms/pumpipx3/PIU/PIU.EXE

# wasm32 (NODERAWFS로 링크되어 호스트 경로를 그대로 받습니다)
scripts/build_web_wasm.sh --target repiu_instruction_census
node build/web_wasm/repiu_instruction_census.js roms/pumpipx3/PIU/PIU.EXE
```

---

# Task 514 Work Log — Guest Instruction Census (Web Port, Stage 2)

Design: [20260828-514](../design/20260828-514-guest-instruction-census.md) ·
Work order: [20260828-514](../work-orders/20260828-514-guest-instruction-census.md) ·
Parent design: [20260828-513](../design/20260828-513-web-wasm-execution.md)

## Result — Stage 3's size is **320 instruction forms**

`pumpipx3`'s `PIU.EXE`, recursive descent from the entry point.

| Measurement | Value |
|---|---:|
| Blocks | 17,367 |
| Instructions | 74,333 |
| **Distinct mnemonics (level A)** | **120** |
| **Distinct (mnemonic, operand signature) (level B)** | **320** |
| Linear-sweep upper bound — distinct mnemonics | **193** |
| Linear sweep — instructions decoded | 287,337 (from 1,006,196 bytes) |
| Linear sweep — undecodable bytes | 115 |

**A Stage 3 estimate rests on 320, not 120.** One `MOV` is 19 forms, and what an interpreter spends
is code per form.

### And those 320 are not evenly spread

| Top n mnemonics | Cumulative share |
|---:|---:|
| 5 | 58.44% |
| 10 | 76.84% |
| 17 | **87.89%** |
| 30 | 95.91% |

The top five are `mov` (32.51%), `push`, `call`, `add`, `cmp`. **The interpreter can be built
incrementally**, and that is why Stage 3 does not have to be written all at once.

## The most important result — printing the gap by name overturned an answer

The work order said to print **both** the recursive descent and the linear sweep. Printed, they came
out 120 against 193 — and **"somewhere between 120 and 193" is not an answer to plan Stage 3 from.**
So the tool gained a listing of the 73 in the gap **by name**.

Only after printing them did something show:

```
-- the gap: in the sweep, not reached from the entry (73) --
  aaa aad aam aas arpl bound clc cmpsd daa das fcos fdecstp fdisi8087_nop
  fiadd ficom ficomp fidivr fimul fincstp fist fisttp fisub fisubr fldenv
  fldl2t fldlg2 fnclex fninit fnsave fnstenv frstor fsin insb insd int1 into
  invlpg iretd jecxz jno jnp jns jo js lahf lds lgdt lidt lodsd loop loope
  loopne movups nop outsb outsd popfd psubd pushfd rol salc scasd setl setnl
  sgdt sidt sldt smsw stac std str verr xlat
```

**`fnsave`, `frstor`, `fnstenv` and `fldenv` are in there.** The recursive-descent census counted
those four at **zero**. The x87 section below changes its conclusion because of it.

### Reading the 73 (judgement, not measurement)

| Class | Basis | Members |
|---|---|---|
| **Data decoded as code** | Things Watcom C could not emit for a 1999 DOS game | BCD `aaa aad aam aas daa das`; system and privileged `arpl bound lgdt lidt sgdt sidt sldt smsw str verr invlpg stac`; legacy and undocumented `fdisi8087_nop salc int1`; **instructions younger than the game** `fisttp` (SSE3, 2004), `movups`, `psubd` |
| **Likely real code that was not reached** | Ordinary compiler output | x87 `fsin fcos fiadd fimul fist fisub fidivr ficom ficomp fnclex fninit fdecstp fincstp fldl2t fldlg2`; x87 state `fnsave frstor fnstenv fldenv`; 32-bit string `cmpsd lodsd scasd`; loops `loop loope loopne jecxz`; branches and setters `jo jno js jns jnp setl setnl`; and `lahf pushfd popfd std clc nop rol xlat` |

`fisttp` is a particularly good marker: **SSE3 is 2004 and this game is 1999.** Those bytes cannot be
code, so the sweep proves for itself that it is decoding data.

### One consistency check comes out clean

**"Reached from the entry, absent from the sweep: 0."** The sweep is a superset of the recursive
descent, not a partly-disjoint set. Had the sweep drifted out of alignment somewhere and skipped real
instructions wholesale, this would not be zero — and then the 193 upper bound itself would be
untrustworthy.

## x87 — 80 bits are observable, and the conclusion is still open

This is the question design 513 left: **does the game logic depend on 80-bit extended precision?**

Within what recursive descent reaches:

| Measurement | Value |
|---|---:|
| x87 instructions | 3,080 (4.14% of what was reached) |
| Distinct x87 mnemonics | 40 |
| **80-bit memory operands** | **14** |
| **Control-word access** (`fldcw` 10 + `fnstcw` 4) | **14** |
| Environment save/restore | **0** |

**14 is not zero.** There are real sites where an 80-bit value goes to memory and comes back, and at
those sites `f64` loses the value. It is Watcom's ordinary pattern of spilling the FPU stack as
`tbyte`, and if precision has to survive between spill and reload then **Stage 3's x87 register file
must use a representation that survives an 80-bit round trip.**

The ten `fldcw` say the same thing: the guest **sets the precision-control field itself**, so Stage 3
has to read what it sets.

**But "environment save/restore: 0" is not a conclusion.** All four of `fnsave`, `frstor`, `fnstenv`
and `fldenv` appear in the gap list above. Either they are real code recursive descent could not
reach, or they are data. **This census cannot tell the two apart.**

So the honest conclusion is this:

> **80 bits are observable (confirmed, 14 sites).** Whether the whole register file is exposed to
> memory is **unresolved**.

## Uncertainty — why recursive descent stops at 74,333

| Counter | Value |
|---|---:|
| Indirect exits | 117 |
| Jump tables | 34 |
| Outside-image targets | 0 |
| **Decode failures** | **0** |
| **Analysis limits reached** | **0** |

**The zeros are what matter.** Recursive descent did not stop because the tool ran out of room; it
stopped at **117 indirect branches**. Each is a wall, and that is where 74,333 comes from.

This is by design rather than a defect — it is exactly why the `dynamic` backend appends blocks from
the live arena at run time, and why a static plan is a **starting set**.

## Three-host agreement — complete

For the same EXE, Windows x86 (MSVC Debug), Linux i386 (GCC Release) and wasm32 (Emscripten
Release) produce **every number identically**.

```
[repiu-census] target=pumpit1 blocks=17367 instructions=74333 counted=74333
               mnemonics=120 forms=320 sweep_mnemonics=193 sweep_decoded=287337
               sweep_failed=115 sweep_only=73 reachable_only=0
[repiu-census-x87] total=3080 float80_mem=14 control_word=14 env_save_restore=0
                   mnemonics=40 indirect_exits=117 jump_tables=34
                   decode_failures=0 redecode_failures=0
```

Those two lines are **byte-identical** across the three hosts, and the 73-name gap list matches in
the same order.

That is both the census's credibility and **the first real use of the web build Stage 1 produced**:
wasm32 carried the parser, the relocation, the Zydis decode and the CFG walk, and gave the same
answer as two x86 hosts.

### It died once on wasm — and that is the first memory number

The first wasm run ended before printing a line:

```
RuntimeError: Aborted(OOM)
    at abortOnCannotGrowMemory ...
    at _emscripten_resize_heap ...
```

Emscripten **starts with a 16 MiB heap and, by default, aborts instead of growing it.** The
relocated guest image is **20,382,644 bytes** on its own, so the very first thing this tool does
exceeds the default.

**That is not a build-settings problem, it is the first hard number the web target has produced.**
The fix is `-sALLOW_MEMORY_GROWTH=1`; a fixed `INITIAL_MEMORY` was rejected because the plan
structures layered on the image are not a fixed size, and an initial heap sized to one title is a
number nothing keeps true.

Stage 5 meets this again. Guest runtime object 4 has a virtual size of `0x0127A940` (about
19.4 MiB), and a real run adds the AOT cache and the arena on top of the 20 MB image.

## Judgement calls

### 1. No new disassembler was written

`runtime::BuildAotTranslationPlan` already walked the CFG, decoded with Zydis in 32-bit legacy mode,
recorded the mnemonic in `AotInstructionRecord.mnemonic`, and kept the original bytes. **The same
shape as Task 512: nothing new is counted, there was simply no reporter.**

A new disassembler would have had its own discovery reach, different from the engine's, and the
census would then have measured "instructions my tool found" rather than "instructions the guest
uses". **What Stage 3 must implement is the set the engine actually translates.**

Only level B's operand signature re-decodes the `bytes`, and that is reinterpretation of settled
bytes rather than new discovery.

### 2. The linear sweep skips **one byte** on failure, not one instruction

A byte that fails to decode says nothing about where the next instruction starts. Skipping further
would swallow whatever code sat behind that byte, and an upper bound that hides code is not an upper
bound.

### 3. Only explicit operands are counted

Zydis reports the hidden operands too — the flags register, the implicit stack pointer of a `PUSH`.
Those follow from the mnemonic rather than being code to write, so they are left out of the
signature. Including them would have inflated the 320, and Stage 3 would have been estimated from
the inflated number.

### 4. Re-decode failures are counted and printed

The plan decoded these bytes once already, so a failure here is a **contradiction** rather than an
ordinary miss. It is counted rather than skipped. In this run it is **0**.

## What was not verified

- **No dynamic census was run.** The design said to decide "if the static result comes out too
  narrow to estimate Stage 3 from". There is now material for that decision — see the next section.
- **Static frequency is not execution frequency.** Fourteen 80-bit memory accesses means fourteen
  sites, not fourteen executions. They could sit in a hot loop.
- **One title only.** `pumpipx3`'s `PIU.EXE`. Other ROM sets' executables may distribute differently.

## Next — a dynamic census is now justified

The design's condition is met. The static census **answered levels A, B and C well enough**, and
left two things open.

1. **The code behind the 117 indirect branches.** Recursive descent stops at 74,333 and the
   linear-sweep bound mixes data in. Counting the mnemonics the `dynamic` backend actually
   translates at run time closes that gap exactly — **and the place to instrument already exists**:
   `aot_boundary_opcode_census` counts boundary opcodes today, and this is the same shape of tally
   on the translation path.
2. **Whether `fnsave` / `frstor` / `fnstenv` / `fldenv` are code or data.** That is the only way to
   close the x87 conclusion, and the correctness of Stages 3 and 4 rests on it.

Stage 3 could start without the dynamic census — the top 17 covering 87.89% is enough of a number to
frame the interpreter. **But the x87 representation should be settled after it.** Changing the
80-bit register file later means writing the interpreter twice.

## Reproducing

```bash
# Windows
scripts/build_win32_x86.ps1 -Configuration Debug -Target repiu_instruction_census
build/win32_x86_debug/Debug/repiu_instruction_census.exe roms/pumpipx3/PIU/PIU.EXE

# Linux i386
scripts/build_linux_i386.sh --config Release --headless --target repiu_instruction_census
build/linux_i386/repiu_instruction_census roms/pumpipx3/PIU/PIU.EXE

# wasm32 (linked with NODERAWFS, so it takes the host path directly)
scripts/build_web_wasm.sh --target repiu_instruction_census
node build/web_wasm/repiu_instruction_census.js roms/pumpipx3/PIU/PIU.EXE
```
