# Task 636 작업 지시: Linux x64 GS base/no-displacement segment override

설계: [20260908-636](../design/20260908-636-linux-x64-gs-base-no-displacement.md)

## 한국어

### 구현 단계

1. `LongModeSegmentOverrideEmittable`에서 GS(`segment_register=5`, prefix
   `0x65`)를 허용하되 FS는 계속 거부합니다.
2. 기존 absolute disp32와 base+disp8 외에 non-SIB `mod=00`, `rm!=4/5`,
   displacement size 0 형식을 허용합니다.
3. `EmitLongModeSegmentOverride`에서 세 주소 형식을 명시적으로 구분합니다.
   * no-displacement 형식의 `original_displacement`는 0
   * absolute 여부는 `mod=00 && rm=5`로 판정
   * no-displacement suffix는 ModRM 바로 뒤에서 복사
   * base 형식은 ModRM을 `mod=10`으로 넓혀 disp32 slot 생성
4. Linux x64 guest-register probe에 GS CMP/MOV 형식을 추가하고 selector
   일치, 불일치, HLE-to-native 복원 경로를 실행 검증합니다.
5. `ARCHITECTURE.md`와 `docs/analysis/linux-port-frontier.md`를 갱신하고 작업
   로그를 작성합니다.

### 금지 사항

* host FS/GS selector 또는 base를 변경하지 않습니다.
* FS, SIB, base+disp32 형식을 허용하지 않습니다.
* patcher의 `original_displacement + live base` 계약을 바꾸지 않습니다.
* i386 방출 경로를 바꾸지 않습니다.

### 검증

1. Linux x64 `repiu_core_probe`, `repiu`, `repiu_instruction_census` 빌드.
2. core probe에서 GS CMP/MOV, 기존 ES 형식, mismatch, 복원 경로 통과.
3. `pumpit2a` census에서 `agrees=true` 유지와 emitted/refused 변화 기록.
4. 실제 `pumpit2a` 실행에서 `0x010F06D0` fault 해소와 다음 frontier 기록.

## English

### Implementation steps

1. Admit GS (`segment_register=5`, prefix `0x65`) in
   `LongModeSegmentOverrideEmittable` while continuing to refuse FS.
2. In addition to absolute disp32 and base+disp8, admit non-SIB `mod=00`,
   `rm!=4/5`, displacement-size-zero forms.
3. Distinguish all three address forms in `EmitLongModeSegmentOverride`:
   * use zero as `original_displacement` for no-displacement forms;
   * identify absolute form with `mod=00 && rm=5`;
   * copy a no-displacement suffix from immediately after ModRM; and
   * widen base forms to `mod=10` with a new disp32 slot.
4. Extend the Linux x64 guest-register probe with the GS CMP/MOV forms and run
   matching-selector, mismatching-selector, and HLE-to-native restoration paths.
5. Update `ARCHITECTURE.md` and `docs/analysis/linux-port-frontier.md`, then
   write the work log.

### Prohibited

* Do not change host FS/GS selectors or bases.
* Do not admit FS, SIB, or base+disp32 forms.
* Do not change the patcher's `original displacement + live base` contract.
* Do not change the i386 emission path.

### Verification

1. Build Linux x64 `repiu_core_probe`, `repiu`, and
   `repiu_instruction_census`.
2. Pass the core probe's GS CMP/MOV, existing ES forms, mismatch, and restore
   paths.
3. Keep `agrees=true` in the `pumpit2a` census and record emitted/refused
   changes.
4. Run real `pumpit2a`, confirm the `0x010F06D0` fault is gone, and record the
   next frontier.
