# 작업 기록 20260902-570 — Linux x64 segment override base+disp8

설계: [20260902-570](../design/20260902-570-linux-x64-segment-base-disp8.md) ·
작업 지시: [20260902-570](../work-orders/20260902-570-linux-x64-segment-base-disp8.md)

## 구현

`LongModeSegmentOverrideEmittable`이 기존 absolute `disp32`와 비-SIB
`base+disp8`을 구분해 허용하도록 확장했습니다. 새 형식은 guest `disp8`을 부호 확장해
`original_displacement`에 저장하고 ModRM을 `mod=10`으로 바꾼 뒤 patch 가능한
`disp32`를 방출합니다. 다른 operand가 guest `ESP`를 가리키면 host `RSP` 오염을 막기
위해 거절합니다.

Linux x64 probe는 다음 두 access를 한 image에 넣고 engine과 같은 runtime patcher로
두 site를 함께 patch했습니다.

```text
26 8a 4f ff              mov cl, es:[edi-1]
26 8b 1d 40 00 00 00     mov ebx, es:[0x40]
```

일치 경로에서 `CL=0x5a`, `EBX=0xfeedface`, guest ESP 보존을 확인했습니다. 새 slot을
첫 번째로 두었으므로 불일치 경로의 INT3 1회와 두 destination 미변경은 새 guard가
access 전에 닫힌다는 직접 증거입니다. HLE routing으로 두 slot의 첫 바이트를 닫은 뒤
native로 재해결해 두 access가 다시 실행되는 것도 확인했습니다.

## census

`roms/pumpipx3/PIU/PIU.EXE`:

```text
segment overrides    7
emittable             72675 / 74333 (97.77%)
refused               1658
complete blocks       14736 / 17367 (84.85%)
emitter agrees        true
reachable serviced    18
reachable blocks      28
reachable instrs      76
first stop            0x10fc2d5
first stop bytes      1f
```

Task 569의 18 block에서 28 block으로 늘었습니다. 다음 장벽은 plain `POP DS`인
`kGuardedSegmentPop`입니다.

## 검증

- Linux x64 Release `repiu_core_probe`: 20/20, skipped 2,
  `guest_segment_override=true`, `linux_x64_guest_register_all=true`.
- Linux x64 Release `repiu_instruction_census`: image build 성공,
  `agrees=true`.
- Linux i386 Release `repiu_core_probe`: 19/19, skipped 3.
- Win32 x86 Debug `repiu_core_probe`: 19/19, skipped 3.
- Win32 x86 Debug `repiu_aot_probe roms\\pumpipx3\\PIU\\PIU.EXE`: exit 0,
  `guarded_segment_load_*` 및 `selector_guard_all=true`.
- `git diff --check`: 오류 없음. 저장소의 기존 CRLF 변환 경고만 출력됐습니다.

---

# Work log 20260902-570 — Linux x64 segment-override base+disp8

Design: [20260902-570](../design/20260902-570-linux-x64-segment-base-disp8.md) ·
work order: [20260902-570](../work-orders/20260902-570-linux-x64-segment-base-disp8.md)

## Implementation

`LongModeSegmentOverrideEmittable` now distinguishes the existing absolute
disp32 form from a non-SIB base-plus-disp8 form. The new form sign-extends the
guest disp8 into `original_displacement`, forces ModRM to `mod=10`, and emits a
patchable disp32. An instruction naming guest `ESP` in another operand is
refused so it cannot corrupt host `RSP`.

The Linux x64 probe puts these two accesses in one image and patches both sites
with the same runtime patcher used by the engine:

```text
26 8a 4f ff              mov cl, es:[edi-1]
26 8b 1d 40 00 00 00     mov ebx, es:[0x40]
```

The matching path observed `CL=0x5a`, `EBX=0xfeedface`, and preserved guest
ESP. Because the new slot is first, the mismatch path's one INT3 and unchanged
destinations directly prove its guard closes before the access. After HLE
routing closed both slot heads, native re-resolution restored and executed
both accesses again.

## Census

For `roms/pumpipx3/PIU/PIU.EXE`:

```text
segment overrides    7
emittable             72675 / 74333 (97.77%)
refused               1658
complete blocks       14736 / 17367 (84.85%)
emitter agrees        true
reachable serviced    18
reachable blocks      28
reachable instrs      76
first stop            0x10fc2d5
first stop bytes      1f
```

Reachability increased from Task 569's 18 blocks to 28. The next barrier is a
plain `POP DS`, classified as `kGuardedSegmentPop`.

## Verification

- Linux x64 Release `repiu_core_probe`: 20/20, 2 skipped,
  `guest_segment_override=true`, `linux_x64_guest_register_all=true`.
- Linux x64 Release `repiu_instruction_census`: image built and `agrees=true`.
- Linux i386 Release `repiu_core_probe`: 19/19, 3 skipped.
- Win32 x86 Debug `repiu_core_probe`: 19/19, 3 skipped.
- Win32 x86 Debug `repiu_aot_probe roms\\pumpipx3\\PIU\\PIU.EXE`: exit 0,
  all `guarded_segment_load_*` checks and `selector_guard_all=true`.
- `git diff --check`: no errors; only the repository's existing CRLF
  conversion warnings were printed.
