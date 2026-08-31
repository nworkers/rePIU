# 20260901-553 Linux x64 code cache long-mode 방출 작업 로그

설계: [20260831-553](../design/20260831-553-linux-x64-code-cache-long-mode-emission.md) ·
작업 지시: [20260831-553](../work-orders/20260831-553-linux-x64-code-cache-long-mode-emission.md)

## 한국어

### 결과

Task 550의 판정기와 Task 552의 lowering이 **code cache emitter에 연결됐습니다.** Task 546
구현 순서 3단계가 끝났습니다.

`AotCodeCacheBuildOptions::enable_long_mode_emission`이 켜지면 emitter는 `kCopy`마다
`ClassifyLongModeBytes`에 물어보고, 복사·lowering·거절 중 하나를 합니다. 기본값이
`false`이므로 i386 경로는 이 branch 자체를 지나가지 않습니다.

| 결과 | 방출 | 세기 |
|---|---|---|
| `kIdenticalBytes` | guest 바이트 그대로 | `long_mode_copied_count` |
| lowering 이름 있음 | `LowerLongModeBytes`의 바이트 | `long_mode_lowered_count` |
| 그 외 · lowering 실패 · `kCopy` 아님 | `0xCC` + `kHleBoundary` fixup | `long_mode_refused_count` |

### x64 전용 경계를 어디에 두었나

**build option입니다. `#ifdef`가 아닙니다.** 방출은 계산이라 실행이 없고, 그래서 같은
plan에 대한 답이 모든 호스트에서 같아야 합니다. `#ifdef`로 갈랐다면 그 답을 이 프로젝트가
실제로 시험 고리를 가진 호스트(Windows)에서 볼 수 없게 됩니다. 아래 측정이 그 선택의
근거입니다 — 세 호스트의 `long_mode_emission_*` 줄이 **글자 하나까지 같습니다.**

**그리고 option의 뜻은 "long mode 호스트를 위해 image를 만든다"입니다.** `kCopy`만
방출되고 나머지 kind는 전부 fail-closed입니다. `kCopy`만 낮추고 inline cache slot과
dispatch stub은 그대로 두는 쪽이 더 작은 변경이었지만, 그것은 판정기가 막으려던 실패를 한
층 위에서 다시 만드는 일입니다 — `68 imm32`는 long mode에서 8바이트를 밉니다.

### 이번에 측정이 뒤집은 것

설계에 "검증 디코드 모드를 바꾸지 않으면 이 항목이 실패한다"고 적었습니다. **틀렸습니다.**

되돌려서 재보니 probe가 그대로 통과했습니다. 이유는 검증 루프가 **길이 합계만** 보기
때문입니다. `67 8B 04 25 78 56 34 12`를 32비트 모드로 읽으면 3바이트 `mov`와 5바이트
`and` 두 명령이 되고, 합이 8바이트로 정확히 맞아 아무 말도 하지 않습니다.

이것은 판정기가 다루는 바로 그 위험 — **조용히 다른 명령** — 이 한 층 위 검증기에서
그대로 재현된 것입니다.

그래서 검증기를 고쳤습니다. long mode 방출에서는 map entry 하나가 정확히 명령
**한 개**입니다(복사 하나, lowering 하나, `0xCC` 하나). 길이 합계에 개수를 더해 확인하도록
바꾸고 나서 다시 재니, 모드를 되돌린 build가 `decode_failures=1`로 **실패했습니다.**

| 검증기 | 모드 되돌림 | 결과 |
|---|---|---|
| 길이 합계만 (기존) | 예 | `image_valid=true` — 놓침 |
| 길이 + 명령 개수 | 예 | `image_valid=false`, `decode_failures=1` — 잡음 |
| 길이 + 명령 개수 | 아니오 | `image_valid=true`, `decode_failures=0` |

> 바이트를 덮는 것과 의도한 대로 디코드되는 것은 다릅니다.

### 측정

| Host | 결과 |
|---|---|
| Win32 x86 Debug | `core_probe_all=true`, 18/18 |
| Linux x64 Release | `core_probe_all=true`, 18/18, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 18/18 |

세 호스트의 새 probe 출력입니다. 줄 단위로 동일합니다.

```text
long_mode_emission_default_builds=true
long_mode_emission_default_unchanged=true,counters_quiet=true
long_mode_emission_image_valid=true,decode_failures=0
  long_mode_emission_copied=true
  long_mode_emission_address_size_prefix=true
  long_mode_emission_absolute_to_sib=true
  long_mode_emission_refused_silent=true
  long_mode_emission_non_copy_boundary=true
long_mode_emission_counts=true,copied=1,lowered=2,refused=2
long_mode_emission_all_refused_builds=true
long_mode_emission_all=true
```

i386 회귀는 두 가지로 확인했습니다.

* option이 `false`이면 emitter의 판단 branch를 지나가지 않습니다. 세 호스트의 기존 probe가
  전부 그대로 통과합니다.
