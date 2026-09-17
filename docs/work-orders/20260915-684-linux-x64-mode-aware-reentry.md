# 작업 지시 20260915-684 — Linux x64 code-mode-aware re-entry

## 배경

Task 683의 실제 runtime trace에서 far transfer target
`0x01100004`가 정적 AOT map에 없을 때, 공용 re-entry gate가 legacy-32
기본 mode로 `66 85 FF`만 검사하고 mode16 원본 byte fallback을 허용하는
문제가 확인되었습니다. 그 결과 다음 `B8 07 00`이 x64 long mode에서 다른
길이의 instruction으로 해석되어 coredump 전 단계의 잘못된 실행이 발생했습니다.

## 목표

* AOT placement/selector metadata로 target의 guest code mode를 판정합니다.
* `CanResumeLinuxX64LegacyTarget`가 mode-aware classifier를 사용하게 합니다.
* non-identical instruction은 post-HLE opt-in 여부와 무관하게 dynamic AOT
  resolver를 거치게 합니다.
* resolver 실패 시 non-identical 원본 byte 실행으로 돌아가지 않게 합니다.
* 주소나 특정 object에 종속된 예외처리를 추가하지 않습니다.

## 범위

### 구현

1. mode range와 selector descriptor를 이용하는 공용 mode 판정 helper를
   execution 경로에 추가합니다.
2. Linux x64 legacy resume gate에 `GuestCodeDefaultOperandSize`를 전달합니다.
3. HLE 처리 후 cache miss 정책을 identical/non-identical로 분리합니다.
4. general stack probe에 mode16 range와 metadata 충돌/부재 경계를 검증하는
   최소 회귀 케이스를 추가합니다.

### 문서

* 설계 문서와 이 작업 지시서를 한국어/영어로 유지합니다.
* 확인된 원인과 검증 결과를 `docs/analysis/linux-port-frontier.md`에
  누적합니다.
* 완료 후 작업 로그를 작성합니다.

## 구현 순서

1. [x] Task 683 checkpoint 커밋과 분석 기록 확인
2. [x] code-mode-aware re-entry 설계 작성
3. [x] 구현 범위와 최소 검증 절차 확정
4. [x] placement/selector 기반 mode 판정 구현
5. [x] compatibility gate에 mode 전달
6. [x] non-identical cache-miss dynamic translation 정책 구현
7. [x] general stack/core probe 회귀 검증 추가
8. [x] Linux x64 빌드와 core probe 실행
9. [x] object-3 runtime trace로 원본 mode16 fallback 제거 확인
10. [x] analysis/work-log 갱신 및 작업 커밋

## 완료 기준

* synthetic mode16 `66 85 FF`가 `CanResumeLinuxX64LegacyTarget`를 통과하지
  않습니다.
* synthetic mode32 identical instruction은 기존처럼 통과합니다.
* `PUSH ESP` 등 기존 stack divergence 거부 동작이 유지됩니다.
* cache miss에서 mode16 non-identical target은 post-HLE 환경변수가 꺼져도
  original long-mode bytes로 실행되지 않습니다.
* Linux x64 core probe가 failure 없이 통과합니다.
* runtime trace가 `0x01100004`의 mode-aware dynamic translation 요청을
  확인하고, 이전의 `0x01100009` 원본 실행 경로가 사라졌음을 보여줍니다.
* 다음 unsupported mode16 instruction인 `66 85 FF`가 일반 lowering
  frontier로 기록되며, 주소별 예외로 숨겨지지 않습니다.

## 제약과 위험

* 원본 guest executable은 수정하지 않습니다.
* selector metadata가 중복되거나 placement와 충돌하면 fail-closed 해야
  합니다.
* mode metadata가 전혀 없는 기존 synthetic context에서는 legacy-32 기본값을
  유지하여 unrelated regression을 줄입니다.
* 이 작업은 mode-aware re-entry 정책만 수정합니다. mode16 TEST, Jcc, push/pop,
  segment, far-return lowering은 별도 frontier로 남을 수 있습니다.

---

# Work Order 20260915-684 — Linux x64 code-mode-aware re-entry

## Background

Task 683's live trace confirmed that when far-transfer target
`0x01100004` is absent from the static AOT map, the shared re-entry gate checks
only `66 85 FF` with the legacy-32 default and permits a mode16 original-byte
fallback. The following `B8 07 00` is then decoded in x64 long mode with a
different length, causing incorrect execution before the coredump.

## Objectives

* Resolve the target guest code mode from AOT placement/selector metadata.
* Make `CanResumeLinuxX64LegacyTarget` use the mode-aware classifier.
* Route non-identical instructions through the dynamic AOT resolver regardless
  of post-HLE opt-in.
* Never return to non-identical original bytes after resolver failure.
* Add no address-specific or object-specific exception.

## Scope

### Implementation

1. Add a shared execution-path helper using mode ranges and selector descriptors.
2. Pass `GuestCodeDefaultOperandSize` into the Linux x64 legacy resume gate.
3. Split the handled-HLE cache-miss policy into identical and non-identical cases.
4. Add the minimum mode16 placement and metadata-boundary regression cases to the
   general stack probe.

### Documentation

* Keep the design and work order in Korean followed by English.
* Accumulate the confirmed cause and verification result in
  `docs/analysis/linux-port-frontier.md`.
* Write the corresponding work log at completion.

## Implementation sequence

1. [x] Confirm the Task 683 checkpoint commit and analysis record.
2. [x] Write the code-mode-aware re-entry design.
3. [x] Confirm implementation scope and minimum verification.
4. [x] Implement placement/selector-based mode resolution.
5. [x] Pass mode into the compatibility gate.
6. [x] Implement non-identical cache-miss dynamic translation policy.
7. [x] Add general-stack/core-probe regression coverage.
8. [x] Run the Linux x64 build and core probe.
9. [x] Use the object-3 runtime trace to confirm original mode16 fallback is gone.
10. [x] Update analysis/work log and commit the task.

## Done criteria

* Synthetic mode16 `66 85 FF` is rejected by
  `CanResumeLinuxX64LegacyTarget`.
* A synthetic mode32 identical instruction remains allowed.
* Existing stack-divergence rejection such as `PUSH ESP` remains intact.
* A mode16 non-identical cache miss cannot execute original long-mode bytes even
  when the post-HLE environment setting is disabled.
* The Linux x64 core probe passes with zero failures.
* Runtime tracing confirms a mode-aware dynamic translation request at
  `0x01100004` and no longer shows the previous original execution path at
  `0x01100009`.
* The next unsupported mode16 instruction `66 85 FF` is recorded as a shared
  lowering frontier rather than hidden behind an address-specific exception.

## Constraints and risks

* Do not modify the original guest executable.
* Duplicate or conflicting selector metadata must fail closed.
* Contexts with no mode metadata retain the legacy-32 default to minimize
  unrelated regressions.
* This task changes only mode-aware re-entry. Mode16 TEST, Jcc, push/pop,
  segment, and far-return lowerings may remain as separate frontiers.
