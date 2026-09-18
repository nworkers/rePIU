# Task 703 작업 지시 — Linux x64 타이머 체인 경계 AOT 재진입

## 목표

`HandleTimerInterruptChainBoundary`가 성공한 뒤 Linux x64 AOT 실행을 guest
continuation의 cache entry로 복귀시켜, 다음 direct `CALL`의 guest return push를
보존합니다.

## 작업 범위

1. 타이머 체인 handler 진입 전 guest EIP를 보존합니다.
2. handler가 EIP를 진행시킨 Linux x64 AOT 경로에서
   `TryResumeAotAfterHandledHle(..., kHandledGuestBoundary)`를 호출합니다.
3. 재진입 실패 시 continuation의 long-mode 호환성을 검사하고 비호환 경로를
   fail-closed로 유지합니다.
4. 아키텍처, 누적 Linux frontier, 작업 로그를 갱신합니다.
5. core probe, 빌드, 실제 `pumpit2a` 추적으로 검증합니다.

## 제외 범위

* 타이머 체인 인식 조건과 EFLAGS 폐기 규칙 변경
* 진짜 이전 INT 8 handler 실행 구현
* LINEXE far-transfer 경계 정책 변경
* 원본 게임 코드 수정

---

## English

### Objective

After `HandleTimerInterruptChainBoundary` succeeds, return Linux x64 AOT
execution to the continuation's cache entry so the following direct `CALL`
preserves its guest return-address push.

### Scope

Capture the pre-handler guest EIP, invoke the shared handled-boundary AOT resume
after an advancing timer-chain handler, fail closed for an incompatible
continuation when resume fails, update architecture/frontier/work-log
documentation, and verify with the core probe, build, and a real `pumpit2a`
trace.

### Out of scope

Timer-chain recognition and EFLAGS cleanup, execution of a real predecessor
INT 8 handler, LINEXE far-transfer policy, and changes to original game code.
