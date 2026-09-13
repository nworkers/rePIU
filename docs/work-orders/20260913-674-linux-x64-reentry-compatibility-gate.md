# 작업 지시서: Linux x64 re-entry 원본 명령 호환성 gate

## 한국어

### 배경

Task 673은 최초 저주소 signal에서 원본 guest `ADD ESP,4`가 실행되었음을 확인했다. 현재 AOT cache는 이 명령을 `R15D` 기반으로 올바르게 lowering하지만, cache boundary의 공통 single-step re-entry가 원본 명령을 먼저 실행한다.

### 구현 순서

1. cache boundary re-entry 직전에 기존 `CanResumeLinuxX64LegacyTarget` compatibility 판정을 연결한다.
2. x64 non-identical 명령은 `ResolveAotTransferTarget` 결과의 cache entry로 직접 resume한다.
3. resolve 실패 시 `aot_reentry_pending`과 TF pass-through 상태를 제거해 원본 실행을 차단한다.
4. identical 명령과 known HLE boundary의 기존 경로는 유지한다.
5. WSL build, core probe, census 및 bounded runtime으로 검증하고 문서화한다.

### 완료 조건

* `ADD ESP,4`와 같은 stack-pointer instruction이 원본 guest 주소에서 실행되지 않는다.
* 정책이 특정 EIP 목록이 아니라 공통 long-mode classifier를 사용한다.
* lowering된 cache entry로 재개된다.
* 기존 original-byte HLE 경계와 identical instruction 동작은 보존된다.

## English

### Background

Task 673 confirmed that original guest `ADD ESP,4` executed at the first low-`RSP` signal. The AOT cache already lowers it to an `R15D` guest-stack update, but the shared cache-boundary single-step re-entry executes the original instruction first.

### Implementation order

1. Use the existing `CanResumeLinuxX64LegacyTarget` compatibility decision before cache-boundary re-entry.
2. Resume x64 non-identical instructions directly at the cache entry returned by `ResolveAotTransferTarget`.
3. On resolution failure, clear pending/TF pass-through state so the original instruction is not executed.
4. Preserve the existing path for identical instructions and known HLE boundaries.
5. Verify with the WSL build, core probe, census, and a bounded runtime run, then document the result.

### Done when

* A stack-pointer instruction such as `ADD ESP,4` is never executed from the original guest address.
* The policy uses the shared long-mode classifier rather than a guest-EIP list.
* Execution resumes from the lowered cache entry.
* Existing original-byte HLE boundaries and identical instructions retain their behavior.
