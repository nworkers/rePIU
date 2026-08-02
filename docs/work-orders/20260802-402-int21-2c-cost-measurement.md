# 20260802-402 INT 21h AH=2Ch 비용 측정 계획 / AH=2Ch Cost Measurement Plan

## 한국어

### 목표

Task 401이 미확정으로 남긴 "게임의 `INT 21h AH=2Ch` 지연 루틴이 약 25 FPS의 주된
비용일 가능성이 높다"를 wall clock 대비로 확정하거나 기각한다. 코드는 바꾸지 않는다.

근거와 결과는 [설계 문서](../design/20260802-402-int21-2c-cost-measurement.md)에 있다.

### 작업 범위

1. 기존 `REPIU_EXECUTION_TIME_PROFILE` 계측으로 45초 실행 2회 측정.
2. `kGuestRunTotal` 대비 `kDosService` 및 커널 왕복 배분값 산출.
3. Task 401의 "census 95%" 서술이 범주 오류였음을 확인하고 정정.
4. 측정이 지목하는 실제 비용 중심 기록.
5. 문서: 설계, 작업 로그, `docs/analysis/pumpit3-bring-up.md`,
   `docs/analysis/current-execution-frontier.md`,
   `docs/guides/execution-stall-eip-census.md` 정정.

### 코드 변경

없음. 측정 전용 Task이므로 빌드 검증도 해당 없음.

### 검증 절차

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=45000
set REPIU_EXECUTION_TIME_PROFILE=1
build\Release\repiu_loader_win32.exe pumpit3 > repiu_cost.log 2>&1
```

두 번 실행해 `Win32 execution time cycles`와 `Win32 VEH gap cycles/counts`가 같은
결론을 주는지 확인한다.

---

## English

### Objective

Settle the item Task 401 left unresolved — whether the game's `INT 21h AH=2Ch` delay routine
is the dominant cost behind ~25 FPS — against wall clock. No code changes.

Rationale and results are in the
[design document](../design/20260802-402-int21-2c-cost-measurement.md).

### Task Scope

1. Measure two 45-second runs with the existing `REPIU_EXECUTION_TIME_PROFILE`.
2. Compute `kDosService` plus an apportioned kernel round trip against `kGuestRunTotal`.
3. Confirm and correct the category error in Task 401's "95% of census" statement.
4. Record the real cost centre the measurement points at.
5. Documentation: design, work log, and corrections to
   `docs/analysis/pumpit3-bring-up.md`,
   `docs/analysis/current-execution-frontier.md`, and
   `docs/guides/execution-stall-eip-census.md`.

### Code Changes

None. Measurement-only, so build verification does not apply.

### Verification Procedure

Run pumpit3 twice for 45 seconds with `REPIU_EXECUTION_TIME_PROFILE=1` on the `aot-dbt`
backend and confirm `Win32 execution time cycles` and `Win32 VEH gap cycles/counts` give the
same conclusion in both runs.
