# 20260809-452 타깃 범위 PIU10 ISA 보드 작업 지시 / Target-Scoped PIU10 ISA Board Work Order

설계: [20260809-452-target-scoped-piu10-isa-board.md](../design/20260809-452-target-scoped-piu10-isa-board.md)

## 한국어

### 목표

PIU10 ISA flash/CAT702 HLE를 명시적인 target capability로 제한하여 초기 세 CHD 타깃의
기존 하드웨어 경로와 뒤의 네 타깃 경로를 분리합니다.

### 작업 항목

- [x] `TargetProfile`에 기본 false인 PIU10 ISA capability를 추가합니다.
- [x] `pumpito`, `pumpitc`, `pumpitpc`, `pumpite`에서만 capability를 활성화합니다.
- [x] capability를 Win32 실행 설정과 port adapter까지 전달합니다.
- [x] YMZ ROM 초기화와 PIU10 ISA 자산 초기화를 분리합니다.
- [x] profile assertion, 빌드, probe와 두 대표 타깃 실행을 검증합니다.
- [x] architecture, Task 451 설계와 작업 로그를 현재 정책에 맞게 갱신합니다.
- [x] 작업 로그와 Git commit을 남깁니다.

## English

### Objective

Restrict PIU10 ISA flash/CAT702 HLE through an explicit target capability, separating the early
three CHD targets' existing hardware path from the four later targets.

### Work Items

- [x] Add a default-false PIU10 ISA capability to `TargetProfile`.
- [x] Enable it only for `pumpito`, `pumpitc`, `pumpitpc`, and `pumpite`.
- [x] Carry the capability through Win32 execution setup and the port adapter.
- [x] Separate YMZ ROM setup from PIU10 ISA asset setup.
- [x] Verify profile assertions, build, probes, and representative runs of both target groups.
- [x] Update architecture, the Task 451 design, and cumulative logs to the current policy.
- [x] Leave a work log and Git commit.
