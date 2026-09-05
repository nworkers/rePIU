# 작업 지시 20260905-603 — Linux x64 혼합 모드 far return 경계

## 목표

object 3의 혼합 모드 `66 CB`가 generic near `RET` AOT 슬롯으로 잘못 연결되는
것을 분리하고, 반환 프레임 ABI를 추정하지 않은 fail-closed 경계를 확보합니다.

## 범위

1. `AotInstructionKind`에 `kFarReturn`을 추가합니다.
2. AOT planner에서 Zydis far-return metadata를 near return과 분리하고
   `far_return_count`를 기록합니다.
3. long-mode emitter와 기본 emitter가 `kFarReturn`을 generic near-return
   inline-cache/thunk로 내보내지 않고 boundary로 남기게 합니다.
4. long-mode emission probe에서 `66 CB` boundary와 통계를 검증합니다.
5. Linux x64 빌드, core probe, probe-success runtime을 수행하고 분석 문서와
   작업 로그를 갱신합니다.

## 제외 범위

* `INT 31h AX=1E7Fh` private ABI 추정 또는 구현
* `0x000000FF`를 유효 target으로 취급하는 보정
* selector를 무시한 far return 구현
* object 3의 D-bit/B-bit 의미를 생략한 고정 `ESP += 6` 패치

## 구현 계획

* `include/repiu/runtime/aot_translation_plan.h`의 명령 종류와 통계를 갱신합니다.
* `src/runtime/aot_translation_plan.cpp`에서 `ZYDIS_BRANCH_TYPE_FAR` return을
  별도 기록합니다.
* `src/runtime/aot_code_cache.cpp`에서 새 종류를 fail-closed boundary로
  내보냅니다.
* `src/tools/aot_probe/long_mode_emission_probe.cpp`에 합성 `66 CB` 사례를
  추가합니다.

## 검증 절차

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
build/linux_x64_debug/repiu_core_probe
```

probe-success runtime은 기존과 같은 다음 환경을 사용합니다.

```text
REPIU_DOS_INT_TRACE=1
REPIU_DPMI_1E7F_TRACE=1
REPIU_DPMI_1E7F_PROBE_SUCCESS=1
REPIU_LINUX_X64_RETURN_TRACE=1
```

## 완료 기준

* `66 CB`가 plan에서 `kFarReturn`으로 기록됩니다.
* long-mode cache에 해당 위치의 generic return thunk가 생기지 않고 `CC`가
  생깁니다.
* 기존 near `C3` return probe가 계속 통과합니다.
* Linux x64 core probe가 모두 통과합니다.
* runtime의 다음 frontier가 `0x01100040` far-return boundary로 확인됩니다.

---

# Work order 20260905-603 — Linux x64 mixed-mode far-return boundary

## Goal

Separate object 3's mixed-mode `66 CB` from the generic near-`RET` AOT slot and
obtain a fail-closed boundary without guessing the return-frame ABI.

## Scope

1. Add `kFarReturn` to `AotInstructionKind`.
2. Separate Zydis far-return metadata from near returns in the AOT planner and
   record `far_return_count`.
3. Keep `kFarReturn` out of the generic near-return inline-cache/thunk in both
   the long-mode and default emitters, leaving a boundary instead.
4. Add a synthetic `66 CB` case to the long-mode emission probe.
5. Build the Linux x64 targets, run the core probe and probe-success runtime, and
   update the analysis and work log.

## Out of scope

* Guessing or implementing the private `INT 31h AX=1E7Fh` ABI
* Treating `0x000000FF` as a valid target
* Implementing far return while ignoring the selector
* A fixed `ESP += 6` patch that omits object D-bit/B-bit semantics

## Implementation plan

* Update the instruction kind and statistics in
  `include/repiu/runtime/aot_translation_plan.h`.
* Record `ZYDIS_BRANCH_TYPE_FAR` returns separately in
  `src/runtime/aot_translation_plan.cpp`.
* Emit the new kind as a fail-closed boundary in `src/runtime/aot_code_cache.cpp`.
* Add the synthetic `66 CB` assertion in
  `src/tools/aot_probe/long_mode_emission_probe.cpp`.

## Verification

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
build/linux_x64_debug/repiu_core_probe
```

The probe-success runtime uses the existing environment:

```text
REPIU_DOS_INT_TRACE=1
REPIU_DPMI_1E7F_TRACE=1
REPIU_DPMI_1E7F_PROBE_SUCCESS=1
REPIU_LINUX_X64_RETURN_TRACE=1
```

## Done criteria

* `66 CB` is recorded as `kFarReturn` in the plan.
* The long-mode cache contains `CC`, not a generic return thunk, at that site.
* Existing near `C3` return probes still pass.
* All Linux x64 core probes pass.
* The next runtime frontier is confirmed as the far-return boundary at
  `0x01100040`.
