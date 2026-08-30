# 20260830-529 pumpipx3 FF 경계 원인 귀속 작업 지시

설계: [20260830-529](../design/20260830-529-pumpipx3-ff-boundary-attribution.md)

## 작업 목표

Task 528에서 관측한 `pumpipx3` late drop 주변의 AOT `FF` 표본을 ModRM 그룹별로 분해하여,
`FF /2` 간접 call과 `FF /4` 간접 jump 중 어느 경로가 증가하는지 확인합니다. 원본 EXE,
게스트 timing, HLE 의미 및 실행 경로는 변경하지 않습니다.

## 실행 절차

1. 기존 `AotBoundaryOpcodeCensus`와 probe의 partition 불변 조건을 확인합니다.
2. `FF` 유효 opcode 뒤의 ModRM reg 필드와 truncated 수를 누적하는 계측을 추가합니다.
3. probe에서 `/2`, `/4`, truncated 입력 및 기존 histogram 합계를 검증합니다.
4. live reporter에 `[repiu-live-ff]` 누적 그룹 출력을 연결합니다.
5. Linux i386 Release를 빌드합니다.
6. 두 타이틀을 동일한 60초 무트레이스 조건으로 실행하고 frame-rate와 FF 그룹을 맞춥니다.
7. late drop과 `/2`·`/4` 증가의 동시성을 판정하고, 원인이 미확정이면 다음 분석 축으로
   기록합니다.

## 성공 기준

* probe가 기존 동작과 FF 그룹 분류를 모두 통과합니다.
* Linux i386 Release 빌드가 성공합니다.
* 두 타이틀에서 최소 4개의 live profile/FF 표본과 종료 이유를 확보합니다.
* `pumpipx3` late drop 전후의 `/2`, `/4`, truncated 누적 변화를 확인합니다.
* 인과관계를 확정할 수 없으면 추측성 최적화를 적용하지 않고 미확정으로 남깁니다.

## 검증 및 산출물

* 실행 로그는 저장소 밖 임시 경로에 보관하고 문서에는 명령과 요약 수치만 남깁니다.
* 코드 변경이 있으므로 Linux i386 Release 빌드와 census probe를 실행합니다.
* 결과는 대응 작업 로그와 `docs/analysis/` frontier에 반영합니다.

---

# 20260830-529 Work Order — FF Boundary Attribution for pumpipx3

Design: [20260830-529](../design/20260830-529-pumpipx3-ff-boundary-attribution.md)

## Objective

Split the AOT `FF` samples observed near the Task 528 `pumpipx3` late drop by ModRM group and
determine whether `FF /2` indirect calls or `FF /4` indirect jumps increase. Original EXE code,
guest timing, HLE semantics, and execution paths remain unchanged.

## Procedure

1. Confirm the existing `AotBoundaryOpcodeCensus` and probe partition invariants.
2. Add cumulative counting of the ModRM reg field following an effective `FF` opcode and count
   truncated samples.
3. Verify `/2`, `/4`, truncated input, and the existing histogram total in the probe.
4. Connect cumulative FF-group output as `[repiu-live-ff]` in the live reporter.
5. Build Linux i386 Release.
6. Run both titles trace-free under identical 60-second conditions and align frame rate with FF
   groups.
7. Decide whether the late drop and `/2` or `/4` growth are simultaneous; if not established,
   record the next analysis axis as unresolved.

## Acceptance criteria

* The probe passes both existing behavior and FF-group classification.
* The Linux i386 Release build succeeds.
* At least four live profile/FF samples and a shutdown reason are obtained for each title.
* Cumulative `/2`, `/4`, and truncated changes are compared before and after the `pumpipx3` drop.
* If causality is not established, no speculative optimization is applied.

## Verification and artifacts

* Keep run logs outside the repository and record only commands and summary values in the docs.
* Run the Linux i386 Release build and the census probe because code is changing.
* Reflect results in the work log and the `docs/analysis/` frontier.
