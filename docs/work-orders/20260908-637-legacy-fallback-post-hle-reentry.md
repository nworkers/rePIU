# Task 637 작업 지시: legacy fallback의 HLE 후 cache 재진입

설계: [20260908-637](../design/20260908-637-legacy-fallback-post-hle-reentry.md)

## 한국어

### 변경 범위

1. `TryResumeAotAfterHandledHle`의 초기 상태 gate가
   `aot_reentry_pending || aot_legacy_fallback`을 허용하도록 수정합니다.
2. 기존 not-pending 거절 계수는 두 상태가 모두 false인 경우에만 증가시킵니다.
3. opt-in HLE reentry trace에 legacy fallback 상태를 출력하고
   `0xFFFFFFFF` 전체 선택 진단값을 지원합니다.
4. `ARCHITECTURE.md`와 Linux frontier 분석에 복구 계약과 실행 증거를 반영합니다.
5. 구현 후 대응 작업 로그를 작성합니다.

### 비범위

* `0x010F1E17` 또는 `0x010F920C` 주소 특례
* 원본 실행 파일 수정
* direct-call emitter/fixup 재설계
* cache miss의 기본 동적 번역 정책 변경

### 완료 조건

* Linux x64 Debug 빌드와 core probe가 통과합니다.
* 추적에서 최초로 안전한 기존 cache entry에 도달한 HLE가 `legacy=1` 상태의
  cache hit와 `resumed`로 이어집니다.
* 기존 raw `0x010F1E17` CALL fault가 재발하지 않습니다.
* 새 frontier와 미확정 사항을 문서화하고 변경을 커밋합니다.

## English

### Change scope

1. Let the initial state gate in `TryResumeAotAfterHandledHle` accept
   `aot_reentry_pending || aot_legacy_fallback`.
2. Increment the existing not-pending rejection counter only when both states
   are false.
3. Print legacy-fallback state in the opt-in HLE re-entry trace and support the
   `0xFFFFFFFF` diagnostic value for selecting every attempt.
4. Update `ARCHITECTURE.md` and the Linux frontier analysis with the recovery
   contract and runtime evidence.
5. Write the matching work log after implementation.

### Out of scope

* Address-specific handling for `0x010F1E17` or `0x010F920C`
* Modification of the original executable
* Redesign of the direct-call emitter or fixups
* Changing the default policy for translating cache misses

### Completion criteria

* The Linux x64 Debug build and core probe pass.
* Tracing shows that the first HLE to reach a safe existing cache entry leads
  from `legacy=1` through a cache hit to `resumed`.
* The prior raw CALL fault at `0x010F1E17` does not recur.
* The new frontier and remaining unknowns are documented and committed.
