# 작업 지시 20260901-567 — x64 segment override slot

설계: [20260901-567-x64-segment-override.md](../design/20260901-567-x64-segment-override.md)

## 범위

1. `EmitLongModeSegmentOverride`를 `aot_code_cache.cpp`에 추가한다. ES/SS/DS만
   받고 FS/GS는 거부한다. ModRM `mod=00 rm=101`, disp32, 기본 opcode map만
   받는다.
2. slot은 `pushfd` → shadow selector 비교 → 불일치면 `popfd` 후 boundary,
   일치면 `popfd` 후 SIB 절대주소 접근 → fallthrough로 `E9`.
3. `AotSegmentOverrideSite`에 displacement/guard 위치를 기록한다.
4. `enable_long_mode_segment_override`를 추가하되 **기본 false**로 둔다.
5. `linux_x64_guest_register_probe.cpp`에 `ProbeSegmentOverride`를 추가한다.
   selector 일치와 불일치를 **모두** 실행한다.
6. census에 "segment override를 patch했다면 얼마나 열리는가"를 별도 줄로
   보고한다.

## 하지 않는 것

- 런타임 patcher. 이번 단위는 slot과 그 검증까지다.
- FS/GS. long mode에서 의미가 다르므로 별도 단위다.

## 검증

- Linux x64 Release `repiu_core_probe` 전부 통과.
- Linux i386 Release, Win32 회귀 통과.
- census 재측정.

---

# Work order 20260901-567 — x64 segment override slot

Design: [20260901-567-x64-segment-override.md](../design/20260901-567-x64-segment-override.md)

## Scope

1. Add `EmitLongModeSegmentOverride` to `aot_code_cache.cpp`. Admit ES/SS/DS
   only; refuse FS/GS. Admit only ModRM `mod=00 rm=101`, disp32, default
   opcode map.
2. The slot: `pushfd`, compare the shadow selector, `popfd` and take the
   boundary on a mismatch, `popfd` and perform the SIB-absolute access on a
   match, then `E9` to the fallthrough.
3. Record displacement and guard offsets in `AotSegmentOverrideSite`.
4. Add `enable_long_mode_segment_override`, defaulting to **false**.
5. Add `ProbeSegmentOverride` to `linux_x64_guest_register_probe.cpp`, running
   **both** the matching and the mismatching selector.
6. Report, as its own census line, how much a patched segment override would
   unlock.

## Not in scope

- The runtime patcher. This unit ends at the slot and its verification.
- FS/GS, which mean something different in long mode and are their own unit.

## Verification

- All of Linux x64 Release `repiu_core_probe` passes.
- Linux i386 Release and Win32 regressions pass.
- Re-measure with the census.
