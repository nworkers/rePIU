# Task 733: Win32 legacy shutdown crash 귀속 작업 지시

## 한국어

1. 현재 branch와 clean worktree를 확인한다.
2. 최신 Win32 x86 Debug loader에서 간헐 `0xC0000005`를 재현한다.
3. 설계에 따라 opt-in pre-resume shutdown recovery trace를 추가한다.
4. 빌드 후 crash 실행의 마지막 상태를 수집해 원인을 특정한다.
5. 원인 범위 안에서 수정하고 반복 실행으로 crash가 사라지는지 검증한다.
6. `scripts/test_all.ps1`의 오래된 pumpit1 단정을 현재의 안정된 계약으로 갱신한다.
7. 관련 analysis, architecture, 작업 로그를 갱신하고 커밋한다.

## English

1. Confirm the current branch and clean worktree.
2. Reproduce the intermittent `0xC0000005` with the current Win32 x86 Debug loader.
3. Add the opt-in pre-resume shutdown recovery trace described by the design.
4. Build and collect the final state of a crashing run to identify the cause.
5. Fix the cause within scope and verify that repeated runs no longer crash.
6. Update stale pumpit1 assertions in `scripts/test_all.ps1` to the current stable contract.
7. Update the relevant analysis, architecture, and work log, then commit the task.
