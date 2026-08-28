# 게스트 명령 census 설계 (웹 이식 Stage 2)

상위 설계: [20260828-513 웹 실행](20260828-513-web-wasm-execution.md) ·
Stage 1 로그: [20260828-513](../work-logs/20260828-513-web-wasm-build.md)

## 배경 — 답해야 하는 질문 하나

Stage 1은 **코어가 wasm32로 넘어간다**를 증명했고, 실행 엔진은 넘어가지 않는다는 것도 함께
확정했습니다. 다음은 Stage 3(플랫폼 중립 인터프리터)인데, **그것이 얼마나 큰 일인지 아무도
모릅니다.**

이 프로젝트는 지금까지 **게스트 명령의 의미를 해석한 적이 한 번도 없습니다.** 원본 x86이
호스트 CPU에서 그대로 돌기 때문이고, 그것이 헌장의 "게임 로직을 재구현하지 않는다"가 실제로
산 방식입니다. 그래서 저장소가 이름을 붙여 다루는 mnemonic은 **45개뿐**이고, 그 45개는
전부 **폴트 경계에서 특별 취급이 필요한** 것들입니다 — 세그먼트 연산, REP 문자열, 포트 I/O,
INT 래퍼. 나머지는 이름 붙여진 적이 없습니다. 이름이 필요 없었기 때문입니다.

**Stage 2는 그 나머지를 셉니다.** 산출물은 코드가 아니라 숫자입니다.

## 결정 1: 새로 발견하지 않습니다 — 기존 번역 계획을 셉니다

`runtime::BuildAotTranslationPlan`이 이미 필요한 일을 전부 합니다. 진입점에서 재귀 하강으로
CFG를 걷고, Zydis로 32비트 legacy 모드 디코드를 하고, **`AotInstructionRecord.mnemonic`에
mnemonic을 이미 기록**하며, 원본 바이트도 `bytes`에 들고 있습니다.

Task 512가 세운 형태와 같습니다 — **새로 세는 것이 없습니다.** 이미 채워지는 값을 보고기가
찍지 않았을 뿐입니다.

이것이 중요한 이유는 정확성입니다. 별도의 디스어셈블러를 새로 쓰면 그 발견 범위가 엔진의
발견 범위와 달라지고, 그러면 census가 재는 것은 "게스트가 쓰는 명령"이 아니라 "내가 새로 쓴
도구가 찾은 명령"이 됩니다. **엔진이 실제로 번역하는 집합을 세야 Stage 3의 크기와 맞습니다.**

## 결정 2: 두 모집단을 나란히 보고합니다 — 격차가 곧 불확실성입니다

정적 재귀 하강은 **양쪽으로 틀립니다.**

| | 방향 | 이유 |
|---|---|---|
| 재귀 하강 (reachable) | **과소** | 간접 call/jump의 target을 다 찾지 못합니다. `dynamic` backend가 실행 중에 arena에서 블록을 덧붙이는 이유가 그것입니다 |
| 선형 스윕 (linear sweep) | **과대** | 코드 object 안의 데이터를 명령으로 디코드합니다 |

**둘 다 찍고, 격차를 불확실성으로 명시합니다.** 하나만 찍으면 그 숫자가 답으로 읽힙니다.

그리고 재귀 하강의 미발견 크기는 **추정할 필요가 없습니다.** plan이 이미 세고 있습니다 —
`indirect_exit_count`, `jump_table_count`, `outside_image_target_count`,
`decode_failure_count`, `analysis_limit_count`. 이 다섯을 그대로 보고합니다.

## 결정 3: 세 층위로 셉니다

| 층위 | 단위 | 무엇에 쓰는가 |
|---|---|---|
| A | **distinct mnemonic** | 표제 숫자. "몇 종류인가" |
| B | **(mnemonic, operand 서명)** | Stage 3의 실제 구현 단위. `MOV r32,r32`와 `MOV r8,m8`은 다른 코드입니다 |
| C | **정적 빈도** | 구현 순서. 상위 20개가 대부분을 덮으면 인터프리터를 점진적으로 세울 수 있습니다 |

층위 B의 서명은 **Zydis operand 타입의 나열**입니다(register / memory / immediate / pointer와
폭). plan이 든 `bytes`를 다시 디코드해서 얻습니다 — 새 발견이 아니라 이미 확정된 바이트에
대한 재해석이므로 결정 1과 충돌하지 않습니다.

**층위 B가 Stage 3 견적의 근거입니다.** A만으로는 부족합니다 — `MOV` 하나가 수십 개 형태이고,
인터프리터가 쓰는 것은 형태별 코드입니다.

## 결정 4: x87은 같은 census에서, 그러나 다른 질문으로 셉니다

