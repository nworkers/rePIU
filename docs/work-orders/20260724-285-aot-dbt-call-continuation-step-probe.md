# AOT-DBT CALL continuation 제한 trap 관측 작업 지시서 / AOT-DBT bounded CALL-continuation trap probe work order

## 한국어

### 목표

Task 284에서 dispatcher-visible RET가 없었던 CALL sequence 27, 30, 33, 56의
synthetic `C3` 직전·직후와 실제 caller continuation 복귀 상태를 byte patch 없이
관측합니다. 결과로 물리적 CALL/RET 전이 결함과 VEH 우회 부수효과를 구분합니다.

### 작업 범위

1. Task 285 설계의 고정 크기 Win32 `aot_dbt_call_step_probe.{h,cpp}`를 구현합니다.
2. `REPIU_AOT_DBT_CALL_STEP`의 최대 8개 sequence를 실행 시작 시 파싱합니다.
3. 선택된 host CALL 성공 시 saved EFLAGS.TF를 켭니다.
4. main VEH 앞단에서 pre-C3, post-C3와 return-target `#DB`를 전용 handler로 처리합니다.
5. post-C3에서 active cache return/guest return에 DR0/DR1 실행 breakpoint를 설치하고
   hit 뒤 원 debug state를 복원합니다.
6. 반환 watch 중 새 native fast path 진입을 막고 기존 DR 소유권 충돌은 fail-closed합니다.
7. live state, final attempt snapshot과 loader 최종 로그를 연결합니다.
8. synthetic probe와 CMake 연결을 추가합니다.
9. sequence 27/30/33/56을 calls-only·격리 EEPROM으로 각각 구동합니다.
10. 분석·아키텍처·작업 로그를 갱신합니다.

### 비범위

- guest byte 또는 code-cache byte 패치
- CALL host-dispatch 크래시의 추측 수정
- inline-cache 정책·layout·patch 순서 변경
- 기본 CALL host-dispatch 활성화
- shared live telemetry POD 버전 변경
- 전체 trap backend 장시간 단일스텝

### 완료 조건

- probe 비활성 시 기존 실행과 방출 byte가 동일합니다.
- 합성 probe가 pre/post/return 전이, 기대 ESP, DR 복원, conflict와 ring을 검증합니다.
- Win32 x86 Debug 전체 빌드와 기존 AOT probe가 통과합니다.
- 네 sequence 각각에 대해 pre/post/return 또는 crash 시 pending phase가 회수됩니다.
- probe가 크래시를 바꾸는 경우 이를 수정 성공이 아니라 VEH 교란 증거로 분리합니다.
- EEPROM hash가 영속 상태의 비의도 변경이 없음을 확인합니다.

## English

### Goal

Observe, without byte patching, state before and after the synthetic `C3` and after the
physical return to the caller for Task 284's uncorrelated CALL sequences 27, 30, 33, and
56. Use the result to distinguish a physical CALL/RET defect from a side effect lost by
bypassing VEH.

### Scope

1. Implement the fixed Win32 `aot_dbt_call_step_probe`.
2. Parse up to eight `REPIU_AOT_DBT_CALL_STEP` sequences at execution start.
3. Set saved EFLAGS.TF after a selected host CALL succeeds.
4. Handle pre-C3, post-C3, and return-target debug traps before the normal VEH chain.
5. Install DR0/DR1 execution breakpoints at active-cache/guest return addresses and restore
   original debug state on hit.
6. Prevent new native-fast-path ownership during a return watch and fail closed on existing
   debug-register conflicts.
7. Connect live state, final-attempt snapshot, and final loader output.
8. Add synthetic probes and CMake integration.
9. Run isolated-EEPROM calls-only tests for sequences 27, 30, 33, and 56 separately.
10. Update analysis, architecture, and the work log.

### Out of scope

- Guest-byte or code-cache-byte patching
- A speculative crash fix
- Inline-cache policy, layout, or patch-order changes
- Enabling CALL host dispatch by default
- Shared-live-telemetry POD version changes
- Full long-running trap-backend single-stepping

### Completion criteria

Disabled mode leaves existing execution and emitted bytes unchanged. Synthetic probes cover
pre/post/return transitions, expected ESP, debug-state restoration, conflicts, and ring
order. The full Win32 x86 Debug build and existing AOT probes pass. Every target sequence
yields pre/post/return evidence or an active phase at the crash. Any crash suppression is
classified as VEH perturbation evidence rather than a fix, and EEPROM hashes show no
unintended persistent-state change.
