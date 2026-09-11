# 20260911-657 작업 지시: Linux x64 segment coverage predicate parity

설계: [20260911-657 설계](../design/20260911-657-linux-x64-segment-coverage-parity.md)

## 한국어

1. `ValidateAotCodeCacheHleCoverage`의 long-mode segment override
   `absolute_disp32` 판정을 emitter와 일치시킵니다.
2. 무변위 명령의 suffix 시작 offset 계산도 emitter와 일치시킵니다.
3. `66 36 89 07` EDI-base segment override coverage regression probe를
   `long_mode_emission_probe`에 추가합니다.
4. 정상 slot coverage 통과와 slot corruption 거절을 probe에서 확인합니다.
5. Linux x64 Debug build, core probe, 실제 `pumpit2a` 실행을 검증합니다.
6. 분석 문서, 아키텍처 문서, 작업 로그를 갱신하고 커밋합니다.

범위 밖:

* segment override emission 바이트 변경
* guest memory semantics 또는 segment patch 정책 변경
* fallback/RET resolver 정책 변경
* i386 기본 emission 변경

## English

1. Make the long-mode segment-override `absolute_disp32` predicate in
   `ValidateAotCodeCacheHleCoverage` match the emitter.
2. Match the emitter's suffix offset for zero-displacement forms.
3. Add an EDI-base `66 36 89 07` segment-override coverage regression probe to
   `long_mode_emission_probe`.
4. Verify valid-slot coverage and rejection after slot corruption in the probe.
5. Verify the Linux x64 Debug build, core probe, and real `pumpit2a` run.
6. Update the analysis, architecture, and work-log documents, then commit.

Out of scope:

* Changing segment-override emission bytes
* Changing guest memory semantics or segment patch policy
* Changing fallback or RET-resolver policy
* Changing default i386 emission
