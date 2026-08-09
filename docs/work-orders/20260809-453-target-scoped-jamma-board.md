# 20260809-453 타깃 범위 JAMMA 보드 작업 지시 / Target-Scoped JAMMA Board Work Order

설계: [20260809-453-target-scoped-jamma-board.md](../design/20260809-453-target-scoped-jamma-board.md)

## 한국어

### 목표

JAMMA/YMZ280B/EEPROM HLE를 명시적인 target capability로 제한하고 모든 `pumpit*`
profile에서만 활성화합니다.

### 작업 항목

- [x] `TargetProfile`에 기본 false인 JAMMA board capability를 추가합니다.
- [x] 모든 `pumpit*` profile에서만 capability를 활성화합니다.
- [x] capability를 Win32 실행 설정과 port adapter까지 전달합니다.
- [x] YMZ280B 초기화와 `0x02A0..0x02AF` 라우팅을 capability로 제한합니다.
- [x] profile assertion, 빌드, probe와 대표 실행을 검증합니다.
- [x] architecture와 관련 분석 문서를 현재 정책에 맞게 갱신합니다.
- [x] 작업 로그와 Git commit을 남깁니다.

## English

### Objective

Restrict JAMMA/YMZ280B/EEPROM HLE through an explicit target capability and enable it only for
all `pumpit*` profiles.

### Work Items

- [x] Add a default-false JAMMA-board capability to `TargetProfile`.
- [x] Enable the capability only for every `pumpit*` profile.
- [x] Carry the capability through Win32 execution setup and the port adapter.
- [x] Gate YMZ280B setup and `0x02A0..0x02AF` routing on the capability.
- [x] Verify profile assertions, build, probes, and representative runs.
- [x] Update architecture and related analysis to the current policy.
- [x] Leave a work log and Git commit.
