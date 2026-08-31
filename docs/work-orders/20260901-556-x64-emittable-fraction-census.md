# 20260901-556 x64가 낼 수 있는 명령의 비율 작업 지시서

## 한국어

### 목적

실제 guest plan에 Task 553의 방출 규칙을 적용해 방출 가능 비율을 측정합니다. 설계는
[20260901-556](../design/20260901-556-x64-emittable-fraction-census.md)입니다.

### 작업

- `repiu_instruction_census`에 long-mode 절을 더한다.
- 표제 숫자는 `enable_long_mode_emission = true`로 image를 실제 빌드해 emitter의 카운터를
  읽는다. 판정 규칙을 census에 다시 구현하지 않는다.
- 사유별 분해는 census가 `ClassifyLongModeBytes`로 따로 세고, emitter 카운터와 일치하는지
  출력한다.
- emitter의 guest-address dedup을 따라한다.
- block 완결성(모든 명령이 방출되는 block 수)을 센다.
- 거절 사유를 건수 순으로 출력한다. 다음 단위의 순서를 이 표로 정한다.

### 검증

`pumpit1`의 `PIU.EXE`로 실행해 숫자를 얻고, emitter 카운터와 census 분해가 일치하는지
확인한다. 도구만 바뀌므로 실행 경로 회귀는 없지만, 세 호스트 `repiu_core_probe`는 그대로
통과해야 한다.

## English

### Objective

Measure the emittable fraction by applying Task 553's rules to the real guest plan. The
design is [20260901-556](../design/20260901-556-x64-emittable-fraction-census.md).

### Work items

- Add a long-mode section to `repiu_instruction_census`.
- Take the headline numbers by actually building an image with
  `enable_long_mode_emission = true` and reading the emitter's counters. Do not
  reimplement the rule in the census.
- Count the breakdown separately with `ClassifyLongModeBytes` and print whether it agrees
  with the emitter's counters.
- Mirror the emitter's guest-address dedup.
- Count block completeness -- blocks in which every instruction is emitted.
- Print refusal reasons ordered by count. The next unit's order is chosen from that table.

### Verification

Run against `pumpit1`'s `PIU.EXE` and obtain the numbers, checking that the emitter's
counters and the census breakdown agree. Only a tool changes, so there is no execution-path
regression, but `repiu_core_probe` must still pass on all three hosts.
