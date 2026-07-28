# 20260728-337 작업 지시: 예외 census / Work order: Exception census

## 한국어

### 목표

Task 336이 커널 예외 전이를 전체의 27.7~30.4%로 재산정했다. 그 비용을 줄이는 방법은
**예외의 종류마다 다르므로**, 무엇을 만들지 정하기 전에 1,307,096회가 각각 무엇인지
배타적으로 센다.

기존 계수기로는 알 수 없다. 같은 single-step 하나가 reentry scope와
`HandleSingleStepTrace` 양쪽에 집계되어 합이 VEH 진입 수를 넘는다.

### 범위

**포함**

* VEH prologue에서 예외 코드별로 **정확히 한 번** 세는 census
  (single-step / breakpoint / access violation / other).
* 연속 single-step 구간 길이 분포. 경계와 경계 사이에 TF로 몇 개를 걷는지가
  "HLE 지점당 1회"인지 "긴 해석 구간"인지를 가른다.
* 종료 summary 출력.

**제외**

* 예외 제거 구현.
* EIP 단위 hotspot(이미 `REPIU_SINGLE_STEP_HOTSPOT_PROFILE`로 존재).

### 구현 지침

* census는 어떤 핸들러도 예외를 소비하기 전에, 검증 직후에 수행한다. 그래야 배타성이
  구조적으로 보장된다.
* guest thread 전용이므로 atomic을 쓰지 않는다. VEH prologue는 hot path다.
* 상시 계측이지만 정수 증가 몇 개뿐이며, 진단 계측 비용은 Task 322에서 1.32%로
  측정된 바 있다.

### 검증 절차

1. Release 전체 빌드 통과, `repiu_aot_probe` exit 0.
2. Release 60초 실행에서 census 합계가 VEH 진입 횟수와 일치하는지 확인한다.
   일치하지 않으면 배타성이 깨진 것이다.

---

## English

### Goal

Task 336 re-priced kernel exception transition at 27.7-30.4% of wall clock, and the remedy differs
by exception class, so before building anything the exceptions are counted exclusively. The
existing counters cannot answer it: one single-step is counted by both the reentry scope and
`HandleSingleStepTrace`, so their sum exceeds the VEH entry count.

### Scope

In scope: a census in the VEH prologue counting each exception exactly once by code, the length
distribution of consecutive single-step runs — which separates "one step per HLE site" from "long
interpreted stretches" — and reporting both in the exit summary. Out of scope: implementing any
exception removal, and per-EIP hotspots, which already exist behind
`REPIU_SINGLE_STEP_HOTSPOT_PROFILE`.

### Implementation notes

The census runs immediately after validation and before any handler can consume the exception, so
exclusivity is structural. It uses plain counters because the VEH prologue is a hot path owned by
the guest thread, and it is always on: a few integer increments, against the 1.32% Task 322
measured for always-on diagnostics.

### Verification

A full Release build with `repiu_aot_probe` exiting 0, and a 60-second Release run whose census
total equals the VEH entry count — a mismatch would mean exclusivity is broken.
