# DOS system date 설정 HLE 작업 로그

설계: [20260815-484-dos-set-system-date.md](../design/20260815-484-dos-set-system-date.md)

작업 지시: [20260815-484-dos-set-system-date.md](../work-orders/20260815-484-dos-set-system-date.md)

## 결과

- `repiu_log.txt`의 종료 원인을 `INT 21h AH=2Bh` 미구현으로 확정했습니다.
- 플랫폼 공용 DOS 날짜 검증·차이·이동·요일 모듈을 추가했습니다.
- 가상 DOS 날짜를 host local date 대비 일수 offset으로 실행 context에 저장합니다.
- Function 2Bh를 일반 DOS HLE와 traced/AOT 분기에 연결했습니다.
- Function 2Ah가 설정된 가상 날짜와 요일을 반환하도록 갱신했습니다.
- 호스트 운영체제의 실제 날짜는 변경하지 않습니다.
- 날짜 계약과 handler set/get round trip을 전체 AOT probe에 추가했습니다.

## 검증

- Win32 x86 Debug `repiu_aot_probe`, `repiu`: 빌드 성공
- Win32 x86 Release `repiu_aot_probe`, `repiu`: 빌드 성공
- Debug `pumpipx3` 전체 probe: `dos_date_probe=pass`,
  `glide_texture_table_stack_probe=pass`, `coherence_all=true`, 종료 코드 0
- Release `pumpipx3` 전체 probe: `dos_date_probe=pass`,
  `glide_texture_table_stack_probe=pass`, `coherence_all=true`, 종료 코드 0
- 첫 Debug 전체 빌드는 대규모 재컴파일 중 300초 제한에 도달했으나, 같은 빌드를 이어서
  완료했고 컴파일 오류는 없었습니다.
- `pumpitpr` image 자체는 기존 AOT cache probe 제한인
  `direct control-flow target is outside the cache`로 전체 probe 입력에 사용할 수 없었습니다.
  날짜 handler는 독립 probe와 `pumpipx3` 전체 coherence probe로 검증했습니다.
- 실제 `pumpitpr` 실행에서 다음 frontier를 확인하는 것은 사용자 재검증 항목입니다.

---

# DOS Set-System-Date HLE Work Log

Design: [20260815-484-dos-set-system-date.md](../design/20260815-484-dos-set-system-date.md)

Work order: [20260815-484-dos-set-system-date.md](../work-orders/20260815-484-dos-set-system-date.md)

## Result

- Confirmed the `repiu_log.txt` stop as missing `INT 21h AH=2Bh` support.
- Added platform-neutral DOS date validation, difference, shift, and weekday logic.
- Stored virtual DOS date as a per-execution day offset from host local date.
- Routed Function 2Bh through both ordinary DOS HLE and traced/AOT dispatch.
- Updated Function 2Ah to return the virtual date and corresponding weekday.
- The real host operating-system date is never changed.
- Added date-contract and handler set/get round-trip checks to the full AOT probe.

## Verification

- Win32 x86 Debug `repiu_aot_probe` and `repiu`: build passed.
- Win32 x86 Release `repiu_aot_probe` and `repiu`: build passed.
- Debug full `pumpipx3` probe: `dos_date_probe=pass`,
  `glide_texture_table_stack_probe=pass`, `coherence_all=true`, exit code zero.
- Release full `pumpipx3` probe: `dos_date_probe=pass`,
  `glide_texture_table_stack_probe=pass`, `coherence_all=true`, exit code zero.
- The first full Debug build reached its 300-second limit during a broad rebuild;
  resuming the same build completed with no compile errors.
- The `pumpitpr` image cannot drive the full probe because of the existing AOT
  cache-probe limitation `direct control-flow target is outside the cache`.
  The date handler was verified by its focused checks and the full `pumpipx3`
  coherence probe.
- A real `pumpitpr` rerun remains the user verification step for the next frontier.
