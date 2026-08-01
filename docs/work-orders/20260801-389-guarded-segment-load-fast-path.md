# 20260801-389 Guarded Segment Load Fast Path 작업 지시 / Work Order

## 한국어

1. register-source `MOV Sreg, r16`을 전용 AOT instruction kind로 분류합니다.
2. ES/DS/FS/GS와 EAX/ECX/EDX/EBX/EBP/ESI/EDI source만 허용하고 SS, ESP, memory source는 거부합니다.
3. source/physical/shadow selector가 모두 같은 경우만 fallthrough하는 guarded slot을 생성합니다.
4. 불일치와 patch 불가 상태는 원래 register/EFLAGS를 복구한 뒤 INT3/VEH로 fail closed 합니다.
5. static placement, dynamic append, selector 재해석에 shadow와 성공/복구 counter patch를 연결합니다.
6. `REPIU_AOT_GUARDED_SEGMENT_LOAD` opt-in과 진단 계수를 추가합니다.
7. synthetic probe, Release 빌드, 전체 probe, 짧은 A/B smoke를 수행합니다.
8. 누적 분석과 작업 로그를 갱신하고 커밋한 뒤 수동 Music Select 검증 명령을 제공합니다.

## English

1. Classify register-source `MOV Sreg, r16` as a dedicated AOT instruction kind.
2. Allow ES/DS/FS/GS and EAX/ECX/EDX/EBX/EBP/ESI/EDI sources; reject SS, ESP, and memory sources.
3. Emit a guarded slot that falls through only when source, physical, and shadow selectors all match.
4. On mismatch or unresolved patching, restore original registers/EFLAGS and fail closed through INT3/VEH.
5. Wire shadow and success/fallback counter patches through static placement, dynamic append, and selector re-resolution.
6. Add the `REPIU_AOT_GUARDED_SEGMENT_LOAD` opt-in and telemetry.
7. Run synthetic probes, the Release build, the full probe, and short A/B smokes.
8. Update cumulative analysis and the work log, commit, then provide the manual Music Select verification command.
