# loader 로그 포맷 작업 지시

## 작업 항목

1. Win32 loader logger pattern 위치를 확인한다.
2. pattern을 `[%X.%e] [%8l] [%n] %v`로 변경한다.
3. 기존 warn/error 분류 정책을 유지한다.
4. `scripts/test_all.ps1`로 전체 검증을 수행한다.
5. 작업 로그를 남긴다.

## 비목표

* 새 로그 backend 추가
* 로그 레벨 분류 재조정
* 실행 동작 변경

# Loader Log Format Work Order

## Tasks

1. Locate the Win32 loader logger pattern.
2. Change the pattern to `[%X.%e] [%8l] [%n] %v`.
3. Preserve the existing warn/error classification policy.
4. Run full verification with `scripts/test_all.ps1`.
5. Leave a work log.

## Non-Goals

* Adding a new log backend.
* Reclassifying log levels.
* Changing execution behavior.
