# 20260830-530 pumpipx3 FF /4 site·addressing mode 귀속 작업 지시서

설계: [20260830-530](../design/20260830-530-pumpipx3-ff4-site-attribution.md)

## 목표

Task 529에서 late drop과 함께 증가한 AOT `FF /4` 표본을 guest EIP/site와 ModRM addressing
mode로 분해합니다. 원본 실행 파일, AOT dispatch 의미, HLE 동작은 변경하지 않습니다.

## 작업 순서

1. `HandleAotReentry`의 기존 boundary byte window와 `RecordAotOtherBoundarySample` 연결점을
   확인합니다.
2. 고정 32-slot `AotFfBoundaryAttribution` site census와 addressing mode 분류기를 추가합니다.
3. 동일 site의 packed bytes variant와 table overflow를 누적합니다.
4. live reporter에 상위 8개 site를 `[repiu-live-ff-site]`로 출력합니다.
5. probe에 mode 분류, site ranking, overflow, inert 동작 검사를 추가합니다.
6. Linux i386 Release 빌드와 Windows AOT probe를 실행합니다.
7. 동일 trace-free 60초 조건으로 `pumpipx3`와 `pumpit1`을 실행하고 #3→#4 구간을 비교합니다.
8. 결과에 따라 resolved target 귀속을 다음 작업으로 확정하거나 site census capacity를
   재검토합니다.

## 수용 기준

* 기존 FF group census와 모든 기존 probe 항목이 통과합니다.
* register, absolute, base, SIB, 16-bit address-size mode의 분류 probe가 통과합니다.
* site count가 fixed slot에서 누적되고 overflow가 명시적으로 측정됩니다.
* 두 타이틀에서 최소 4개 live profile/site 표본과 종료 이유를 확보합니다.
* `pumpipx3` late drop 직전·직후의 상위 site와 mode를 비교합니다.
* 관측 결과만으로 최적화를 적용하지 않습니다.

## 검증 정책

* 표본별 formatted logging을 추가하지 않습니다.
* guest memory 재조회와 resolved target 계산은 이 작업 범위에서 제외합니다.
* 측정 로그는 저장소 밖에 두고, 작업 로그에는 명령과 요약 수치만 남깁니다.

---

# 20260830-530 Work Order: pumpipx3 FF /4 Site and Addressing-Mode Attribution

Design: [20260830-530](../design/20260830-530-pumpipx3-ff4-site-attribution.md)

## Objective

Split the AOT `FF /4` samples that grew with the Task 529 late drop by guest EIP/site and ModRM
addressing mode. Original executable code, AOT dispatch semantics, and HLE behavior remain unchanged.

## Procedure

1. Confirm the existing boundary byte window and the `RecordAotOtherBoundarySample` connection.
2. Add a fixed 32-slot `AotFfBoundaryAttribution` site census and addressing-mode classifier.
3. Accumulate byte variants for each site and table overflow.
4. Emit the top eight sites through `[repiu-live-ff-site]` in the live reporter.
5. Add probe checks for mode classification, site ranking, overflow, and inert behavior.
6. Run the Linux i386 Release build and Windows AOT probe.
7. Run both titles trace-free for 60 seconds under identical conditions and compare #3-to-#4.
8. Based on the result, define resolved-target attribution as the next unit or review site capacity.

## Acceptance criteria

* Existing FF-group census and all existing probe checks pass.
* Register, absolute, base, SIB, and 16-bit address-size mode probes pass.
* Site counts accumulate in fixed slots and overflow is explicit.
* At least four live profile/site samples and a shutdown reason are obtained for each title.
* Top sites and modes immediately before and after the `pumpipx3` drop are compared.
* No optimization is applied based only on this observation.

## Verification policy

* Do not add formatted logging per sample.
* Guest-memory rereads and resolved-target calculation are outside this unit.
* Keep run logs outside the repository and record only commands and summary values in the work log.
