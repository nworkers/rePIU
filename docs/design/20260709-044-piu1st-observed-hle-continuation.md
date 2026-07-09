# piu_1st 관측 기반 HLE 연속 진행 설계

## 배경

`piu_1st`는 현재 `INT 21h AH=0x4A`에서 중단된다. 직전 byte pattern은 `B4 4A CD 21`이고 예외 시점의 `EAX=0x00004A2B`이므로 DOS memory block resize 계열 호출로 볼 수 있다.

사용자는 더 이상 진행할 수 없는 지점이 나올 때까지 남은 opcode와 HLE 요구사항을 계속 처리하기를 요청했다. 따라서 앞으로의 작업 단위는 새 정지점이 나올 때마다 별도 큰 구조를 만들기보다, 현재 trace에서 관측된 함수와 opcode만 좁게 추가하는 방식으로 진행한다.

## 설계 원칙

* 원본 실행 경로를 유지하고, 게임 로직은 재구현하지 않는다.
* 전체 DOS HLE를 `piu_1st`에 켜지 않는다.
* 일반 DOS HLE에 이미 같은 최소 응답이 있고 현재 trace와 맞는 경우에만 trace 기반 핸들러로 옮긴다.
* 의미가 불명확한 메모리 변경, 파일 I/O, 장치 상태, descriptor state가 필요해지면 진행을 멈추고 분석 결과를 보고한다.
* 새 정지점은 작업 로그와 `docs/TODO.md`에 누적한다.

## 현재 처리 후보

`INT 21h AH=0x4A`는 DOS memory block resize 함수이다. 현재 일반 DOS HLE는 이 호출에 대해 carry flag만 clear하고 성공으로 통과시킨다. `piu_1st` trace 기반 경로에서도 같은 최소 응답을 적용해 다음 지점을 관찰한다.

## 검증

각 처리 후 다음 검증을 수행한다.

* `scripts\test_all.ps1`
* 의미 있는 공통 실행 경로 변경이면 `scripts\test_openwatcom_samples.ps1 -CompareBaseline`

# piu_1st Observed HLE Continuation Design

## Background

`piu_1st` currently stops at `INT 21h AH=0x4A`. The preceding byte pattern is `B4 4A CD 21`, and exception-time `EAX=0x00004A2B`, so this is treated as a DOS memory block resize call.

The user asked to keep handling remaining opcodes and HLE requirements until a point is reached where progress is no longer possible. Therefore, this work proceeds by narrowly adding only functions and opcodes observed in the current trace, rather than introducing a new broad structure at every stop.

## Design Principles

* Preserve the original execution path and do not reimplement game logic.
* Do not enable the full DOS HLE path for `piu_1st`.
* Move behavior into the trace-based handler only when the general DOS HLE already has the same minimal response and it matches the current trace.
* Stop and report the analysis when progress requires unclear memory mutation, file I/O, device state, or descriptor state.
* Accumulate new stops in the work log and `docs/TODO.md`.

## Current Candidate

`INT 21h AH=0x4A` is the DOS memory block resize function. The current general DOS HLE treats this as success by clearing the carry flag. Apply the same minimal response to the `piu_1st` trace-based path and observe the next point.

## Verification

After each handling step, run:

* `scripts\test_all.ps1`
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline` when a shared execution path changes meaningfully
