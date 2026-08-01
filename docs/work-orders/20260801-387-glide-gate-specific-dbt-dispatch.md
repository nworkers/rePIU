# 20260801-387 Glide Gate 전용 직접 Dispatch 작업 지시 / Work Order

## 한국어

1. 합성 Glide gate image를 opt-in에서만 `CALL thunk + RET imm16`으로 변환하는 Win32 전용 모듈을 추가합니다.
2. 자산 유래 주소, ordinal, 인자 크기 및 기존 `UD2 + ordinal + RET` 바이트를 변환 전에 검증합니다.
3. 전용 host-stack thunk가 `HandleGlideGateBoundary`를 호출하고 EIP/ESP ABI를 검증하도록 합니다.
4. 번역된 복귀 target 또는 기존 TF one-step bridge를 원 guest return slot에 기록합니다.
5. 상태와 direct entry/success/fallback/terminal 계측을 추가합니다.
6. layout과 invalid-byte 비변경 synthetic probe를 추가합니다.
7. Release 빌드, 전체 probe, 동일 EEPROM baseline/opt-in 스모크를 수행합니다.
8. 분석과 작업 로그를 갱신하고, 수동 Music Select 캡처 전까지 기본값은 OFF로 유지합니다.

## English

1. Add a Win32-specific module that transforms synthetic Glide gates to `CALL thunk + RET imm16` only under opt-in.
2. Validate asset-derived address, ordinal, argument size, and original `UD2 + ordinal + RET` bytes before mutation.
3. Have a dedicated host-stack thunk call `HandleGlideGateBoundary` and validate EIP/ESP ABI.
4. Write a translated return target or existing TF one-step bridge into the original guest return slot.
5. Add state and direct entry/success/fallback/terminal telemetry.
6. Add synthetic probes for layout and invalid-byte non-mutation.
7. Run Release builds, the full probe, and identical-EEPROM baseline/opt-in smokes.
8. Update analysis/work log and keep default OFF until a manual Music Select capture.
## 실행 경로 정정 / Execution-path correction

9. 자산에서 검증된 Glide 주소와 일치하는 미해결 AOT direct fixup만 실제 executable gate 주소로 연결합니다.
10. opt-in 게이트 페이지를 실행 전용으로 보호하고 instruction cache를 flush합니다.
11. fixup 연결 수, 게이트 이미지 검증 수, direct 진입/성공/target-miss를 검증합니다.

9. Resolve only unresolved AOT direct fixups that exactly match asset-derived Glide addresses to executable gate addresses.
10. Protect the opt-in gate page as executable and flush its instruction cache.
11. Verify fixup resolution count, gate-image verification count, and direct entry/success/target-miss telemetry.