# DS zero-page dword load 작업 지시

## 목표

guest `DS`가 활성화된 `8B /r`의 첫 4 KiB 저메모리 dword read를 zero-backed HLE로 처리한다.

## 작업

* 설계 조건을 `HandleTracedMemoryLoadInstruction`에 추가한다.
* arena와 shadow-memory read를 우선 유지한다.
* 테스트 관측점을 다음 blocker에 맞게 갱신한다.
* 빌드와 전체 테스트를 실행한다.
* 결과를 작업 로그에 기록한다.

# DS Zero-Page Dword Load Work Order

## Goal

Handle a dword read from the first 4 KiB of low memory through zero-backed HLE for `8B /r` while guest `DS` is active.

## Tasks

* Add the designed condition to `HandleTracedMemoryLoadInstruction`.
* Preserve real-arena and shadow-memory reads as higher priorities.
* Update the test observation point for the next blocker.
* Run the build and full tests.
* Record the result in the work log.
