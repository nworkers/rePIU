# 20260831-553 Linux x64 code cache long-mode 방출 작업 지시서

## 한국어

### 목적

Task 550의 판정기와 Task 552의 lowering을 code cache emitter의 `kCopy` 경로에
연결합니다. 설계는
[20260831-553](../design/20260831-553-linux-x64-code-cache-long-mode-emission.md)입니다.

### 작업

- `AotCodeCacheBuildOptions`에 `enable_long_mode_emission`을 추가한다. 기본값 `false`.
  호스트 매크로로 가르지 않는다 — 방출은 계산이므로 모든 호스트에서 같은 답이어야 하고,
  그 답이 Windows에서 관측 가능해야 한다.
- option이 켜지면 `kCopy`에서 `ClassifyLongModeBytes`를 호출한다.
  `kIdenticalBytes`는 그대로 복사, lowering이 이름 붙은 것은 `LowerLongModeBytes`,
  나머지는 fail-closed.
- option이 켜지면 `kCopy` 외의 모든 kind를 fail-closed로 보낸다. 32비트 slot을 long mode
  image에 넣지 않는다.
- option이 켜지면 timer safe point를 방출하지 않는다. 손으로 쓴 32비트 시퀀스이다.
- option이 켜지면 방출 후 검증 디코드를 `LONG_64` / `STACK_WIDTH_64`로 한다. 이것이 없으면
  옳은 lowering이 decode failure로 보고된다.
- image에 `long_mode_copied_count`, `long_mode_lowered_count`, `long_mode_refused_count`,
  `long_mode_emission_enabled`를 둔다.
- 모든 호스트에서 도는 probe를 추가해 core probe 공용 목록에 넣는다.

### 검증

- **Win32 x86 Debug 빌드와 `repiu_core_probe` 실행.** option 기본값이 `false`이므로 기존
  probe가 전부 그대로 통과해야 한다 — 이것이 i386 회귀 시험이다.
- **Linux x64 빌드와 `repiu_core_probe` 실행.** 새 probe를 포함해 전부 통과.
- 새 probe는 option off일 때 바이트가 기존 경로와 같은지, on일 때 복사·lowering·거절이
  각각 의도한 바이트를 내는지, 그리고 on으로 만든 image가 `valid`인지 확인한다.

## English

### Objective

Connect Task 550's classifier and Task 552's lowering to the code cache emitter's `kCopy`
path. The design is
[20260831-553](../design/20260831-553-linux-x64-code-cache-long-mode-emission.md).

### Work items

- Add `enable_long_mode_emission` to `AotCodeCacheBuildOptions`, defaulting to `false`.
  Do not split it on a host macro — emission is computation, so the answer must be the
  same on every host and must stay observable on Windows.
- With the option on, call `ClassifyLongModeBytes` from `kCopy`: copy `kIdenticalBytes`,
  run `LowerLongModeBytes` for anything with a named lowering, fail closed on the rest.
- With the option on, send every kind other than `kCopy` to the fail-closed path. No
  32-bit slot goes into a long-mode image.
- With the option on, emit no timer safe point. It is a hand-written 32-bit sequence.
- With the option on, run the post-emission verification decode in `LONG_64` /
  `STACK_WIDTH_64`. Without it a correct lowering reports as a decode failure.
- Put `long_mode_copied_count`, `long_mode_lowered_count`, `long_mode_refused_count` and
  `long_mode_emission_enabled` on the image.
- Add a probe that runs on every host and register it in the core probe's shared list.

### Verification

- **Build Win32 x86 Debug and run `repiu_core_probe`.** The option defaults to `false`, so
  every existing probe must still pass — that is the i386 regression test.
- **Build Linux x64 and run `repiu_core_probe`.** Everything passes, the new probe
  included.
- The new probe checks that the bytes with the option off match the existing path, that
  copy, lowering and refusal each produce the intended bytes with it on, and that an image
  built with it on is `valid`.
