# segment register store HLE 작업 지시

## 목표

`piu_1st`가 `66 26 8C 1D ED 3A 0F 02` (`MOV r/m16, Sreg`)에서 멈추지 않고, guest selector shadow state의 값을 relocated runtime memory에 기록한 뒤 다음 명령으로 계속 진행하게 한다.

## 작업 항목

1. privileged instruction classifier가 prefix 뒤의 `MOV r/m16, Sreg`를 분류하게 한다.
2. Win32 execution trampoline에 segment store 처리 상태와 로그 필드를 추가한다.
3. `66 26 8C /r` 중 absolute displacement memory destination 형태를 디코딩한다.
4. source selector를 guest segment shadow state에서 읽어 relocated memory에 16-bit 값으로 기록한다.
5. `piu_1st`와 `dos4gw_hello` 실행으로 회귀를 확인한다.

## 제한

* 실제 host segment register는 변경하지 않는다.
* 이번 작업은 현재 trace에서 확인된 absolute displacement memory destination 형태만 처리한다.
* selector/descriptor 권한 검사는 이후 DPMI descriptor API 연결 단계에서 확장한다.

# Segment Register Store HLE Work Order

## Goal

Allow `piu_1st` to continue past `66 26 8C 1D ED 3A 0F 02` (`MOV r/m16, Sreg`) by writing the guest selector shadow-state value to relocated runtime memory.

## Tasks

1. Teach the privileged instruction classifier to classify `MOV r/m16, Sreg` after instruction prefixes.
2. Add segment store handling state and log fields to the Win32 execution trampoline.
3. Decode the absolute-displacement memory destination form of `66 26 8C /r`.
4. Read the source selector from guest segment shadow state and write it as a 16-bit value to relocated memory.
5. Verify regression behavior with `piu_1st` and `dos4gw_hello`.

## Limits

* Do not modify real host segment registers.
* This task handles only the absolute-displacement memory destination form seen in the current trace.
* Selector/descriptor permission checks are left for the later DPMI descriptor API connection step.
