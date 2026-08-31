# 20260901-558 x64에서 guest 상태를 어디에 두는가 작업 지시서

## 한국어

### 목적

guest register mapping과 guest `ESP`의 거처를 결정으로 확정하고, 방출된 바이트를
**실행해서** 확인합니다. 설계는
[20260901-558](../design/20260901-558-x64-guest-register-placement.md)입니다.

### 작업

- mapping을 헤더로 남긴다. guest GPR 번호 ↔ host GPR 번호, 그리고 guest `ESP`가 `R15`라는
  것을 상수와 static assertion으로 적는다. 주석이 아니라 코드가 말하게 한다.
- x64 전용 probe를 추가한다.
  - `kCopy`만으로 plan을 만든다. 복사·`0x67` lowering·`FF /0` 재인코딩이 모두 들어가고,
    일곱 개 host register를 모두 건드린다.
  - `enable_long_mode_emission`으로 방출하고, 마지막 `kCopy`까지의 바이트를 하위 4 GiB
    실행 페이지에 복사한 뒤 `ret`을 붙인다.
  - mapping대로 register를 채우고 `R15D`에 guest `ESP`를 넣어 호출한다.
  - guest가 뜻한 결과와, `R15`가 그대로이고 상위 절반이 0인지 확인한다.
- 실행 결과를 값으로 출력한다. true/false만으로는 무엇이 틀렸는지 알 수 없다.

### 검증

- **Linux x64**: 새 probe 포함 `core_probe_all=true`.
- **Linux i386 · Win32 x86**: 기존 probe 전부 통과. 새 probe는 빌드되지 않으므로
  `core_probe_skipped`에 나타나야 한다.
- 방출 경로는 바뀌지 않으므로 census 숫자는 그대로여야 한다.

## English

### Objective

Settle the guest register mapping and guest `ESP`'s home as decisions, and confirm them by
**executing** emitted bytes. The design is
[20260901-558](../design/20260901-558-x64-guest-register-placement.md).

### Work items

- Put the mapping in a header: guest GPR number to host GPR number, and guest `ESP` in
  `R15`, as constants and static assertions. Let the code say it rather than a comment.
- Add an x64-only probe.
  - Build a `kCopy`-only plan covering a copy, a `0x67` lowering and an `FF /0`
    re-encoding, touching all seven mapped host registers.
  - Emit with `enable_long_mode_emission`, copy the bytes up to the last `kCopy` onto an
    executable page below 4 GiB, and append a `ret`.
  - Load the registers per the mapping, put guest `ESP` in `R15D`, and call it.
  - Check the guest's intended results, and that `R15` is unchanged with a zero upper half.
- Print the observed values, not only pass or fail. A boolean cannot say what went wrong.

### Verification

- **Linux x64**: `core_probe_all=true` including the new probe.
- **Linux i386 and Win32 x86**: every existing probe still passes, and the new probe is not
  built, so it must appear in `core_probe_skipped`.
- The emission path does not change, so the census numbers must be unchanged.
