# Task 613 — 동적 AOT generation probe 작업 지시

## 한국어

1. AOT code-cache API에 최신 dynamic append 범위 안의 exact active guest
   address를 INT3로 표시하는 opt-in 함수를 추가합니다.
2. `RequestAotDynamicTranslation`이 append 성공을 받은 직후,
   `REPIU_EXECUTION_PROBE_OFFSET`로 설정된 relocated guest address를 새
   generation에 patch합니다.
3. patch 실패는 translation 결과를 실패로 바꾸지 않고 진단 상태로만 남깁니다.
4. Linux x64 `CopySnapshotFromContextRecord`가 fixed-width guest fields를
   복사하도록 하여 probe hit 후 EAX/EFLAGS가 비어 있지 않게 합니다.
5. 설계 문서와 Linux frontier 분석, 작업 로그를 갱신합니다.
6. Linux x64 `repiu`/`repiu_core_probe`를 빌드하고, probe 설정/미설정
   `pumpit2a`를 실행해 반환 EAX와 기존 종료 경계를 비교합니다.

### 완료 조건

* probe 설정 시 초기 generation뿐 아니라 동적 generation의 target에도
  INT3가 설치됩니다.
* `0x010F1E1C` post-call probe가 helper 반환 EAX를 관찰합니다.
* Linux x64 probe snapshot이 EIP/EAX/EFLAGS를 실제 값으로 보존합니다.
* probe 미설정 실행은 새 출력과 동작 변화가 없습니다.
* `repiu_core_probe`가 모두 통과하고 fault-free DOS 종료가 유지됩니다.
* 이번 단계에서는 allocator 동작, free-list, selector limit, stack 또는
  memory contract를 추정해 변경하지 않습니다.

## English

1. Add an opt-in AOT code-cache API that marks exact active guest addresses in
   the latest dynamic append range with INT3.
2. Immediately after `RequestAotDynamicTranslation` reports an append, patch the
   relocated guest address selected by `REPIU_EXECUTION_PROBE_OFFSET` in the new
   generation.
3. Keep a probe patch failure diagnostic-only; it must not turn a successful
   translation into a failure.
4. Make Linux x64 `CopySnapshotFromContextRecord` copy the fixed-width guest
   fields so EAX/EFLAGS are present after a probe hit.
5. Update the design document, Linux frontier analysis, and work log.
6. Build Linux x64 `repiu` and `repiu_core_probe`, then compare `pumpit2a` runs
   with and without the probe, including captured return EAX and termination.

### Done criteria

* With the probe configured, INT3 is installed in both the initial and dynamic
  generations for the selected target.
* The `0x010F1E1C` post-call probe captures the helper return EAX.
* The Linux x64 probe snapshot preserves actual EIP/EAX/EFLAGS values.
* The unset-probe run has no new output or behavior change.
* `repiu_core_probe` passes and fault-free DOS termination remains intact.
* No guessed allocator, free-list, selector-limit, stack, or memory-contract
  behavior is implemented in this step.
