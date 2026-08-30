# 20260830-533 pumpipx3 FF /4 index 값과 producer 후보 관측 설계

## 목적

Task 532는 `FF /4` target 변화가 동일 pointer의 table dword 변화가 아니라 pointer 변화와
함께 발생한다는 것을 확인했습니다. 다음으로 pointer를 구성하는 SIB index/base register의
실제 값 변화를 기록하여, pointer 변화가 index 변화인지 base 변화인지 구분합니다.

현재 예외 경계에서 확인할 수 있는 것은 해당 FF instruction이 실행되기 직전의 register
snapshot입니다. 이 지점만으로 register 값을 만들어 낸 직전 guest instruction을 확정할 수
없으므로, producer 명령 자체는 정적 disassembly 후보로 별도 판정합니다.

## 범위와 불변 조건

* guest code, register, memory, EIP, AOT dispatch 결과를 변경하지 않습니다.
* resolved 32-bit memory operand에 대해서만 SIB index/base component를 기록합니다.
* 기존 pointer/target change count와 resolved/unresolved 의미를 유지합니다.
* sample마다 formatted logging을 추가하지 않습니다.
* register-form, 16-bit address-size, unsupported segment, unreadable target은 기존 fail-closed
  정책을 유지합니다.
* target instruction 자체의 순수 cycle 비용은 이 단위에서 측정하지 않습니다. 현재 sample
  hook은 FF 경계의 VEH 처리 중에 실행되므로, 이 위치에서 읽은 cycle을 target 실행 비용으로
  명명하면 kernel/handler/다음 guest 경로가 섞입니다.

## 설계

`AotFfBoundaryTargetResult`에 resolved SIB index/base register의 값과 유효성을 추가하고,
site 및 hotspot에 다음 누적 상태를 추가합니다.

* 마지막 index/base register 번호와 값
* index 값 변화 count
* base 값 변화 count
* 마지막 component 값의 유효성

변화 count는 이전과 현재 표본 모두 같은 component가 유효할 때 register 번호 또는 값이
달라지면 증가시킵니다. 따라서 `pumpipx3`처럼 `[edx*4+disp]`이고 base가 없는 site에서는
다음 관계를 직접 검사할 수 있습니다.

```text
pointer_change_count == index_value_change_count
base_value_change_count == 0
```

이 관계는 해당 관측 window에서 pointer 변화가 EDX 값 변화와 일치한다는 증거이지, EDX를
계산한 producer instruction의 확정은 아닙니다. producer는 static probe의 bounded linear
disassembly와 runtime component register 번호/값을 함께 대조합니다.

live site line에는 `ir`, `iv`, `ic`, `br`, `bv`, `bc`를 추가합니다. 이들은 각각 index
register/value/change count와 base register/value/change count입니다. 기존 line 길이 제한과
fixed hotspot capacity는 유지합니다.

```mermaid
flowchart LR
    SAMPLE[Resolved FF /4 sample] --> COMPONENTS[Read SIB index/base registers]
    COMPONENTS --> HISTORY[Compare component history]
    HISTORY --> POINTER[Compare effective pointer]
    POINTER --> RUNTIME[Runtime index/base evidence]
    RUNTIME --> STATIC[Bounded static disassembly]
    STATIC --> CANDIDATE[Producer candidate, not proof]
```

## 측정 계획

Task 532와 동일한 trace-free 60초 조건으로 두 타이틀을 재실행하고, 주요 site에서 다음을
비교합니다.

* last index/base register와 값
* component change count와 기존 pointer change count
* 기존 target-change split 및 resolved/unresolved breakdown
* site 직전 bounded static disassembly에서 register write 후보

이번 측정에서는 target-following cycle을 target instruction 비용으로 해석하지 않습니다.
해당 비용은 FF 경계의 선행·후속 exception timestamp와 guest work를 분리할 수 있는 별도
instrument가 필요하므로 후속 단위로 남깁니다.

## 검증 기준

* 기존 Task 532 probe 검사와 새 index/base component 검사가 통과합니다.
* synthetic SIB index 변경은 index change count에 기록되고 base change count는 증가하지
  않습니다.
* 기존 Windows probe와 Linux i386 Release build가 통과합니다.
* 두 타이틀에서 runtime component 결과와 static producer 후보를 확보합니다.
* 확인됨·추정·미확정을 결과 문서에 분리합니다.

---

# 20260830-533 Design: FF /4 Index Values and Producer Candidates in Pumpipx3

## Objective

Task 532 confirmed that `FF /4` target changes occurred with pointer changes rather than same-
pointer table dword changes. The next step records the SIB index/base register values to distinguish
index-driven pointer changes from base-driven changes.

The exception boundary exposes the register snapshot immediately before the FF instruction, but it
does not by itself identify the preceding guest instruction that produced that register. Producer
instructions therefore remain a static-disassembly candidate question.

## Scope and invariants

* Do not change guest code, registers, memory, EIP, or AOT dispatch results.
* Record SIB index/base components only for resolved 32-bit memory operands.
* Preserve existing pointer/target change and resolved/unresolved meanings.
* Do not add formatted logging per sample.
* Preserve the existing fail-closed policy for register form, 16-bit address size, unsupported
  segments, and unreadable targets.
* Do not measure pure target-instruction cycles in this unit. The current hook runs during FF-boundary
  VEH handling, so naming its cycles as target cost would mix kernel, handler, and subsequent guest
  work.

## Design

Add resolved SIB index/base register values and validity to `AotFfBoundaryTargetResult`, and add to
each site and hotspot:

* last index/base register number and value
* index-value change count
* base-value change count
* validity of the last component values

A component change increments when the component was valid in both samples and its register number
or value changed. For a pumpipx3 site shaped as `[edx*4+disp]` with no base, the direct runtime
check is:

```text
pointer_change_count == index_value_change_count
base_value_change_count == 0
```

This establishes agreement between pointer changes and EDX values within the observation window;
it does not prove which instruction produced EDX. Static bounded disassembly is correlated with the
runtime component register/value as a producer candidate.

The live site line adds `ir`, `iv`, `ic`, `br`, `bv`, and `bc` for index register/value/change and
base register/value/change. Existing line-length and fixed-hotspot limits remain unchanged.

## Measurement plan

Rerun both titles under Task 532's identical trace-free 60-second conditions and compare:

* last index/base register and value
* component-change counts against pointer-change count
* existing target-change split and resolved/unresolved breakdown
* register-write candidates in bounded static disassembly before the site

Do not interpret target-following cycles as target-instruction cost in this run. Separating guest work
from FF-boundary exception timestamps requires a dedicated follow-up instrument.

## Acceptance criteria

* Existing Task 532 checks and the new index/base component checks pass.
* A synthetic SIB index change increments index-change count without incrementing base-change count.
* The existing Windows probe and Linux i386 Release build pass.
* Runtime component results and static producer candidates are captured for both titles.
* Confirmed, inferred, and unresolved findings are separated in the result documents.
