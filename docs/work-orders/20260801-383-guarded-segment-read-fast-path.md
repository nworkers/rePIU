# 20260801-383 작업 지시: Guarded Segment Read Fast-Path / Work Order: Guarded Segment Read Fast-Path

설계: [20260801-383-guarded-segment-read-fast-path.md](../design/20260801-383-guarded-segment-read-fast-path.md)

## 한국어

1. register-form `mov r32, Sreg`를 별도 AOT instruction kind로 분류합니다.
2. 실제 CPU selector와 shadow selector가 같은 경우에만 native로 완료하고, 불일치 시 정확한 진입 상태를 복구한 뒤 기존 INT3 HLE boundary로 보내는 guarded slot을 구현합니다.
3. 성공 경로가 EFLAGS, 임시 EAX, 대상 GPR 상위 16비트를 보존하도록 합니다.
4. static placement, dynamic append, selector re-resolution에서 두 shadow 주소와 fallback provenance를 patch·유지합니다.
5. `REPIU_AOT_GUARDED_SEGMENT_READ=1` opt-in을 추가하고 기본값은 off로 둡니다.
6. planner/code-cache probe, Release x86 빌드, 동일 EEPROM 5초 A/B 스모크를 수행합니다.
7. 분석 문서와 작업 로그를 갱신하고 Music Select capture 명령을 제공합니다.

## English

1. Classify register-form `mov r32, Sreg` as a dedicated AOT instruction kind.
2. Implement a guarded slot that completes natively only when the physical CPU selector equals the shadow selector; on mismatch, restore exact entry state and reach the existing INT3 HLE boundary.
3. Preserve EFLAGS, scratch EAX, and the destination GPR's upper 16 bits on the success path.
4. Patch and retain both shadow addresses and fallback provenance across static placement, dynamic append, and selector re-resolution.
5. Add the `REPIU_AOT_GUARDED_SEGMENT_READ=1` opt-in and keep it off by default.
6. Run planner/code-cache probes, a Release x86 build, and a five-second A/B smoke with identical EEPROM copies.
7. Update analysis and the work log, then provide the Music Select capture command.