설계 513이 남긴 미확정 항목입니다 — **게임 로직이 80비트 확장 정밀도에 의존하는가.** wasm에는
`f32`와 `f64`뿐이고, 의존한다면 Stage 3·4의 정확성이 통째로 흔들립니다.

"x87 명령이 몇 개인가"는 답이 아닙니다. 80비트가 **관측 가능해지는 지점**을 세야 합니다.

| 세는 것 | 왜 그것이 답인가 |
|---|---|
| `m80fp` 메모리 피연산자 (`FLD`/`FSTP`의 80비트 형태) | 80비트 값이 메모리로 나갔다 들어오면 그 형식 자체가 게스트에게 보입니다. `f64`로는 재현할 수 없습니다 |
| `FLDCW` / `FNSTCW` | 정밀도 제어(PC) 필드를 게스트가 바꾸는지. 24/53/64비트를 오간다면 그 자체가 의존입니다 |
| `FSAVE`/`FRSTOR`/`FNSTENV`/`FLDENV` | 레지스터 파일 전체를 메모리로 노출합니다 |
| 나머지 x87 mnemonic 분포 | 인터프리터가 구현해야 하는 FPU 표면 |

**앞의 셋이 0이면 80비트는 산술 정밀도로만 남고**, 그때 비로소 "`f64`로 충분한가"가 측정
가능한 질문이 됩니다. 0이 아니면 Stage 3 설계가 달라집니다.

이 census는 그 질문에 답하지 않습니다. **답할 수 있는 형태로 만드는 것**까지가 범위입니다.

## 결정 5: 도구는 플랫폼 중립이고, 세 호스트에서 같은 답을 냅니다

parse → runtime plan → relocatable plan → relocated image → translation plan 사슬이 전부
`src/exe`와 `src/runtime`에 있습니다. Win32 정책은 그 **뒤에** 붙으므로, census 도구는
`repiu_exe`만으로 섭니다.

그래서 `repiu_instruction_census`는 Windows·Linux·**wasm32 세 곳에서 모두** 빌드되고 돌아야
합니다. 이것이 Stage 1이 만든 웹 빌드의 첫 실사용이고, 동시에 "같은 코드가 세 호스트에서 같은
답을 내는가"를 한 번 더 확인합니다 — Task 501이 `repiu_core_probe`로 세운 관례 그대로입니다.

wasm에서는 Emscripten 기본 MEMFS가 호스트 파일 시스템을 보지 못하므로 EXE 경로를 넘길 수
없습니다(Stage 1에서 `repiu_chd_cd_probe`가 걸린 것과 같은 벽). **Node 실행은
`-sNODERAWFS`로 구성합니다** — census 도구에 한정된 링크 옵션이며, 브라우저 타깃에는 쓰지
않습니다.

## 결정 6: 산출물은 사람이 읽는 표와 기계가 읽는 줄 둘 다입니다

이 저장소의 측정은 전부 스크립트로 다시 읽힙니다. 그래서 요약은 `key=value` 한 줄로,
분포는 표로 냅니다. Task 512가 "한 줄이 길어지면 사람도 스크립트도 읽기 나빠진다"고 적은
대로 두 줄로 나눕니다.

## 범위 밖

- **인터프리터 구현.** Stage 3입니다. 이 작업은 그 크기를 숫자로 만드는 것까지입니다.
- **동적 census.** 실행 중 실제로 번역된 mnemonic을 세는 것은 별도 작업입니다. 정적 결과가
  Stage 3 견적에 부족할 만큼 좁게 나오면 그때 판단합니다 — 지금 미리 하는 것은 측정 없이
  두 번째 도구를 만드는 일입니다.
- **명령별 의미 명세.** census는 목록이지 명세가 아닙니다.

---

# Guest Instruction Census Design (Web Port, Stage 2)

Parent design: [20260828-513, web execution](20260828-513-web-wasm-execution.md) ·
Stage 1 log: [20260828-513](../work-logs/20260828-513-web-wasm-build.md)

## Background — one question to answer

Stage 1 proved **the core carries to wasm32**, and settled that the execution engine does not. Next
is Stage 3, a platform-neutral interpreter — and **nobody knows how large that is.**

This project has **never once interpreted the meaning of a guest instruction.** The original x86
runs on the host CPU, which is how the charter's "do not reimplement the game logic" has actually
been lived. So the repository names only **45 mnemonics**, and all 45 are the ones needing **special
handling at a fault boundary**: segment operations, REP strings, port I/O, INT wrappers. The rest
have never been named, because names were never needed.

**Stage 2 counts the rest.** Its deliverable is a number, not code.

## Decision 1: discover nothing new — count the existing translation plan

