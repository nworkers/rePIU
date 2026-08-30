# 20260830-528 pumpipx3와 pumpit1 성능 차이 귀속 작업 지시

설계: [20260830-528](../design/20260830-528-pumpipx3-performance-attribution.md)

## 작업 목표

중단된 동일 조건 실행을 복원하고, `pumpipx3`의 프레임 저하가 원본 게스트 timing인지
HLE/AOT 경계 비용인지 증거로 분리합니다. 이번 작업은 측정과 문서화까지이며 성능 코드를
수정하지 않습니다.

## 실행 절차

1. WSLg Ubuntu-24.04에서 Release 실행 파일과 ROM set을 확인합니다.
2. trace 없는 60~90초 실행으로 두 타이틀의 종료 이유, 프레임 곡선, live profile을 수집합니다.
3. 각 타이틀에 대해 필요할 때 별도의 trace 실행을 하여 `INT 21h` vector/AH 분포를 셉니다.
4. `repiu-live-opcode`, `repiu-live-aot`, `repiu-live-veh`, `repiu-frame-rate`를 시간축으로
   맞춰 후반 저하의 시작점을 찾습니다.
5. 호출 빈도, cycles/frame, DOS 비율, VEH 비율을 표로 정리하고 다음 분석 축을 결정합니다.
6. 기존 미커밋 Task 528 opcode 계측 변경을 포함하여 설계·작업 로그를 남깁니다.
7. trace 오버헤드를 피하기 위해 INT 21h 전용 AH 누적 카운터를 `[repiu-live-dos]`로
   출력하고, 두 타이틀의 live 호출 분포를 비교합니다.

## 성공 기준

* 두 타이틀이 같은 환경 변수와 실행 제한으로 재현됩니다.
* 각 타이틀에서 최소 두 개 이상의 live profile 표본과 종료 이유를 확보합니다.
* `pumpipx3` 후반 저하 구간의 시작 시점을 프레임 로그로 확인합니다.
* DOS trace의 `AH=2Ch` 호출 수와 live profile의 DOS 비율을 분리해 제시합니다.
* trace 없는 실행에서 `[repiu-live-dos]`의 INT 21h 전용 AH 분포를 확보합니다.
* 성능 원인을 확정할 수 없으면 미확정으로 남기고 추측성 최적화를 하지 않습니다.

## 검증 및 산출물

* 코드 계측 변경이 포함되어 있으므로 Linux i386 Release 빌드를 검증합니다.
* 실행 로그는 저장소 밖의 임시 경로에 두며, 문서에는 재현 가능한 명령과 요약 수치만 남깁니다.
* 결과는 대응 작업 로그와 관련 `docs/analysis/` frontier 문서에 반영합니다.

---

# 20260830-528 Work Order — Performance Attribution for pumpipx3 vs pumpit1

## Objective

Restore the interrupted same-condition run and separate the `pumpipx3` frame-rate drop into
original guest timing or HLE/AOT boundary cost using evidence. This unit covers measurement and
documentation; it does not modify performance code.

## Procedure

1. Verify the Release executable and ROM sets under WSLg Ubuntu-24.04.
2. Collect 60–90 second runs without tracing for both titles, including shutdown reason, frame
   curve, and live profile.
3. Run separate trace-enabled executions as needed to count the `INT 21h` vector/AH distribution.
4. Align `repiu-live-opcode`, `repiu-live-aot`, `repiu-live-veh`, and `repiu-frame-rate` on the
   time axis and locate the beginning of the late drop.
5. Summarize call frequency, cycles/frame, DOS share, and VEH share, then choose the next axis.
6. Record the design and work log, including the existing uncommitted Task 528 opcode census
   change.
7. Print the INT 21h-specific cumulative AH counters as `[repiu-live-dos]` to avoid trace
   overhead, and compare the live call distributions between the two titles.

## Acceptance criteria

* Both titles reproduce with identical environment variables and execution limits.
* Obtain at least two live-profile samples and a shutdown reason for each title.
* Identify the start of the late `pumpipx3` drop from the frame log.
* Present `AH=2Ch` count separately from the DOS share in the live profile.
* Obtain the INT 21h-specific AH distribution from `[repiu-live-dos]` in a trace-free run.
* If the cause cannot be established, leave it unresolved and do not apply speculative tuning.

## Verification and artifacts

* Because code instrumentation is present, verify a Linux i386 Release build.
* Keep execution logs in a temporary path outside the repository; retain reproducible commands and
  summary values in the documentation.
* Reflect the result in the corresponding work log and the relevant `docs/analysis/` frontier.
