# 작업 로그 20260915-684 — Linux x64 code-mode-aware re-entry

## 결과 요약

Task 683에서 확인한 mode16 원본 byte fallback을 공용 re-entry 정책에서
차단했습니다. `CanResumeLinuxX64LegacyTarget`가 AOT placement의
`RuntimeCodeModeRange`와 selector descriptor를 사용해 guest code mode를
판정하고, 그 mode를 long-mode compatibility classifier에 전달합니다.

cache miss에서 현재 instruction이 non-identical이면
`REPIU_AOT_DBT_POST_HLE_TRANSLATE` 설정이 꺼져 있어도 dynamic AOT resolver를
시도하도록 수정했습니다. resolver가 만든 cache entry가 지원하지 않는
instruction을 만나면 원본 bytes로 돌아가지 않고 기존 fail-closed 경계에서
중단합니다.

## 구현 내용

* placement range 우선, executable selector descriptor 보조 순서로 mode를
  찾는 공용 helper를 추가했습니다.
* placement와 selector metadata가 충돌하거나 여러 range/descriptor가 겹치면
  `kUnknown`으로 처리해 original-byte resume을 허용하지 않습니다.
* mode16 synthetic `66 85 FF`와 selector fallback을 general stack probe에
  추가했습니다. 기존 mode32 identical `89 C2` 허용 및 `PUSH ESP` 거부도
  유지되었습니다.
* post-HLE 설정이 꺼진 non-identical cache miss를 별도 trace stage로
  표시하고 dynamic resolver로 보냈습니다.

## 검증

Linux x64 Debug build:

```text
[100%] Built target repiu_core_probe
[100%] Built target repiu
```

Core probe:

```text
general_stack_push=...,legacy_resume_policy=true,legacy_resume_mode_aware=true,...
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

Object-3 runtime trace:

```text
[repiu-linexe-far-jump] ... resolved target=0x01100004
[repiu-hle-reentry] stage=cache-miss-non-identical ... detail=translate
[repiu-aot-plan-trace] guest=0x01100004 bytes=6685FF ... code_mode=16
[repiu-aot-dynamic] stage=image-entry guest=0x01100004 bytes=CC ...
[repiu-fault] unhandled signal=0x5 ... eip=0x01100004
```

이 trace는 `0x01100004`가 mode16 non-identical dynamic 경로로 들어갔음을
확인합니다. 이전처럼 원본 long mode에서 `B8 07 00`을 소비하여
`0x0110000E`로 이동하는 기록은 사라졌습니다. 현재 SIGTRAP은 dynamic image가
아직 mode16 `66 85 FF`를 지원하지 않아 INT3 fail-closed 경계에서 발생한
것이며, 이 작업에서 해결해야 할 재진입 오류와는 분리된 다음 lowering
frontier입니다.

## 결론 및 다음 작업

code-mode를 모르는 공용 re-entry gate 문제는 해결되었습니다. 남은 문제는
주소별 예외가 아닌 mode16 operand-width/control-flow lowering의 공용 범위
확장입니다. 다음 작업에서는 `66 85 FF`의 mode16 TEST, 이어지는 mode16 Jcc와
MOV AX immediate를 순차적으로 설계·검증합니다.

---

# Work Log 20260915-684 — Linux x64 code-mode-aware re-entry

## Summary

Task 683's mode16 original-byte fallback is now blocked by the shared re-entry
policy. `CanResumeLinuxX64LegacyTarget` resolves guest code mode from AOT
placement `RuntimeCodeModeRange` metadata, with executable selector descriptors
as a fallback, and passes that mode to the long-mode compatibility classifier.

On a cache miss, a non-identical instruction now attempts the dynamic AOT
resolver even when `REPIU_AOT_DBT_POST_HLE_TRANSLATE` is disabled. If the
resulting cache entry reaches an unsupported instruction, execution stops at
the existing fail-closed boundary instead of returning to original bytes.

## Implementation

* Added a shared mode resolver with placement-first and executable-selector
  fallback precedence.
* Conflicting or overlapping placement/selector metadata resolves to
  `kUnknown`, which does not permit original-byte resume.
* Added synthetic mode16 `66 85 FF` and selector-fallback coverage to the
  general stack probe; the existing mode32 `89 C2` admission and `PUSH ESP`
  rejection remain intact.
* Added a distinct trace stage for disabled-post-HLE non-identical cache misses
  routed to the dynamic resolver.

## Verification

Linux x64 Debug build:

```text
[100%] Built target repiu_core_probe
[100%] Built target repiu
```

Core probe:

```text
general_stack_push=...,legacy_resume_policy=true,legacy_resume_mode_aware=true,...
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

Object-3 runtime trace:

```text
[repiu-linexe-far-jump] ... resolved target=0x01100004
[repiu-hle-reentry] stage=cache-miss-non-identical ... detail=translate
[repiu-aot-plan-trace] guest=0x01100004 bytes=6685FF ... code_mode=16
[repiu-aot-dynamic] stage=image-entry guest=0x01100004 bytes=CC ...
[repiu-fault] unhandled signal=0x5 ... eip=0x01100004
```

The trace confirms that `0x01100004` enters the mode16 non-identical dynamic
path. The previous original-long-mode execution of `B8 07 00` and resulting
advance to `0x0110000E` no longer appears. The remaining SIGTRAP is the INT3
fail-closed boundary because the dynamic image does not yet support mode16
`66 85 FF`; it is the next lowering frontier, separate from the re-entry bug
fixed here.

## Conclusion and next task

The shared code-mode-unaware re-entry gate is fixed. The remaining work is a
shared expansion of mode16 operand-width/control-flow lowerings, not an
address-specific exception. The next task will design and verify mode16 TEST
for `66 85 FF`, followed by mode16 Jcc and MOV AX immediate lowerings.
