# piu_1st 관측 기반 HLE 연속 진행 작업 지시

## 목표

`piu_1st`에서 새로 관측되는 opcode와 HLE 요구사항을 더 이상 안전하게 진행할 수 없는 지점까지 순차적으로 처리한다.

## 작업 범위

* 현재 정지점 `INT 21h AH=0x4A`를 trace 기반 핸들러에 추가한다.
* 각 처리 후 `piu_1st`를 실행해 다음 정지점을 확인한다.
* 일반 DOS HLE에 이미 존재하는 최소 응답과 일치하는 경우에만 trace 기반 경로에 추가한다.
* 새 관측점과 중단 사유를 작업 로그에 기록한다.
* `test_all.ps1`의 기대 관측점을 최신 지점으로 갱신한다.

## 중단 조건

다음 중 하나가 필요하면 구현을 멈추고 사용자에게 보고한다.

* 의미를 확정할 수 없는 메모리 쓰기
* 실제 파일 시스템이나 장치 상태 의존
* selector/descriptor 변환의 일반화
* 원본 실행파일 코드 패치
* 게임 로직 재구현

## 검증

* `scripts\test_all.ps1`
* 공통 실행 경로 변경 시 `scripts\test_openwatcom_samples.ps1 -CompareBaseline`

# piu_1st Observed HLE Continuation Work Order

## Goal

Sequentially handle newly observed `piu_1st` opcodes and HLE requirements until no further safe progress can be made.

## Scope

* Add the current stop, `INT 21h AH=0x4A`, to the trace-based handler.
* Run `piu_1st` after each change to observe the next stop.
* Add behavior to the trace-based path only when it matches an existing minimal response in the general DOS HLE path.
* Record new observation points and stop reasons in the work log.
* Update the expected observation point in `test_all.ps1`.

## Stop Conditions

Stop implementation and report to the user if progress requires any of the following.

* Memory writes whose meaning cannot be established
* Real filesystem or device state dependency
* Generalized selector/descriptor translation
* Original executable patching
* Game logic reimplementation

## Verification

* `scripts\test_all.ps1`
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline` when a shared execution path changes
