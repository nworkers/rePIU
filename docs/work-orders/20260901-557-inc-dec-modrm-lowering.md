# 20260901-557 `INC`/`DEC r32`를 ModRM 형태로 작업 지시서

## 한국어

### 목적

`INC`/`DEC r32` 805건을 `FF /0`·`FF /1`로 재인코딩합니다. 설계는
[20260901-557](../design/20260901-557-inc-dec-modrm-lowering.md)입니다.

### 작업

- `LongModeLowering`에 `kIncDecToModRm`을 추가한다.
- `IsSilentlyDifferentOpcode`에서 `0x40`–`0x4F`를 빼고, 판정기가 그 범위를 따로 다룬다.
  - stack pointer면 `kStackPointerRegister`로 거절한다. `r == 4`를 하드코딩하지 말고
    기존 `TouchesStackPointer`를 부른다.
  - prefix 없는 1바이트가 아니면 `kSilentlyDifferent`로 계속 거절한다.
  - 그 외에는 `kNeedsReencode` + `kIncDecToModRm`.
- `LowerLongModeBytes`가 `FF` + `C0+r`(INC) / `C8+r`(DEC)를 만든다.
- 판정기 probe: 여덟 register의 낮춘 바이트, `inc esp`·`dec esp` 거절, `66 40` 거절,
  그리고 **낮춘 바이트를 long mode 디코더로 다시 읽어 원래 뜻과 같은지** 확인한다.
- 방출 probe에 `INC`가 lowering으로 나오는 항목을 더한다.
- census를 다시 돌려 감소분을 기록한다.

### 검증

세 호스트 `repiu_core_probe` 전부 통과. 판정기는 호스트 무관이므로 출력이 같아야 한다.
census는 `refused`가 805 줄고 `lowered`가 805 늘어야 하며, 다르면 그 차이를 기록한다.
i386 경로는 `enable_long_mode_emission` 기본 `false`라 영향받지 않는다.

## English

### Objective

Re-encode the 805 `INC`/`DEC r32` instructions as `FF /0` and `FF /1`. The design is
[20260901-557](../design/20260901-557-inc-dec-modrm-lowering.md).

### Work items

- Add `kIncDecToModRm` to `LongModeLowering`.
- Remove `0x40`-`0x4F` from `IsSilentlyDifferentOpcode` and handle the range separately.
  - The stack pointer is refused with `kStackPointerRegister`. Call the existing
    `TouchesStackPointer` rather than hardcoding `r == 4`.
  - Anything that is not a prefix-free single byte stays refused as
    `kSilentlyDifferent`.
  - Otherwise `kNeedsReencode` plus `kIncDecToModRm`.
- `LowerLongModeBytes` produces `FF` plus `C0+r` (INC) or `C8+r` (DEC).
- Classifier probe: lowered bytes for all eight registers, `inc esp` / `dec esp` refused,
  `66 40` refused, and **the lowered bytes re-read with a long-mode decoder** to confirm
  they mean what the original meant.
- Add an emission-probe item showing `INC` coming out lowered.
- Re-run the census and record the drop.

### Verification

`repiu_core_probe` passes on all three hosts, with identical output because the classifier
is host-independent. The census should show `refused` down by 805 and `lowered` up by 805;
any difference is recorded. The i386 path is unaffected because
`enable_long_mode_emission` defaults to `false`.