`runtime::BuildAotTranslationPlan` already does all of the work. It walks the CFG by recursive
descent from the entry, decodes with Zydis in 32-bit legacy mode, **already records the mnemonic in
`AotInstructionRecord.mnemonic`**, and keeps the original bytes in `bytes`.

The same shape Task 512 used: **nothing new is counted.** The values were already filled; no
reporter printed them.

This matters for accuracy. A new disassembler would have its own discovery reach, different from the
engine's, and then the census would measure "instructions my new tool found" rather than
"instructions the guest uses". **Only the set the engine actually translates matches Stage 3's
size.**

## Decision 2: report two populations side by side — the gap is the uncertainty

Static recursive descent is wrong **in both directions**.

| | Direction | Why |
|---|---|---|
| Recursive descent (reachable) | **under** | It cannot find every indirect call and jump target. That is exactly why the `dynamic` backend appends blocks from the live arena at run time |
| Linear sweep | **over** | It decodes data inside the code object as instructions |

**Both are printed, and the gap is stated as the uncertainty.** Printing one alone makes that number
read as the answer.

The size of what recursive descent misses **needs no estimate**: the plan already counts it —
`indirect_exit_count`, `jump_table_count`, `outside_image_target_count`, `decode_failure_count`, and
`analysis_limit_count`. All five are reported as they are.

## Decision 3: three levels

| Level | Unit | What it is for |
|---|---|---|
| A | **Distinct mnemonic** | The headline. "How many kinds" |
| B | **(mnemonic, operand signature)** | Stage 3's real implementation unit. `MOV r32,r32` and `MOV r8,m8` are different code |
| C | **Static frequency** | Implementation order. If the top twenty cover most of it, the interpreter can be built incrementally |

Level B's signature is the **sequence of Zydis operand types** (register / memory / immediate /
pointer, with widths), obtained by re-decoding the `bytes` the plan already holds. That is
reinterpretation of settled bytes rather than new discovery, so it does not conflict with Decision 1.

**Level B is what a Stage 3 estimate rests on.** A alone is not enough: one `MOV` is dozens of
forms, and what an interpreter spends is code per form.

## Decision 4: x87 is counted here, but as a different question

This is the item design 513 left unresolved: **does the game logic depend on 80-bit extended
precision?** wasm has only `f32` and `f64`, and if it does depend, the correctness of Stages 3 and 4
moves as a whole.

"How many x87 instructions" is not the answer. What has to be counted is **where 80 bits become
observable**.

| Counted | Why that is the answer |
|---|---|
| `m80fp` memory operands (the 80-bit forms of `FLD` / `FSTP`) | Once an 80-bit value goes to memory and comes back, the format itself is visible to the guest. `f64` cannot reproduce it |
| `FLDCW` / `FNSTCW` | Whether the guest changes the precision-control field. Moving between 24, 53 and 64 bits is itself a dependency |
| `FSAVE` / `FRSTOR` / `FNSTENV` / `FLDENV` | They expose the whole register file to memory |
| The rest of the x87 mnemonic distribution | The FPU surface an interpreter has to implement |

**If the first three are zero, 80 bits survive only as arithmetic precision**, and only then does
"is `f64` enough" become a measurable question. If they are not zero, the Stage 3 design changes.

This census does not answer that question. Making it **answerable** is the scope.

## Decision 5: the tool is platform-neutral and gives the same answer on three hosts

The chain parse → runtime plan → relocatable plan → relocated image → translation plan lives
entirely in `src/exe` and `src/runtime`. The Win32 policy attaches **after** it, so a census tool
stands on `repiu_exe` alone.

So `repiu_instruction_census` builds and runs on Windows, Linux **and wasm32**. That is the first
real use of the web build Stage 1 produced, and it checks once more that the same code gives the
same answer on three hosts — the convention Task 501 established with `repiu_core_probe`.

On wasm, Emscripten's default MEMFS cannot see the host filesystem, so no EXE path can be passed
(the same wall `repiu_chd_cd_probe` hit in Stage 1). **The Node build links with `-sNODERAWFS`**, a
link option scoped to this tool and not used for any browser target.

## Decision 6: the output is both a human table and a machine line

Every measurement in this repository gets re-read by a script. So the summary is one `key=value`
line and the distributions are tables, split across two lines as Task 512 put it: one long line
reads worse for both people and scripts.

## Out of scope

- **Implementing the interpreter.** That is Stage 3. This work turns its size into a number.
- **A dynamic census.** Counting the mnemonics actually translated at run time is separate work. If
  the static result comes out too narrow to estimate Stage 3 from, that is when to decide — doing it
  now means building a second tool without a measurement asking for it.
- **Per-instruction semantic specification.** A census is a list, not a specification.
