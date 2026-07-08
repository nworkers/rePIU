# privileged instruction 분류 작업 지시

## 목표

`0x020F3890`에서 발생하는 privileged instruction 예외를 다음 구현 작업의 입력으로 사용할 수 있게 구조화해서 분류한다.

## 범위

1. HLE 모듈에 privileged instruction classifier 구조와 함수를 추가한다.
2. loader가 예외 byte window를 얻은 뒤 classifier 결과를 출력한다.
3. `docs/TODO.md`에 이번 분류 작업의 완료 상태와 다음 작업을 반영한다.
4. Win32 x86 loader와 `dos4gw_hello` 경로를 검증한다.
5. 작업 로그를 작성한다.

## 제외

* 이번 작업에서는 명령을 실제로 에뮬레이션하지 않는다.
* HLE dispatcher의 handler 호출/복귀 규약은 다음 작업으로 둔다.
* DPMI descriptor API 구현은 다음 작업으로 둔다.

# Privileged Instruction Classification Work Order

## Goal

Classify the privileged-instruction exception at `0x020F3890` in a structured way so it can drive the next implementation step.

## Scope

1. Add privileged instruction classifier structures and functions to the HLE module.
2. Print classifier results after the loader builds the exception byte window.
3. Update `docs/TODO.md` with this classification completion state and the next tasks.
4. Verify the Win32 x86 loader and the `dos4gw_hello` path.
5. Write the work log.

## Out Of Scope

* This task does not emulate the instruction.
* The HLE dispatcher handler call/return convention remains a later task.
* DPMI descriptor API implementation remains a later task.
