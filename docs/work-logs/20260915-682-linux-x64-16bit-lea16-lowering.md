# 작업 로그 20260915-682 — Linux x64 16비트 주소 LEA lowering

## 결과

실제 object 3 frontier인 `8D 8C 24 00`을 공용 mode16 LEA16 lowering으로
처리했습니다. source low word를 scratch에 materialize한 뒤 x64의 32비트
address-size LEA와 16비트 destination write를 사용하여 원본 16비트
effective-address 및 flags semantics를 보존합니다.

## 구현 및 검증

* SI/DI/BX 기반 mode16 address form과 displacement subset을 분류합니다.
* BP 기반 form과 segment override는 default SS/base semantics가 확정되지
  않았으므로 계속 fail-closed합니다.
* `R14D`와 `R10D` scratch, `MOVZX`, `LEA`만 사용하여 flags를 변경하지
  않습니다.
* 실제 frontier `8D 8C 24 00`은
  `44 0F B7 F6 67 66 41 8D 8E 24 00 00 00`으로 lower됩니다.
* compatibility, lowering, emission probe가 모두 통과했습니다.
* lowering probe는 low-word 결과와 destination upper word, ZF 보존을 실제
  x64에서 확인했습니다.
* Linux x64 core probe 결과는 `core_probe_total=27`,
  `core_probe_failures=0`, `core_probe_all=true`입니다.

## Runtime 결과

최신 debug trace에서 `0x0110000E`의 plan은 mode16 length 4로 기록되었고,
dynamic image는 `guest_length=4`, `emitted_length=13`을 기록했습니다.
따라서 해당 frontier의 INT3 boundary는 제거되었습니다. 다음 중단 지점은
`0x01100012: E0 FF`, mode16 `LOOPNZ`로 이동했습니다.

## 남은 범위

mode16 LOOP control-flow semantics와 이후 `FF` stack opcode, 16비트
PUSH/POP, MOV SS, far return 및 일반 stack ABI는 별도 작업으로 남깁니다.

## Git 상태

Task 681과 함께 `work/20260913-678-linux-x64-coredump-investigation`에서
커밋하며 `main`에는 merge하지 않습니다.

---

# Work Log 20260915-682 — Linux x64 16-bit address LEA lowering

## Result

The actual object-3 frontier `8D 8C 24 00` is now handled by a shared mode16
LEA16 lowering. The source low word is materialized in scratch and the result
uses x64 32-bit addressing with a 16-bit destination write, preserving the
guest effective address and flags semantics.

## Implementation and verification

* Admit the proven SI/DI/BX-based mode16 address and displacement subset.
* Keep BP-based forms and segment overrides fail-closed until default
  SS/base semantics are established.
* Use only R14D/R10D scratch, MOVZX, and LEA so flags remain unchanged.
* Lower the frontier to
  `44 0F B7 F6 67 66 41 8D 8E 24 00 00 00`.
* Compatibility, lowering, and emission probes pass.
* The lowering probe verifies low-word result, destination upper word, and ZF
  preservation on x64.
* The Linux x64 core probe reports `core_probe_total=27`,
  `core_probe_failures=0`, and `core_probe_all=true`.

## Runtime result

The latest debug trace records `0x0110000E` as a mode16 length-4 plan entry,
and the dynamic image reports `guest_length=4`, `emitted_length=13`. The INT3
boundary is therefore gone. The next stop moved to
`0x01100012: E0 FF`, mode16 `LOOPNZ`.

## Remaining scope

Mode16 LOOP control-flow semantics, the following `FF` stack opcode, 16-bit
PUSH/POP, MOV SS, far return, and a general stack ABI remain separate work.

## Git state

This task is committed with Task 681 on
`work/20260913-678-linux-x64-coredump-investigation`; it is not merged into
`main`.
