# 20260726-305 작업 지시: retired trap 즉시 span 재진입 / Work order: immediate span re-entry after retired traps

설계: [20260726-305-retired-trap-immediate-span.md](../design/20260726-305-retired-trap-immediate-span.md)

## 한국어

### 작업

- [x] `REPIU_AOT_RETIRED_SPAN_REENTRY` opt-in 정책과 resolver probe 추가.
- [x] retired fallback 직후 기존 native-span scanner를 호출하고 실패 시 기존 TF 유지.
- [x] attempt/success live 및 최종 telemetry 추가.
- [x] synthetic retired-entry 성공/거절 검증 추가.
- [x] 전체 probe와 Win32 x86 Debug 빌드.
- [x] OFF/ON 교차 A/B 및 기본 승격/보류 결정.
- [x] architecture, analysis, 작업 로그 갱신 후 커밋.

### 완료 조건

ON은 HLE/분기/write 경계를 통과하지 않아야 하며 fatal/legacy fallback 증가, EEPROM 변화,
예상 밖 span cancel이 없어야 합니다. 실제 success와 반복 성능 개선이 없으면 opt-in으로
남깁니다.

## English

Add an opt-in immediate native-span attempt after unresolved retired cache entries, preserving
the old TF path on rejection. Expose attempt/success, add policy and synthetic probes, pass
all existing probes and the full Win32 x86 Debug build, then run alternating OFF/ON A/B.
Default promotion requires real successes, repeatable performance improvement, unchanged
HLE/write/control boundaries, and all fatal/fallback/EEPROM/cancellation safety gates.
