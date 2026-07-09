# loader 로그 레벨 분류 작업 지시

## 작업 항목

1. 현재 loader 로그 출력 위치를 확인한다.
2. fixed range probe/reservation 실패 중 relocated execution으로 계속 진행 가능한 항목을 `warn`으로 조정한다.
3. original entry 실행 중 잡힌 SEH 예외와 예외 위치 byte window를 `error`로 조정한다.
4. 예외 위치의 instruction classification이 unknown일 때 current blocker 메시지를 `error`로 출력한다.
5. `scripts/test_all.ps1`로 전체 검증을 수행한다.
6. 작업 로그를 남긴다.

## 비목표

* 새 HLE 명령 처리 구현
* trampoline 또는 예외 처리 정책 변경
* 프로세스 exit code 정책 변경

# Loader Log Level Classification Work Order

## Tasks

1. Inspect the current loader log output locations.
2. Change fixed range probe/reservation failures that can continue through relocated execution to `warn`.
3. Change SEH exceptions caught during original entry execution and the exception byte window to `error`.
4. Print an `error` level current blocker message when the instruction classification at the exception point is unknown.
5. Run full verification with `scripts/test_all.ps1`.
6. Leave a work log.

## Non-Goals

* Implementing new HLE instruction handling.
* Changing trampoline or exception handling policy.
* Changing process exit code policy.
