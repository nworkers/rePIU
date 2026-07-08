# segment register load HLE 작업 지시

## 목표

`piu_1st`가 `8E D9` (`MOV DS, CX`)에서 멈추지 않고, guest selector 상태를 HLE로 기록한 뒤 다음 명령으로 계속 진행하게 한다.

## 범위

1. privileged instruction classifier에 `MOV Sreg, r/m16` 분류를 추가한다.
2. Win32 execution trampoline에 segment load HLE 플래그를 추가한다.
3. `8E /r` 중 register source 형태를 디코딩하고 guest segment selector를 기록한다.
4. execution attempt와 loader 로그에 segment load 처리 정보를 출력한다.
5. `piu_1st`와 `dos4gw_hello`를 검증한다.
6. TODO와 작업 로그를 갱신한다.

## 제외

* 실제 host segment register를 변경하지 않는다.
* descriptor validation과 limit/permission check는 다음 작업으로 둔다.
* memory operand 형태의 `8E /r`는 이번 작업에서 처리하지 않는다.

# Segment Register Load HLE Work Order

## Goal

Allow `piu_1st` to continue past `8E D9` (`MOV DS, CX`) by recording guest selector state through HLE.

## Scope

1. Add `MOV Sreg, r/m16` classification to the privileged instruction classifier.
2. Add a segment load HLE flag to the Win32 execution trampoline.
3. Decode `8E /r` register-source forms and record guest segment selectors.
4. Print segment load handling information in the execution attempt and loader logs.
5. Verify `piu_1st` and `dos4gw_hello`.
6. Update TODO and the work log.

## Out Of Scope

* Do not modify the real host segment register.
* Leave descriptor validation and limit/permission checks for a later task.
* Do not handle memory-operand `8E /r` forms in this task.
