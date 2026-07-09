# DOS Current Directory HLE 작업 지시

## 목표

`INT 21h AH=0x3B`을 처리해 `piu_1st`가 current-directory 변경 요청 이후의 다음 실행 지점까지 진행하게 한다.

## 작업 범위

* 공용 HLE 계층에 DOS 가상 current directory 상태와 path resolver를 추가한다.
* Win32 execution trampoline이 target working directory를 DOS 루트로 받아 실행 thread context에 보관하게 한다.
* `INT 21h AH=0x3B`에서 guest ASCIZ 경로를 읽고 가상 current directory를 갱신한다.
* host process current directory는 변경하지 않는다.
* loader 로그에 마지막 DOS `chdir` 요청, resolved host path, 결과를 출력한다.
* `scripts\test_all.ps1`의 `piu_1st` 기대 지점을 새 중단 지점으로 갱신한다.

## 제외 범위

* `open/read/findfirst/getcwd` 같은 후속 파일 API는 구현하지 않는다.
* DOS drive별 current directory table은 아직 구현하지 않는다.
* host filesystem write 동작은 추가하지 않는다.

## 검증

* `scripts\test_all.ps1`
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`

# DOS Current Directory HLE Work Order

## Goal

Handle `INT 21h AH=0x3B` so `piu_1st` can advance to the next execution point after the current-directory change request.

## Scope

* Add DOS virtual current-directory state and a path resolver to the shared HLE layer.
* Pass the target working directory into the Win32 execution trampoline as the DOS root.
* Read the guest ASCIZ path in `INT 21h AH=0x3B` and update the virtual current directory.
* Do not change the host process current directory.
* Print the last DOS `chdir` request, resolved host path, and result in loader logs.
* Update the `piu_1st` expectation in `scripts\test_all.ps1` to the new stop.

## Out of Scope

* Do not implement later file APIs such as `open`, `read`, `findfirst`, or `getcwd`.
* Do not implement per-drive DOS current-directory tables yet.
* Do not add host filesystem write behavior.

## Verification

* `scripts\test_all.ps1`
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`