* Windows에서 emitter를 직접 쓰는 probe 넷을 돌렸습니다 — `--direct-return-table`,
  `--timer-safe-point`, `--jump-table-guard`, `--return-stage-profile` 모두 `_all=true`.
* `dos4gw_hello` 정적 AOT는 `direct control-flow target is outside the cache`로 실패하는데,
  이것은 [Task 434 설계](../design/20260806-434-github-actions-release-ci.md)가 기록한
  **기존 한계**이고 이번 변경과 무관합니다.

### 아직 아닌 것

**guest는 여전히 실행되지 않습니다.** Task 544의 fence는 그대로입니다. 이 단위는 x64용
바이트를 **만드는 것**까지이고, 그 바이트를 x64에서 실행하는 것은 다음입니다.

## English

### Result

Task 550's classifier and Task 552's lowering are **wired to the code cache emitter.**
Step 3 of Task 546's implementation order is done.

With `AotCodeCacheBuildOptions::enable_long_mode_emission` on, the emitter asks
`ClassifyLongModeBytes` about every `kCopy` and then copies, lowers, or refuses. The
default is `false`, so the i386 path does not enter that branch at all.

| Outcome | Emitted | Counted as |
|---|---|---|
| `kIdenticalBytes` | the guest's bytes unchanged | `long_mode_copied_count` |
| a named lowering | what `LowerLongModeBytes` produces | `long_mode_lowered_count` |
| anything else, a failed lowering, or not `kCopy` | `0xCC` + a `kHleBoundary` fixup | `long_mode_refused_count` |

### Where the x64-only boundary was put

**A build option, not an `#ifdef`.** Emission is computation and executes nothing, so the
answer for a given plan has to be the same on every host. An `#ifdef` would have made that
answer invisible on the host where this project's test loop actually runs. The measurement
below is the argument for the choice: the `long_mode_emission_*` lines from three hosts are
**identical character for character.**

**And the option means "build an image for a long-mode host".** Only `kCopy` is emitted;
every other kind is fail-closed. Lowering only the copies while leaving the inline cache
slots and dispatch stubs alone would have been the smaller change, and it would have
recreated one layer up the failure the classifier exists to prevent — `68 imm32` pushes
eight bytes in long mode.

### What the measurement overturned

The design said that without changing the verification decode's mode, this item would
fail. **That was wrong.**

Reverting it and measuring again, the probe still passed. The reason is that the
verification loop checks **total length only**. Read in 32-bit mode,
`67 8B 04 25 78 56 34 12` is a three-byte `mov` followed by a five-byte `and`; the eight
bytes are covered exactly and nothing is reported.

That is the classifier's own hazard — **quietly a different instruction** — reappearing one
layer up, inside the verifier.

So the verifier was fixed. Under long-mode emission a map entry is exactly **one**
instruction (one copy, one lowering, or one `0xCC`), so the count is now checked alongside
the coverage. Measured again with the mode reverted, that build **failed** with
`decode_failures=1`.

| Verifier | Mode reverted | Result |
|---|---|---|
| total length only (as it was) | yes | `image_valid=true` — missed it |
| length plus instruction count | yes | `image_valid=false`, `decode_failures=1` — caught it |
| length plus instruction count | no | `image_valid=true`, `decode_failures=0` |

> Covering the bytes is not the same as decoding them as intended.

### What was measured

| Host | Result |
|---|---|
| Win32 x86 Debug | `core_probe_all=true`, 18 of 18 |
| Linux x64 Release | `core_probe_all=true`, 18 of 18, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 18 of 18 |

The new probe's output on all three hosts, line for line identical:

```text
long_mode_emission_default_builds=true
long_mode_emission_default_unchanged=true,counters_quiet=true
long_mode_emission_image_valid=true,decode_failures=0
  long_mode_emission_copied=true
  long_mode_emission_address_size_prefix=true
  long_mode_emission_absolute_to_sib=true
  long_mode_emission_refused_silent=true
  long_mode_emission_non_copy_boundary=true
long_mode_emission_counts=true,copied=1,lowered=2,refused=2
long_mode_emission_all_refused_builds=true
long_mode_emission_all=true
```

The i386 regression rests on two things:

* With the option `false` the emitter never enters the judgement branch, and every existing
  probe on all three hosts still passes.
* Four Windows probes that drive the emitter directly were run — `--direct-return-table`,
  `--timer-safe-point`, `--jump-table-guard` and `--return-stage-profile`, all `_all=true`.
* `dos4gw_hello`'s static AOT still fails with `direct control-flow target is outside the
  cache`, which is the **pre-existing limitation** recorded in the
  [Task 434 design](../design/20260806-434-github-actions-release-ci.md) and is unrelated
  to this change.

### What this is not yet

**The guest still does not run.** Task 544's fence stands. This unit goes as far as
**producing** x64 bytes; executing them on x64 is next.
