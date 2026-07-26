# 20260726-306 작업 지시: retired trap hotset 및 해결 결과 계측 / Work order: retired-trap hotset and resolution profiling

설계: [20260726-306-retired-trap-hotset-profile.md](../design/20260726-306-retired-trap-hotset-profile.md)

## 한국어

### 작업

- [x] opt-in profile 상태, exact guest/cache histogram과 65,536-entry cap 구현.
- [x] retired cache entry의 generation, guest/emitted length, relink 가능 여부 기록.
- [x] resolver 결과를 active/generation/quarantine/failure/fallback/trace로 분류.
- [x] 최종 top 16, coverage, 결과별 count를 실행 결과와 로그에 출력.
- [x] 설정·metadata·반복 count·정렬·overflow 합성 probe 추가.
- [x] 전체 probe와 Win32 x86 Debug 빌드.
- [x] 장시간 runtime profile로 실제 hotset과 다음 최적화 분기 결정.
- [x] architecture, analysis, 작업 로그 갱신 후 커밋.

### 완료 조건

profiler OFF는 기존 실행 제어 흐름을 바꾸지 않아야 합니다. profiler ON 측정은 요청 시간을
완료하고 fatal/legacy fallback 증가나 EEPROM 변화가 없어야 합니다. exact histogram이
overflow하지 않은 경우 top-16 coverage를 확정값으로 기록하고, 결과에 따라 stale relink
개선 또는 다른 boundary opcode 분석 중 다음 작업을 선택합니다.

## English

Implement an opt-in, bounded exact retired-trap profile with guest/cache hotsets, entry
generation and length metadata, relink eligibility, and resolver outcome classification.
Expose sorted top 16 results and coverage in final execution logs, add synthetic probes, pass
all existing probes and the full Win32 x86 Debug build, then run a bounded long profile with
unchanged fatal/fallback/EEPROM safety evidence. Use the measured concentration and outcome
distribution to choose stale-entry prevention or the next boundary-opcode investigation.
