# 20260729-351 AOT timer source 귀속 작업 지시 / AOT timer-source attribution work order

## 한국어

### 목표

Task 347의 AOT guest 실행 유도값 안에서 240Hz timer pacing이 차지하는 시간 상한을,
guest 정지 없이 기존 timer safe-point source와 소비 tick으로 측정합니다.

### 구현

1. 고정 1024-entry `aot_timer_source_profile` 공용 정책/집계 모듈을 추가합니다.
2. AOT placement에 safe-point breakpoint-offset → guest-source index를 추가하고 초기
   image와 dynamic append에서 유지합니다.
3. PIT due tick을 atomic으로 누적하고 주입 성공 source에서만 소비·귀속합니다.
4. 종료 attempt와 loader summary에 profile 전체와 상위 source를 출력합니다.
5. `repiu_aot_probe` 합성 검증과 Release 60초 3회 측정 스크립트를 추가합니다.

### 완료 조건

- 합성 probe가 enable, 병합, injected/deferred, tick 보존, overflow, 정렬을 통과합니다.
- Win32 x86 Release loader와 AOT probe가 빌드됩니다.
- 60초 3회에서 malformed/fatal/Glide issue가 0이고 240Hz divisor가 유지됩니다.
- 상위 source의 원본 디스어셈블리를 확인해 pacing/active/unresolved를 구분합니다.
- pacing tick×PIT 주기와 Task 347 유도값의 잔여를 분석 문서에 반영합니다.
- suspend 샘플링 프로토타입의 폐기 근거를 작업 로그에 남깁니다.
- 아키텍처·분석·작업 로그를 갱신하고 하나의 Git 커밋으로 마칩니다.

---

## English

### Objective and completion

Measure the timer-pacing upper bound inside Task 347's derived AOT guest share
without suspending the guest. Add a fixed 1024-entry timer-source profile,
maintain exact breakpoint-to-guest-source metadata for initial and dynamic AOT
images, atomically preserve due PIT ticks, and consume them only at a
successful injected source. Export the profile through the execution attempt
and loader summary, add a synthetic AOT probe and a three-run Release script,
and statically classify the leading sources.

Completion requires passing probe and Win32 x86 Release builds, three
semantically clean 60-second runs at the expected 240Hz divisor, documented
pacing-time attribution and remaining Task 347 bound, the rejected suspend
prototype evidence, updated architecture/analysis/work log, and one Git
commit.
