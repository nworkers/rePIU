# 20260901-556 x64가 낼 수 있는 명령의 비율 / What fraction the x64 emitter can produce

상위 설계: [20260831-546 x64 AOT/DBT 실행 모델](20260831-546-linux-x64-aot-dbt-execution-model.md) ·
선행: [20260831-553 long-mode 방출](20260831-553-linux-x64-code-cache-long-mode-emission.md),
[20260901-555 stack pointer 거절](20260901-555-linux-x64-stack-pointer-refusal.md) ·
현황: [Linux 이식 frontier](../analysis/linux-port-frontier.md)

## 한국어

### 목적

"x64가 게임을 돌리기까지 얼마나 남았는가"를 **감이 아니라 숫자로** 만듭니다.

지금까지의 답은 "stack과 control flow가 없으니 아직 멀다"였습니다. 맞는 말이지만 다음
순서를 정하기에는 부족합니다 — 무엇을 먼저 낮춰야 가장 많이 열리는지 말해 주지 않습니다.

### 무엇을 재는가

실제 guest(`pumpit1`의 `PIU.EXE`, 14,307 block · 59,908 명령)의 AOT plan에 Task 553의
방출 규칙을 그대로 적용하고 결과를 셉니다.

| 층위 | 답하는 질문 |
|---|---|
| 명령 | 지금 방출 가능한 명령이 몇 %인가 |
| 거절 사유 | 못 내는 것들이 **무엇 때문에** 못 나오는가 |
| block | **끝까지 방출되는 block이 몇 개인가** |

세 번째가 결정적입니다. 명령의 90%를 낼 수 있어도 **block 하나를 끝까지 못 내면 실행은
0**입니다. block은 control flow로 끝나고, 그 control flow가 안 나오면 다음 block으로 갈
길이 없습니다.

### 결정

#### 1. 표제 숫자는 emitter가 직접 셉니다

Task 553이 image에 `long_mode_copied_count`·`long_mode_lowered_count`·
`long_mode_refused_count`를 둔 이유가 이것입니다. census가 판정 규칙을 **다시 구현하면
드리프트**합니다 — emitter가 바뀌고 census가 안 바뀌면 숫자가 조용히 거짓이 됩니다.

그래서 census는 plan을 만들어 `enable_long_mode_emission = true`로 **실제로 image를
빌드하고**, emitter 자신의 카운터를 읽습니다. 구조적으로 어긋날 수 없습니다.

#### 2. 사유별 분해는 census가 따로 셉니다

카운터 셋은 합계일 뿐 "왜"를 말하지 않습니다. 그래서 census는 명령마다
`ClassifyLongModeBytes`를 한 번 더 불러 divergence 사유별로 셉니다.

**두 숫자는 일치해야 하고, 일치하는지 출력합니다.** 어긋나면 그 자체가 발견입니다 —
census가 emitter의 규칙을 잘못 옮겼거나, emitter가 판정기와 다르게 행동한다는 뜻입니다.

#### 3. emitter의 dedup을 따라합니다

emitter는 같은 guest 주소를 두 번 만나면 두 번째를 건너뜁니다. plan의 명령 수와 emitter가
센 수가 다를 수 있으므로, census도 본 주소를 기억합니다. 이것을 맞추지 않으면 결정 2의
일치 검사가 실패하고, 실패의 원인이 진짜 결함인지 dedup인지 구분할 수 없게 됩니다.

#### 4. block 완결성은 명령 규칙에서 유도합니다

block 안의 **모든** 명령이 방출되면 그 block은 완결입니다. 하나라도 거절되면 그 block은
INT3에서 멈춥니다.

### 비범위

* 무엇을 먼저 고칠지 **정하는 것**. 이 단위는 숫자를 만들고, 순서는 그 숫자를 보고 정합니다.
* lowering 추가.
* 동적 census(실행 중 실제로 번역된 명령). 정적 plan이 먼저입니다.

## English

### Objective

Turn "how far is x64 from running the game" into **a number rather than a feeling.**

The answer so far has been "stack and control flow are missing, so it is far". That is
true and not enough to order the next units by -- it does not say which lowering opens the
most.

### What is measured

Task 553's emission rules, applied to the real guest's AOT plan (`pumpit1`'s `PIU.EXE`,
14,307 blocks and 59,908 instructions), with the outcomes counted.

| Level | Question it answers |
|---|---|
| Instruction | what fraction can be emitted today |
| Refusal reason | **why** the rest cannot |
| Block | **how many blocks come out complete** |

The third decides. Being able to emit 90% of instructions still means **zero execution if
not one block completes**: a block ends in control flow, and without that there is no way
to the next block.

### Decisions

#### 1. The headline numbers are counted by the emitter itself

This is what Task 553 put `long_mode_copied_count`, `long_mode_lowered_count` and
`long_mode_refused_count` on the image for. A census that **reimplements the rule drifts**
-- change the emitter without changing the census and the number quietly becomes false.

So the census builds the plan, **actually builds an image** with
`enable_long_mode_emission = true`, and reads the emitter's own counters. They cannot
disagree by construction.

#### 2. The census counts the breakdown separately

Three counters are a total and say nothing about why. So the census calls
`ClassifyLongModeBytes` once more per instruction and tallies by divergence reason.

**The two must agree, and whether they agree is printed.** A disagreement is itself a
finding: either the census copied the emitter's rule wrongly, or the emitter behaves
differently from the classifier.

#### 3. The emitter's dedup is mirrored

The emitter skips a guest address it has already seen. The plan's instruction count and the
emitter's counted total can therefore differ, so the census remembers addresses too.
Without that, decision 2's agreement check fails and nobody can tell whether the cause is a
real defect or the dedup.

#### 4. Block completeness follows from the instruction rule

A block is complete when **every** instruction in it is emitted. One refusal and that block
stops at an INT3.

### Out of scope

* **Deciding** what to fix first. This unit produces the numbers; the order is chosen by
  reading them.
* Adding any lowering.
* A dynamic census of what actually gets translated at run time. The static plan comes
  first.
