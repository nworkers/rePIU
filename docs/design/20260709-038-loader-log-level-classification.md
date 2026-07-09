# loader 로그 레벨 분류 설계

## 배경

현재 Win32 loader 로그는 대부분 `info` 레벨로 출력된다.
이 때문에 정상 진행 상태, 예상 가능한 우회, 현재 구현이 막힌 지점이 한 흐름에 섞여 보인다.

특히 `piu_1st` 실행 결과에서 fixed low address reserve 실패는 relocated image 실행으로 이어지는 예상 가능한 우회인데, 실제 다음 구현 대상인 `26 8A 4F FF` 예외 지점과 같은 레벨로 출력된다.

## 목표

* 정상 진행 정보는 `info`로 유지한다.
* 현재 실행 경로에서 예상 가능한 제약이나 우회는 `warn`으로 출력한다.
* 사용자가 다음 구현 대상 또는 현재 blocker로 해석해야 하는 실행 중단점은 `error`로 출력한다.
* 프로세스 종료 정책은 바꾸지 않는다.
  현재 관찰 가능한 stop을 기록하고 exit code 0으로 끝나는 동작은 유지한다.

## 비목표

* HLE 처리 범위를 새로 늘리지 않는다.
* 예외 처리 정책이나 실행 trampoline 동작을 바꾸지 않는다.
* 로그 출력 포맷 전체를 재설계하지 않는다.

## 분류 규칙

`info`:

* target 선택
* runtime policy 값
* relocated image base 후보와 선택
* stack plan, dispatcher table, relocated placement 성공
* HLE 처리 count와 마지막 처리된 HLE/DOS/segment 이벤트
* 정상 반환된 guest output

`warn`:

* fixed original address range가 이미 점유되어 relocated execution으로 우회하는 경우
* fixed range reservation이 실패했지만 이후 relocated execution으로 계속 진행 가능한 경우

`error`:

* 실제 loader 단계 실패로 반환 코드 1을 내는 기존 오류
* original entry 실행 중 SEH 예외가 잡혀 더 진행하지 못하는 경우
* 예외 지점의 byte window와 분류 결과가 현재 미구현 명령 또는 memory access임을 나타내는 경우

## 검증

* `scripts/test_all.ps1`가 계속 성공해야 한다.
* `dos4gw_hello`는 정상 반환 경로이므로 blocker `error`가 없어야 한다.
* `piu_1st`는 현재 관찰 지점 `26 8A 4F FF`를 `error` 레벨 current blocker로 출력해야 한다.

# Loader Log Level Classification Design

## Background

Most Win32 loader logs currently use the `info` level.
This makes normal progress, expected fallback paths, and the current implementation blocker look like the same kind of event.

In particular, for `piu_1st`, fixed low address reservation failure is an expected fallback into relocated image execution, but it is printed at the same level as the real next implementation target, the `26 8A 4F FF` exception point.

## Goals

* Keep normal progress information at `info`.
* Print expected constraints or fallback paths in the current execution route as `warn`.
* Print execution stops that users should interpret as the next implementation target or current blocker as `error`.
* Do not change process exit policy.
  The current behavior of recording an observable stop and exiting with code 0 is preserved.

## Non-Goals

* Do not add new HLE coverage.
* Do not change exception handling policy or execution trampoline behavior.
* Do not redesign the whole log output format.

## Classification Rules

`info`:

* target selection
* runtime policy values
* relocated image base candidates and selected base
* successful stack plan, dispatcher table, and relocated placement
* HLE count and last handled HLE/DOS/segment events
* guest output from normal return paths

`warn`:

* fixed original address range is occupied and execution falls back to relocated execution
* fixed range reservation fails but execution can continue through relocated execution

`error`:

* existing loader-stage failures that return code 1
* SEH exception caught while executing the original entry, meaning execution cannot continue
* byte window and classification result at the exception point when they represent the current unimplemented instruction or memory access

## Verification

* `scripts/test_all.ps1` must continue to pass.
* `dos4gw_hello` should not print a blocker `error` because it returns normally.
* `piu_1st` should print the current observation point `26 8A 4F FF` as an `error` level current blocker.
