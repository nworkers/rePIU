# 작업 로그 20260915-681 — Linux x64 16비트 LEA lowering

## 결과

16비트 LE code object에서 명시적인 `67 66 LEA r32,m32` 형식을 공용
mode-aware classifier/lowerer로 처리했습니다. 원본 guest bytes는 변경하지
않고 x64 AOT cache에서만 `66`을 제거하고 guest ESP를 R15로 remap합니다.

## 구현 및 검증

* `LongModeLowering::k16BitLea32ToGuestGprs`를 추가했습니다.
* `67 66 8D 8C 24 00 E0 FF FF`를
  `67 41 8D 8C 27 00 E0 FF FF`로 변환합니다.
* ESP가 없는 32비트 addressing form도 같은 규칙으로 분류할 수 있도록
  classifier 조건을 공통 prefix/operand/address 검증으로 구성했습니다.
* compatibility, lowering, emission regression을 추가했습니다.
* Linux x64 debug 빌드와 `repiu_core_probe`를 실행했으며 결과는
  `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`입니다.
* lowering probe는 실제 x64 실행 후 `ECX=0x30000000`을 관측했습니다.

## 실행 trace에서 확인된 다음 frontier

최신 debug 실행 파일로 object 3 trace를 확인한 결과, 실제 동적 요청은
`0x0110000E`의 `8D 8C 24 00`이었습니다. 이 바이트는 mode16에서
`LEA CX,[SI+disp16]`로 해석되며, 현재 Task 681의 `67 66` 형식과는
다릅니다. 이 명령은 x64 cache에서 INT3 boundary가 되었고, mode16
비동일 명령을 HLE가 처리하지 못해 기존의 `SIGTRAP`/coredump frontier가
재현되었습니다.

따라서 Task 681은 특정 주소 예외를 추가하지 않고 계획한 32비트 명시형
LEA lowering을 완료한 것으로 기록합니다. 실제 다음 작업은 16비트 주소
계산을 보존하는 별도 Task 682에서 진행합니다.

## 남은 범위

16비트 주소 계산, 16비트 목적지 LEA, PUSH/POP, MOV SS, far return 및
일반적인 16비트 stack ABI는 아직 해결하지 않았습니다.

## Git 상태

작업은 `work/20260913-678-linux-x64-coredump-investigation`에서 수행했으며,
`main`에는 merge하지 않았습니다.

---

# Work Log 20260915-681 — Linux x64 16-bit LEA lowering

## Result

The shared mode-aware classifier/lowerer now handles the explicit
`67 66 LEA r32,m32` form in a 16-bit LE code object. Original guest bytes are
unchanged; only the x64 AOT cache removes `66` and remaps guest ESP to R15.

## Implementation and verification

* Added `LongModeLowering::k16BitLea32ToGuestGprs`.
* Lowered `67 66 8D 8C 24 00 E0 FF FF` to
  `67 41 8D 8C 27 00 E0 FF FF`.
* Kept the classifier based on shared prefix, operand-width, and address-width
  checks so forms without ESP use the same policy.
* Added compatibility, lowering, and emission regressions.
* The Linux x64 debug build and `repiu_core_probe` passed with
  `core_probe_total=27`, `core_probe_failures=0`, and `core_probe_all=true`.
* The lowering probe executed the emitted x64 bytes and observed
  `ECX=0x30000000`.

## Next frontier observed in the runtime trace

The latest debug run requested `8D 8C 24 00` at `0x0110000E`. In mode16 this
is `LEA CX,[SI+disp16]`, not the Task 681 `67 66` form. It became an INT3
boundary in the x64 cache, and the existing SIGTRAP/coredump frontier was
reproduced because HLE does not execute this non-identical mode16 instruction.

Task 681 therefore completes its planned shared 32-bit-explicit LEA lowering
without an address-specific exception. Task 682 separately designs the
16-bit address-calculation lowering needed by the observed frontier.

## Remaining scope

16-bit address calculation, 16-bit destination LEA, PUSH/POP, MOV SS, far
return, and a general 16-bit stack ABI remain unresolved.

## Git state

Work was performed on `work/20260913-678-linux-x64-coredump-investigation` and
was not merged into `main`.
