# 20260728-336 작업 지시: VEH residual 귀속과 예외 전이 가격 재측정 / Work order

## 한국어

### 목표

1. Task 334~335 이후 이름이 없던 **VEH residual 36.56%(전체의 11.48%)** 를 귀속한다.
2. Task 323이 Debug에서 얻은 **"커널 예외 전이 1.20%, TF/VEH 제거 상한 1.012배"** 를
   Release 기준으로 재측정한다. Task 330의 규칙에 따라 구성 표기 없는 성능 결론은
   더 쓸 수 없기 때문이다.

### 범위

**포함**

* 기존 계측만으로 residual을 귀속한다. 새 코드가 필요한지 먼저 확인한다.
* `repiu_aot_probe`의 예외 전이 calibration을 두 구성에서 읽고, 60초 실행의 VEH 진입
  횟수와 곱해 `unaccounted` 안의 커널 전이 비중을 유도한다.
* `docs/analysis/current-execution-frontier.md`의 1.012배 결론을 갱신한다.

**제외**

* TF/VEH 제거 구현. 이 작업은 **상한을 다시 계산하는 데까지**다.
* 새 최적화.

### 검증 절차

1. Release 60초 실행 1회로 residual이 실제로 귀속되는지 확인한다.
2. 예외 전이 가격을 Debug와 Release에서 각각 읽어 **구성 독립성**을 확인한다.
   값이 다르면 그 자체가 조사 대상이다.
3. 유도한 커널 전이 비중이 `unaccounted`를 넘지 않는지(음수 잔여가 없는지) 확인한다.

---

## English

### Goal

Attribute the VEH residual that Tasks 334 and 335 left unnamed at 36.56% of the VEH and 11.48% of
wall clock, and re-measure in Release the conclusion Task 323 drew in Debug — kernel exception
transition at 1.20% and a 1.012x bound on removing TF and VEH — because Task 330's rule forbids
carrying a configuration-less performance conclusion forward.

### Scope

In scope: attributing the residual with existing instrumentation, checking first whether any new
code is needed; reading the probe's exception-transition calibration in both configurations and
multiplying by the 60-second run's VEH entry count to derive the kernel share inside `unaccounted`;
and updating the frontier's 1.012x conclusion. Out of scope: implementing TF/VEH removal, which
this task only re-prices, and any new optimization.

### Verification

One 60-second Release run showing the residual actually attributed; the transition price read in
both Debug and Release to confirm it is configuration-independent, with any difference becoming its
own investigation; and a check that the derived kernel share fits inside `unaccounted` without
leaving a negative remainder.
