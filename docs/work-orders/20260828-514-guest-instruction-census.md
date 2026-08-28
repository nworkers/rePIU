# 게스트 명령 census 작업 지시 (웹 이식 Stage 2)

설계: [20260828-514](../design/20260828-514-guest-instruction-census.md) ·
상위 설계: [20260828-513](../design/20260828-513-web-wasm-execution.md)

1. `src/tools/instruction_census/`에 `repiu_instruction_census`를 추가합니다. EXE 경로를
   받아 parse → runtime plan → relocatable plan → relocated image → translation plan
   사슬을 타고, **기존 `runtime::BuildAotTranslationPlan`이 만든 계획을 셉니다.** 새 디스
   어셈블러를 쓰지 않습니다(설계 결정 1).
2. 세 층위를 냅니다 — (A) distinct mnemonic 수, (B) distinct `(mnemonic, operand 서명)` 수,
   (C) 정적 빈도 상위 목록. 서명은 plan이 든 `bytes`를 Zydis로 다시 디코드해 얻습니다.
3. **선형 스윕 상한을 같이 냅니다.** 코드 object를 처음부터 순차 디코드한 distinct mnemonic
   수이며, 재귀 하강 결과와 **나란히** 찍습니다. 하나만 찍으면 그것이 답으로 읽힙니다.
4. plan의 불확실성 카운터 다섯을 **그대로** 보고합니다 — `indirect_exit_count`,
   `jump_table_count`, `outside_image_target_count`, `decode_failure_count`,
   `analysis_limit_count`.
5. x87 하위 census를 냅니다 — `m80fp` 메모리 피연산자 수, `FLDCW`/`FNSTCW` 수,
   `FSAVE`/`FRSTOR`/`FNSTENV`/`FLDENV` 수, 그리고 x87 mnemonic 분포. 앞의 셋이 **80비트가
   관측 가능해지는 지점**이며 설계 결정 4의 질문에 답할 재료입니다.
6. 출력은 사람이 읽는 표와 기계가 읽는 `key=value` 줄을 함께 냅니다. 한 줄에 몰지 않습니다.
7. CMake에서 **세 구성 모두**에 타깃을 넣습니다. Emscripten 구성에서는 `-sNODERAWFS`를 이
   타깃에만 붙여 Node가 호스트 파일 시스템을 보게 합니다.
8. Windows·Linux·wasm32에서 같은 EXE로 돌려 **세 호스트의 숫자가 일치하는지** 확인합니다.
   어긋나면 그것이 이 작업의 가장 중요한 결과이므로 로그에 그대로 적습니다.
9. 값이 채워지는지를 해석보다 **먼저** 확인합니다. Task 512가 절차에 넣었고 Task 513에서
   `llvm-nm`이 조용히 죽어 `0`이 나온 적이 있습니다.

## 완료 조건

`repiu_instruction_census`가 세 호스트에서 빌드되고, `pumpipx3`의 `PIU.EXE`에 대해 층위
A·B·C와 x87 하위 census, 선형 스윕 상한, 불확실성 카운터 다섯을 냅니다. 세 호스트의 숫자
일치 여부가 로그에 기록되어야 합니다.

**인터프리터 구현은 이 작업의 완료 조건이 아닙니다.** 산출물은 Stage 3의 크기를 정하는
숫자입니다.

## 범위 밖

인터프리터 구현(Stage 3), 동적 census, 명령별 의미 명세.

---

# Guest Instruction Census Work Order (Web Port, Stage 2)

Design: [20260828-514](../design/20260828-514-guest-instruction-census.md) ·
Parent design: [20260828-513](../design/20260828-513-web-wasm-execution.md)

1. Add `repiu_instruction_census` under `src/tools/instruction_census/`. It takes an EXE path,
   follows the chain parse → runtime plan → relocatable plan → relocated image → translation plan,
   and **counts the plan the existing `runtime::BuildAotTranslationPlan` produced.** No new
   disassembler (design Decision 1).
2. Report three levels: (A) distinct mnemonic count, (B) distinct `(mnemonic, operand signature)`
   count, (C) the top of the static frequency list. Signatures come from re-decoding the `bytes` the
   plan already holds.
3. **Print the linear-sweep upper bound alongside it** — the distinct mnemonic count from decoding
   the code object sequentially from its start, printed **next to** the recursive-descent result.
   Printing one alone makes it read as the answer.
4. Report the plan's five uncertainty counters **as they are**: `indirect_exit_count`,
   `jump_table_count`, `outside_image_target_count`, `decode_failure_count`, and
   `analysis_limit_count`.
5. Produce the x87 sub-census: `m80fp` memory operands, `FLDCW` / `FNSTCW`, `FSAVE` / `FRSTOR` /
   `FNSTENV` / `FLDENV`, and the x87 mnemonic distribution. The first three are **where 80 bits
   become observable**, and they are the material for design Decision 4's question.
6. Emit both a human-readable table and machine-readable `key=value` lines. Do not crowd them onto
   one line.
7. Add the target to **all three configurations** in CMake. Under Emscripten, attach `-sNODERAWFS`
   to this target only, so Node can see the host filesystem.
8. Run it on Windows, Linux and wasm32 against the same EXE and check **whether the three hosts
   agree**. A disagreement is the most important result this work can produce, so record it as it is.
9. Confirm the values are filled **before** interpreting them. Task 512 added that step, and Task
   513 met it again when `llvm-nm` died quietly and produced a `0`.

## Completion criteria

`repiu_instruction_census` builds on three hosts and, for `pumpipx3`'s `PIU.EXE`, produces levels A,
B and C, the x87 sub-census, the linear-sweep upper bound, and the five uncertainty counters.
Whether the three hosts agree is recorded in the log.

**Implementing the interpreter is not a completion criterion.** The deliverable is the number that
sizes Stage 3.

## Out of scope

Implementing the interpreter (Stage 3), a dynamic census, per-instruction semantic specification.
