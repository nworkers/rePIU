# 문자열 복사 스토어 frontier(0x0302208C) 분석 작업 지시서
# Work Order: Analyzing the String-Copy Store Frontier (0x0302208C)

## 1. 목적 (Objective)

Task 221 이후의 새 종료 지점 — guest `0x0302208C`의 `mov [edi], al`이 **wild pointer
`0xDD1523B1`**(SEH 진단 `fault VA: 1/0xDD1523B1`, 쓰기)에 쓰다 죽는 현상 — 의 EDI 출처를
역추적해 수정 방향을 정한다.

## 2. 1차 조사에서 확인된 사실 (Findings So Far)

1. **명령 문맥 (repiu_aot_probe, block `0x1022052`):** 예외 지점은 NUL 종결 문자열 복사
   루프(2바이트 언롤)의 첫 쓰기다. 직전 코드는 전역 구조체에서 16비트 필드 3개를 읽어
   호출자 제공 포인터들(`[esp+0x140]`/`[esp+0x13C]`)에 저장하고, `EAX += 0x129538D`(런타임
   `0x0329538D`, 전역 테이블 base) 후 `ESI = EAX+4`로 **소스 문자열 주소를 전역 테이블에서**
   얻는다. 즉 소스는 정상(전역 테이블), **목적지 EDI는 호출자가 전달한 인자**다.
2. **fault 시점의 마지막 정상 HLE:** `INT 21h AH=3Fh`(파일 읽기), handle `0xB`, `0x1000`바이트
   → buffer `0x041CEAC8`. 직전 open 기록은 `PIU.DAT`(8.7MB) 다음 `PIU.MTL`이므로 handle 0xB는
   그중 하나로 추정 — 전역 테이블 `0x0329538D` 부근은 이 파일들의 파싱 결과로 채워지는
   것으로 보인다.
3. `EDI = 0xDD1523B1`은 arena/스택 어디에도 속하지 않는 쓰레기 값 — 호출자가 초기화되지 않은
   메모리나 잘못 파싱된 필드에서 읽었을 가능성이 크다.

## 3. 세부 작업 (Tasks)

1. 예외 지점이 속한 함수의 시작과 호출자를 확정한다(정적: `repiu_aot_probe`로 함수 프롤로그
   역탐색 + 호출부 검색, 또는 런타임: 종료 예외 시점의 게스트 레지스터/스택 캡처를 SEH
   진단에 추가 — 현재 fault VA만 기록되고 레지스터는 마지막 디스패치 값이라 EDI 원값이
   남지 않는다).
2. EDI 값(`0xDD1523B1`)의 출처를 확정한다: (a) 미구현 HLE가 채워야 할 구조체를 게임이
   읽었는지, (b) PIU.MTL/PIU.DAT 파싱 결과의 오프셋 해석 문제인지.
3. 원인에 맞는 HLE 보완을 설계·구현하고 aot-dynamic 60초 + trap 30초로 검증한다.
4. **선행 확인:** Task 221에서 미수행한 기본 trap 백엔드 30초 회귀 확인을 이 작업 시작 시
   먼저 수행한다.

## 4. 검증 범위 (Verification Scope)

각 단계는 aot-dynamic 60초 구동으로 확인하고, 코드 수정이 포함되면 trap 백엔드 30초 회귀를
함께 확인한다.

## 5. 진행 경과 (Progress — 2026-07-16)

* **선행 trap 30초 회귀:** 완료, 회귀 없음(progress 118,692). fault 미재현.
* **작업 1 (레지스터/스택 캡처):** `CaptureException`이 이미 fault 시점 전체 레지스터를
  캡처·보고함을 확인(2차 findings의 "fault VA만 기록"은 오판). 스택 창(fault ESP에서 96
  dword) 캡처를 추가·검증했다.
* **작업 2 (EDI 출처) — 정정 확정:** EDI는 호출자 인자가 아니라 함수 `0x03021DF8`의 지역
  `[esp+0x154]=ESI+0xC`다. 구조체 필드 store(`[ESI]/[ESI+4]/[ESI+8]`)는 성공하고 ESI는
  유효한데 EDI가 홀수 wild 값이므로 **목적지 지역 포인터 `[esp+0x154]`가 손상**된 것으로
  확정. 소스 데이터·명령 번역은 정상 → 미구현 HLE(a)/파싱 미종결(b) 가설 모두 기각.
* **작업 3 (HLE 보완):** 보류 — 근인이 "지역 포인터 오염"으로 재정의되어 HLE 추측 수정은
  부적절. 다음: `[esp+0x154]` 오염 시점을 write-watch/trap 단일스텝으로 포착.
* 상세: `docs/work-logs/20260716-222-string-copy-store-frontier-log.md`,
  `docs/analysis/current-execution-frontier.md` Task 222 항목.

## 5. Progress (English)

* Deferred trap 30 s regression: done, no regression (118,692); fault not reproduced.
* Task 1 (register/stack capture): `CaptureException` already captured full fault-time
  registers (the "only fault VA" note was mistaken); added and verified a 96-dword stack-window
  capture from the fault ESP.
* Task 2 (EDI provenance) — corrected: EDI is not a caller argument but local `[esp+0x154]`
  = `ESI+0xC` in function `0x03021DF8`. The struct-field stores succeed and ESI is valid, yet
  EDI is an odd wild value, so the destination-pointer local `[esp+0x154]` is corrupted. Source
  data and instruction translation are correct, rejecting both the missing-HLE (a) and
  unterminated-source (b) hypotheses.
* Task 3 (HLE fix): deferred — the root cause is re-framed as local-pointer corruption, so a
  speculative HLE change is inappropriate. Next: catch when `[esp+0x154]` is corrupted via
  write-watch / trap single-step.
