# Task 620 작업 지시서: Linux x64 `RET imm16` stack 보정

## 한국어

### 작업

1. long-mode near-return emitter에서 원본 `C3`와 `C2 iw`를 구분합니다.
2. `C2 iw`의 unsigned `imm16`을 포함해 guest `R15D` stack adjustment를
   계산합니다.
3. flags를 보존하는 `LEA` sequence로 계산된 adjustment를 적용합니다.
4. 미지원·불완전한 return bytes는 기존 fail-closed 경계로 유지합니다.
5. Linux x64 빌드, core probe, map byte 확인, `pumpit2a` 재현을 수행하고
   분석 문서와 작업 로그를 갱신합니다.

### 범위 제한

* 원본 guest 실행 파일의 bytes는 수정하지 않습니다.
* zero return target을 추측하거나 자동 복구하지 않습니다.
* far return과 HLE segment stack 동작은 변경하지 않습니다.

### 완료 조건

* `core_probe_failures=0`.
* `RET 4` entry가 `+8` stack adjustment를 갖는 emitted sequence로
  검증됩니다.
* runtime 결과가 기존 frontier의 해소 여부 또는 새로 관찰된 frontier로
  기록됩니다.

## English

### Work

1. Distinguish original `C3` and `C2 iw` in the long-mode near-return emitter.
2. Include unsigned `imm16` when calculating the guest `R15D` stack
   adjustment.
3. Apply the adjustment with a flags-preserving `LEA` sequence.
4. Keep unsupported or truncated return bytes at the existing fail-closed
   boundary.
5. Build Linux x64, run the core probe, inspect map bytes, reproduce
   `pumpit2a`, and update analysis and work-log documents.

### Scope limits

* Do not modify original guest executable bytes.
* Do not guess or automatically repair a zero return target.
* Do not change far-return or HLE segment-stack behavior.

### Done criteria

* `core_probe_failures=0`.
* The `RET 4` entry is verified to use an emitted `+8` stack adjustment.
* Runtime results record either resolution of the old frontier or a newly
  observed frontier.
