# Runtime Memory Arena 작업 지시

## 목표

`INT 21h AH=0x4A` 이후 원본 런타임이 접근하는 relocated placement 바깥 영역을 안전하게 다루기 위한 최소 runtime memory arena 구조를 추가한다.

## 작업 범위

* 공용 runtime 계층에 `RuntimeMemoryArenaPlan`과 builder를 추가한다.
* 초기 expansion slack은 `0x00010000`으로 둔다.
* Win32 loader가 relocated base 선택과 image placement에 arena reserve size를 사용하게 한다.
* loader 로그에 arena base, image reserve, expansion slack, arena reserve, arena end를 출력한다.
* `piu_1st`가 기존 `0x020F8405` write blocker를 통과하는지 확인하고 새 blocker를 기록한다.

## 제외 범위

* `INT 21h AH=0x4A`의 전체 DOS MCB semantics는 구현하지 않는다.
* resize 실패 정책은 아직 구현하지 않는다.
* 파일/장치 I/O나 descriptor table 일반화는 다루지 않는다.

## 검증

* `scripts\test_all.ps1`
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`

# Runtime Memory Arena Work Order

## Goal

Add a minimal runtime memory arena structure so the project can safely handle accesses beyond the current relocated placement after `INT 21h AH=0x4A`.

## Scope

* Add `RuntimeMemoryArenaPlan` and a builder to the shared runtime layer.
* Use `0x00010000` as the initial expansion slack.
* Make the Win32 loader use the arena reserve size for relocated base selection and image placement.
* Log arena base, image reserve, expansion slack, arena reserve, and arena end.
* Check whether `piu_1st` passes the existing `0x020F8405` write blocker and record the new blocker.

## Out of Scope

* Do not implement full DOS MCB semantics for `INT 21h AH=0x4A`.
* Do not implement resize failure policy yet.
* Do not handle file/device I/O or generalized descriptor tables.

## Verification

* `scripts\test_all.ps1`
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`
