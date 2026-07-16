# Task 214 이후 장기 구동 재검증 작업 지시서
# Post-Task-214 Long-Run Reverification Work Order

## 1. 목적 (Objective)

Task 214가 남긴 미확정 1번 항목("progress `15583` 이후 실제로 어디까지 도달하는지")을 확인하기 위해, `aot-dynamic` 백엔드로 `pumpit1`을 90초 이상 구동하여 Glide 초기화 이후의 실제 진행 상태(그리기 게이트 도달 여부 또는 로더 post-attempt hang 재발 여부)를 확정한다.

To resolve Task 214's open item 1 ("what is actually reached past progress `15583`"), run `pumpit1` under the `aot-dynamic` backend for 90+ seconds and determine what happens after Glide initialization — whether the drawing gate (71–77) is reached or the previously observed loader post-attempt hang recurs.

## 2. 세부 작업 (Tasks)

1. `repiu_loader_win32`/`repiu_supervisor_win32` Debug 타깃을 최신 커밋 기준으로 재빌드한다.
2. `REPIU_EXECUTION_BACKEND=aot-dynamic` 환경에서 `repiu_supervisor_win32.exe pumpit1 90000`을 구동하고 전체 stdout/stderr를 로그로 저장한다.
3. 로그의 `[repiu-live]`/`[repiu-supervisor]` 텔레메트리 라인을 시간순으로 분석하여 `glide_ordinal`, `dispatch_entry/exit`, `last_eip`, `exception`, `fatal_count`의 변화를 확정한다.
4. 결과를 `docs/analysis/current-execution-frontier.md`에 반영한다.

## 3. 검증 범위 (Verification Scope)

코드 변경 없이 기존 빌드의 실행 동작만 관찰하는 순수 검증 작업이므로 별도 단위 테스트는 없다. 90초 supervised 구동과 텔레메트리 로그 분석이 검증 수단이다.

This is a pure observational verification task with no code changes, so no unit tests apply. The 90-second supervised run and telemetry log analysis serve as the verification method.
