# Task 538 작업지시서: FF4 급증과 후반 성능 하락 시간축 측정

## 한국어

### 배경

Task 537의 10초 프로파일은 `pumpipx3` 후반에 FF4 해결 대상이 크게 증가한다는 사실을 보여주었지만, 성능 하락과의 순서를 충분히 분해하지 못했습니다. 기존의 `CD` 중심 DOS 인터럽트 경계는 Task 528에서 이미 확인되었으므로, 이번 작업은 새로운 런타임 계층을 추가하지 않고 시간 해상도만 높입니다.

### 작업 항목

1. 설계 문서의 동일 조건으로 `pumpipx3`와 `pumpit1`을 각각 60초 실행합니다.
2. 1초 라이브 프로파일, FF4 site/target, AOT boundary, opcode census 로그를 보존합니다.
3. 누적 카운터 차분표를 만들어 FF4 delta, boundary delta, CD delta, frames, cycles/frame를 정렬합니다.
4. FF4 burst와 지속적인 FPS 하락의 선후관계를 판정합니다.
5. 확인된 사실과 아직 인과를 확정할 수 없는 부분을 분석 문서에 반영합니다.
6. 측정 결과와 재현 명령을 작업 로그에 기록하고 커밋합니다.

### 변경 범위

- 소스 코드: 변경하지 않습니다.
- 계측: 기존 환경 변수만 변경합니다.
- 문서: 설계, 작업지시, 분석 누적 문서, 작업 로그를 갱신합니다.

### 검증

- 두 로그 모두 1초 간격의 라이브 프로파일 행을 포함해야 합니다.
- 실행 종료 후 프로세스가 남지 않아야 합니다.
- 차분 계산에서 카운터 리셋이나 실행 중단으로 인한 음수 delta를 별도로 표시합니다.

## English

### Background

Task 537 showed a substantial increase in `pumpipx3` FF4 resolved-target samples late in the run, but its ten-second profiles did not sufficiently resolve the ordering relative to the performance drop. The existing `CD`-dominated DOS interrupt boundary was already investigated in Task 528, so this task increases time resolution without adding another runtime layer.

### Work items

1. Run `pumpipx3` and `pumpit1` for 60 seconds under the design conditions.
2. Preserve one-second live profiles, FF4 site/target, AOT-boundary, and opcode-census logs.
3. Build an adjacent-counter-difference table for FF4 delta, boundary delta, CD delta, frames, and cycles/frame.
4. Determine the ordering between the FF4 burst and persistent FPS degradation.
5. Update cumulative analysis with confirmed facts and unresolved causal questions.
6. Record measurement results and reproduction commands in the work log and commit them.

### Scope

- Source code: unchanged.
- Instrumentation: existing environment variables only.
- Documentation: update the design, work order, cumulative analysis, and work log.

### Verification

- Both logs must contain one-second live-profile rows.
- No process should remain after each run.
- Negative deltas caused by counter resets or interrupted execution must be marked separately.
