# memory-source segment register load HLE 작업 지시

## 목표

`piu_1st`가 `66 8E 05 E4 65 1A 02` (`MOV Sreg, r/m16`)에서 멈추지 않고, relocated memory source에서 selector를 읽어 guest segment shadow state에 기록한 뒤 다음 명령으로 계속 진행하게 한다.

## 작업 항목

1. segment load HLE가 instruction prefix를 건너뛰고 `8E /r`를 디코딩하게 한다.
2. 기존 register-source `mod=3` 처리 경로를 유지한다.
3. memory-source `mod=00`, `r/m=5`, disp32 처리 경로를 추가한다.
4. relocated runtime memory에서 16-bit selector를 읽어 guest segment shadow state에 기록한다.
5. loader 로그에 마지막 segment load source address를 출력한다.
6. `piu_1st`와 `dos4gw_hello` 실행으로 회귀를 확인한다.

## 제한

* 실제 host segment register는 변경하지 않는다.
* 이번 작업은 현재 trace에서 확인된 absolute displacement memory source 형태만 추가한다.
* SIB, displacement8/32 base-relative addressing, descriptor 권한 검사는 이후 단계에서 확장한다.

# Memory-Source Segment Register Load HLE Work Order

## Goal

Allow `piu_1st` to continue past `66 8E 05 E4 65 1A 02` (`MOV Sreg, r/m16`) by reading the selector from relocated memory source and recording it in guest segment shadow state.

## Tasks

1. Let segment load HLE skip instruction prefixes before decoding `8E /r`.
2. Preserve the existing register-source `mod=3` path.
3. Add the memory-source `mod=00`, `r/m=5`, disp32 path.
4. Read a 16-bit selector from relocated runtime memory and record it in guest segment shadow state.
5. Print the last segment load source address in loader logs.
6. Verify regression behavior with `piu_1st` and `dos4gw_hello`.

## Limits

* Do not modify real host segment registers.
* This task only adds the absolute displacement memory source form seen in the current trace.
* SIB, displacement8/32 base-relative addressing, and descriptor permission checks are left for later steps.
