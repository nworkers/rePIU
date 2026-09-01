# 작업 기록 20260902-569 — Linux x64 guarded segment load

## 구현

`MOV ES,AX`를 long mode에서 그대로 실행하지 않았다. guest selector를 host
segment register에 설치하지 않는 기존 결정을 지키면서, `AX`와 shadow `ES`가 같을
때만 의미상 no-op으로 통과하는 slot을 추가했다. 비교가 flags를 바꾸므로 Task 559의
`PUSHFD`/`POPFD` lowering으로 둘러쌌고, 불일치는 flags를 복원한 뒤 INT3로 간다.

guarded-load patch loop는 `src/runtime/aot_segment_patch.cpp`로 옮겼다. i386 site는
기존 success/fallback counter operand를 유지하고 x64 site는 shadow 주소만 patch한다.
두 emitter 모두 자기 slot에서 실제 첫 5바이트를 떠서 site에 기록한다. unresolved
상태가 첫 바이트를 `CC`로 닫은 뒤에도 native 재해석이 host별 prologue를 정확히
복원한다.

## 실제 x64 실행

Linux x64 probe에서 emitted slot을 하위 4 GiB code cache에 놓고 실행했다.

```text
segment_load_patcher_sites      1
segment_load_patcher_native     1
segment_load_unresolved         1
segment_load_unresolved_trap    0xcc
segment_load_restored           1
segment_load_marker             0x12345678
segment_load_source             0x1c
segment_load_flags              0xffffff00
segment_load_esp                0x20001800
segment_load_boundary           1
segment_load_no_marker          0
segment_load_mismatch_esp       0x20001800
guest_segment_load              true
```

일치 경로는 marker까지 갔고 source `EAX`, ZF를 관측한 `EDX`, guest ESP가 기대값과
같았다. 불일치 경로는 marker 전에 INT3를 한 번 냈고 guest ESP가 균형을 유지했다.

## census

`roms/pumpipx3/PIU/PIU.EXE`:

```text
guarded seg loads   55
reachable serviced  16
reachable blocks    18
reachable instrs    39
agrees               true
first stop           0x10fc27d
first stop bytes     26 8a 4f ff
```

Task 568의 11 block에서 18 block으로 늘었다. 다음 장벽은 같은 segment override
계열이지만 absolute disp32가 아니라 `mov cl, es:[edi-1]`의 base+disp8 주소 형식이다.

## 검증

- Linux x64 Release `repiu_core_probe`: 20/20, skipped 2,
  `linux_x64_guest_register_all=true`.
- Linux x64 Release `repiu_instruction_census`: `agrees=true`, image build 성공.
- Linux i386 Release `repiu_core_probe`: 19/19, skipped 3.
- Win32 x86 Debug `repiu_core_probe`: 19/19, skipped 3.
- Win32 x86 Debug `repiu_aot_probe`: `guarded_segment_load_*` 및
  `selector_guard_all=true`, 전체 probe 종료 성공.
- `git diff --check`: 오류 없음. 저장소의 기존 CRLF 변환 경고만 출력됨.

---

# Work log 20260902-569 — Linux x64 guarded segment load

## Implementation

`MOV ES,AX` is not executed directly in long mode. Preserving the existing
decision not to install guest selectors into host segment registers, the new
slot passes only when `AX` equals shadow `ES`, making the instruction a
semantic no-op. The comparison is surrounded by Task 559's `PUSHFD`/`POPFD`
lowering because it changes flags; a mismatch restores flags and reaches INT3.

The guarded-load patch loop moved to `src/runtime/aot_segment_patch.cpp`. i386
sites retain their success/fallback counter operands, while x64 sites patch
only the shadow address. Both emitters snapshot the actual first five bytes of
their own slots into the site. A native re-resolution therefore restores the
right host-specific prologue after an unresolved state closes the first byte
with `CC`.

## Actual x64 execution

The Linux x64 probe placed and ran the emitted slot in a below-4-GiB code
cache.

```text
segment_load_patcher_sites      1
segment_load_patcher_native     1
segment_load_unresolved         1
segment_load_unresolved_trap    0xcc
segment_load_restored           1
segment_load_marker             0x12345678
segment_load_source             0x1c
segment_load_flags              0xffffff00
segment_load_esp                0x20001800
segment_load_boundary           1
segment_load_no_marker          0
segment_load_mismatch_esp       0x20001800
guest_segment_load              true
```

The matching path reached the marker and preserved source `EAX`, the ZF result
observed through `EDX`, and guest ESP. The mismatching path raised one INT3
before the marker and kept guest ESP balanced.

## Census

For `roms/pumpipx3/PIU/PIU.EXE`:

```text
guarded seg loads   55
reachable serviced  16
reachable blocks    18
reachable instrs    39
agrees               true
first stop           0x10fc27d
first stop bytes     26 8a 4f ff
```

Reachability increased from Task 568's 11 blocks to 18. The next wall is in
the same segment-override family, but it is the base-plus-disp8 address form of
`mov cl, es:[edi-1]`, not the absolute disp32 form.

## Verification

- Linux x64 Release `repiu_core_probe`: 20/20, 2 skipped,
  `linux_x64_guest_register_all=true`.
- Linux x64 Release `repiu_instruction_census`: `agrees=true`; image built.
- Linux i386 Release `repiu_core_probe`: 19/19, 3 skipped.
- Win32 x86 Debug `repiu_core_probe`: 19/19, 3 skipped.
- Win32 x86 Debug `repiu_aot_probe`: all `guarded_segment_load_*` checks and
  `selector_guard_all=true`; the full probe completed.
- `git diff --check`: no errors; only the repository's existing CRLF conversion
  warnings were printed.
