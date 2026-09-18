# Task 705 작업 지시 — Linux x64 thread interrupt no-op context 보존

## 목표

guest/AOT 밖 host code에서 shutdown interrupt callback이 복귀를 거절할 때 native
R15를 포함한 전체 host register state를 변경하지 않아 execution-timeout 종료의
SIGSEGV를 제거합니다.

## 작업 범위

1. callback 반환값으로 native context write-back 필요 여부를 명시합니다.
2. sampling 또는 복귀 거절 callback은 native context write-back을 생략합니다.
3. 실제 register 편집 callback의 기존 write-back은 유지합니다.
4. host-thread probe에 no-op 보존 회귀 검증을 추가합니다.
5. Linux x64 빌드, 전체 core probe, 실제 timeout 종료로 검증합니다.
6. 아키텍처, 누적 분석, 작업 로그를 갱신합니다.

## 제외 범위

- shutdown recovery 대상 범위 확대
- signal 종류 또는 retry/timeout 정책 변경
- guest R15D stack ABI 변경
- 강제 thread termination 추가

---

## English

### Objective

Preserve the complete native host register state, including R15, when a
shutdown interrupt callback refuses recovery outside guest/AOT code, removing
the execution-timeout SIGSEGV.

### Scope

Add an explicit callback result for native write-back; skip write-back for
sampling and refused recovery; retain write-back for real edits; add host-thread
regression coverage; verify Linux x64 builds, all core probes, and real timeout
shutdown; and update architecture, cumulative analysis, and the work log.

### Out of scope

Expanding recoverable shutdown regions, changing signal or retry policy,
changing the guest R15D stack ABI, or adding forced thread termination.
