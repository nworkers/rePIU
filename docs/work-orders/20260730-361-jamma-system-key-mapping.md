# JAMMA 시스템 키 매핑 수정 작업 지시
# JAMMA System Key Mapping Correction Work Order

* 작업 번호 / Task: 361
* 작성일 / Date: 2026-07-30
* 설계 / Design: `docs/design/20260730-361-jamma-system-key-mapping.md`

## 1. 목표
## 1. Objective

Win32 JAMMA 시스템 입력을 F1=TEST, F2=SERVICE, F3=CLEAR, F5=COIN1으로
분리하여 TEST 키가 원본 게임의 시스템 메뉴 신호를 발생시키도록 한다.

Separate the Win32 JAMMA system inputs into F1=TEST, F2=SERVICE, F3=CLEAR, and
F5=COIN1 so the TEST key emits the original game's system-menu signal.

## 2. 작업 범위
## 2. Scope

1. `ReadJammaPort8`의 `0x02A9` 키 매핑을 수정한다.
2. `kJammaBitsSystem` 로그 이름표를 같은 비트 정의로 수정한다.
3. PIU I/O 분석 문서에 시스템 입력 비트와 호스트 키 정책을 기록한다.
4. Win32 x86 빌드와 네 키의 active-low 실행 검증을 수행한다.
5. 검증 결과를 작업 로그에 기록한다.

1. Correct the `0x02A9` key mapping in `ReadJammaPort8`.
2. Correct the `kJammaBitsSystem` log labels to the same bit definition.
3. Record the system input bits and host-key policy in the PIU I/O analysis.
4. Build Win32 x86 and run active-low verification for all four keys.
5. Record the verification results in the work log.

## 3. 완료 조건
## 3. Completion Criteria

* F1, F2, F3, F5가 각각 `0x02`, `0x40`, `0x80`, `0x04`를 active-low로
  구동한다.
* 진단 로그의 신호 이름과 포트 값이 일치한다.
* Win32 x86 Debug 빌드가 성공한다.
* 원본 게스트 명령과 JAMMA 폴링 구조는 변경하지 않는다.

* F1, F2, F3, and F5 drive `0x02`, `0x40`, `0x80`, and `0x04`
  active-low respectively.
* Diagnostic signal names match the emitted port values.
* The Win32 x86 Debug build succeeds.
* Original guest instructions and the JAMMA polling structure remain unchanged.
